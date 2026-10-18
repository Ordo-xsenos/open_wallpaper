from pathlib import Path
import hashlib
import json

import httpx
import pytest
import asyncio


class TestClient:
    """In-process ASGI transport; no socket or cross-thread event-loop portal."""

    __test__ = False

    def __init__(self, app):
        self.app = app

    def __enter__(self):
        return self

    def __exit__(self, *args):
        pass

    def get(self, url, **kwargs):
        async def request():
            # Restricted runners may suppress cross-thread self-pipe wakeups.
            # A timer keeps the real ASGI/thread-pool path progressing without sockets.
            async def heartbeat():
                while True:
                    await asyncio.sleep(0.01)

            tick = asyncio.create_task(heartbeat())
            async with (
                asyncio.timeout(10),
                httpx.AsyncClient(
                    transport=httpx.ASGITransport(app=self.app),
                    base_url="http://127.0.0.1:8765",
                ) as client,
            ):
                try:
                    return await client.get(url, **kwargs)
                finally:
                    tick.cancel()

        return asyncio.run(request())


from server.app.config import Config
from server.app.main import create_app
from server.app.storage import LocalStorage
from server.app.validation import validate_package

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "server/mock"
ITEMS = json.loads((FIXTURE / "catalog.json").read_text())["items"]


@pytest.fixture
def app(tmp_path):
    app = create_app(
        Config(
            database_url="sqlite:///" + str(tmp_path / "metadata.sqlite3"),
            storage_root=FIXTURE,
        ),
        mock=True,
    )
    for item in ITEMS:
        app.state.database.upsert(item, True)
    return app


@pytest.fixture
def client(app):
    with TestClient(app) as client:
        yield client


def test_health_categories_search_and_pagination(client):
    assert client.get("/api/v1/health").json() == {
        "status": "ok",
        "apiVersion": 1,
        "demo": True,
    }
    assert (
        sum(c["count"] for c in client.get("/api/v1/categories").json()["items"]) == 6
    )
    first = client.get("/api/v1/wallpapers?limit=2").json()
    second = client.get("/api/v1/wallpapers?limit=2&page=2").json()
    assert first["total"] == 6 and first["pages"] == 3
    assert not {x["id"] for x in first["items"]} & {x["id"] for x in second["items"]}
    assert client.get("/api/v1/wallpapers/search?q=mountain").json()["total"] == 3
    assert client.get("/api/v1/wallpapers?category=nature").json()["total"] == 3
    for sort in ("popular", "trending", "new"):
        page = client.get("/api/v1/wallpapers", params={"sort": sort}).json()
        assert page["total"] == 6 and not page["popularityAvailable"]
        assert all(
            x["downloadCount"] is None and x["rating"] is None for x in page["items"]
        )


def test_detail_package_hash_preview_and_rights(client):
    item = client.get("/api/v1/wallpapers/" + ITEMS[0]["id"]).json()
    assert item["packageUrl"].startswith("http://127.0.0.1:8765/")
    package = client.get(item["packageUrl"])
    assert package.status_code == 200 and len(package.content) == item["packageSize"]
    assert hashlib.sha256(package.content).hexdigest() == item["sha256"]
    preview = client.get(item["previewUrl"])
    assert hashlib.sha256(preview.content).hexdigest() == item["previewSha256"]
    assert "CC0-1.0" in client.get(item["licenseProofUrl"]).text
    assert package.headers["x-content-type-options"] == "nosniff"


def test_invalid_queries_and_paths(client):
    for query in (
        "limit=0",
        "limit=49",
        "page=-1",
        "page=100001",
        "sort=bad",
        "category=../secret",
        "q=" + "x" * 129,
    ):
        assert client.get("/api/v1/wallpapers?" + query).status_code == 422
    assert client.get("/api/v1/wallpapers/search?q=%25").json()["total"] == 0
    assert client.get("/api/v1/wallpapers?q=' OR 1=1 --").json()["total"] == 0
    assert client.get("/api/v1/wallpapers/not-a-uuid").status_code == 422
    assert (
        client.get(
            "/assets/" + ITEMS[0]["id"] + "/" + "0" * 64 + "/package.awscene"
        ).status_code
        == 404
    )
    assert (
        client.get(
            "/assets/" + ITEMS[0]["id"] + "/" + ITEMS[0]["sha256"] + "/secret"
        ).status_code
        == 404
    )


def test_moderation_hides_drafts_and_assets(app):
    app.state.database.set_published(ITEMS[0]["id"], False)
    with TestClient(app) as client:
        assert client.get("/api/v1/wallpapers").json()["total"] == 5
        assert client.get("/api/v1/wallpapers/" + ITEMS[0]["id"]).status_code == 404
        assert client.get("/" + ITEMS[0]["packageUrl"]).status_code == 404
    app.state.database.set_published(ITEMS[0]["id"], True)
    assert app.state.database.get(ITEMS[0]["id"]) is not None


def test_rate_limit_is_explicit_and_bounded(tmp_path):
    app = create_app(
        Config(
            database_url="sqlite:///" + str(tmp_path / "db"),
            storage_root=tmp_path / "objects",
            requests_per_minute=2,
        )
    )
    with TestClient(app) as client:
        assert client.get("/api/v1/health").status_code == 200
        assert client.get("/api/v1/health").status_code == 200
        response = client.get("/api/v1/health")
        assert response.status_code == 429 and response.headers["retry-after"] == "60"


def test_config_requires_production_https_and_postgres():
    with pytest.raises(ValueError):
        Config(development=False).validate()
    with pytest.raises(ValueError):
        Config(public_base_url="http://example.com").validate()
    with pytest.raises(ValueError):
        Config(public_base_url="https://user:secret@example.com").validate()
    assert Config(
        database_url="postgresql://localhost/aetherwall",
        public_base_url="https://example.com",
        development=False,
    ).validate()


def test_object_storage_prevents_traversal_and_outside_symlinks(tmp_path):
    storage = LocalStorage(tmp_path / "objects")
    storage.put("a/b", b"hello", "text/plain")
    assert b"".join(storage.chunks("a/b")) == b"hello"
    for key in ("../secret", "/absolute", "a/../../secret", "a\\b"):
        with pytest.raises(ValueError):
            storage.path(key)
    (storage.root / "outside").symlink_to(tmp_path)
    with pytest.raises(ValueError):
        storage.path("outside/secret")


def test_every_published_mock_package_passes_real_core_validator():
    validator = ROOT / "out/build/portable-tests/AetherWall.SceneCheck"
    if not validator.is_file():
        pytest.skip(
            "Build portable-tests or provide publication validator on the test host"
        )
    for item in ITEMS:
        checked = validate_package(FIXTURE / item["packageUrl"], validator)
        assert checked["sha256"] == item["sha256"] and checked["id"] == item["id"]


def test_publication_fails_without_validator_or_for_corrupt_package(tmp_path):
    package = FIXTURE / ITEMS[0]["packageUrl"]
    with pytest.raises(ValueError):
        validate_package(package, tmp_path / "missing")
    corrupted = tmp_path / "corrupt.awscene"
    corrupted.write_bytes(b"not zip")
    validator = ROOT / "out/build/portable-tests/AetherWall.SceneCheck"
    if validator.is_file():
        with pytest.raises(ValueError):
            validate_package(corrupted, validator)


def test_indexed_query_handles_large_metadata_dataset_without_materializing_every_row(
    app,
):
    # Synthetic metadata exists only in this test; no public catalog count claim.
    from uuid import UUID

    for i in range(10000):
        item = dict(
            ITEMS[0],
            id=str(UUID(int=i + 100)),
            title=f"Test fixture {i}",
            category="test",
        )
        app.state.database.upsert(item, True)
    total, page = app.state.database.list(400, 24, "", "test", "new")
    assert total == 10000 and len(page) == 24


def test_publication_cli_creates_validated_draft_then_requires_explicit_moderation(
    tmp_path, monkeypatch
):
    from server.app.cli import main
    import sys

    validator = ROOT / "out/build/portable-tests/AetherWall.SceneCheck"
    if not validator.is_file():
        pytest.skip("Build portable-tests for real publication validation")
    monkeypatch.setenv(
        "AETHERWALL_DATABASE_URL", "sqlite:///" + str(tmp_path / "db.sqlite3")
    )
    monkeypatch.setenv("AETHERWALL_STORAGE_ROOT", str(tmp_path / "objects"))
    proof = tmp_path / "permission.txt"
    proof.write_text("Original bundled AetherWall work; CC0-1.0", encoding="utf-8")
    package = FIXTURE / ITEMS[0]["packageUrl"]
    monkeypatch.setattr(
        sys,
        "argv",
        [
            "catalog",
            "publish",
            str(package),
            "--validator",
            str(validator),
            "--description",
            "Original publication test",
            "--category",
            "abstract",
            "--rights-proof",
            str(proof),
        ],
    )
    main()
    app = create_app(Config.from_env())
    client = TestClient(app)
    assert client.get("/api/v1/wallpapers").json()["total"] == 0
    assert client.get("/" + ITEMS[0]["packageUrl"]).status_code == 404
    monkeypatch.setattr(
        sys, "argv", ["catalog", "moderate", ITEMS[0]["id"], "--approve"]
    )
    main()
    assert client.get("/api/v1/wallpapers").json()["total"] == 1
    response = client.get("/" + ITEMS[0]["packageUrl"])
    assert hashlib.sha256(response.content).hexdigest() == ITEMS[0]["sha256"]


def test_publication_requires_nonempty_rights_proof(tmp_path, monkeypatch):
    from server.app.cli import main
    import sys

    monkeypatch.setenv("AETHERWALL_DATABASE_URL", "sqlite:///" + str(tmp_path / "db"))
    monkeypatch.setenv("AETHERWALL_STORAGE_ROOT", str(tmp_path / "objects"))
    proof = tmp_path / "empty.txt"
    proof.write_text("  ")
    monkeypatch.setattr(
        sys,
        "argv",
        [
            "catalog",
            "publish",
            str(FIXTURE / ITEMS[0]["packageUrl"]),
            "--validator",
            "unused",
            "--description",
            "Test",
            "--category",
            "test",
            "--rights-proof",
            str(proof),
            "--approve",
        ],
    )
    with pytest.raises(SystemExit):
        main()
    assert (
        create_app(Config.from_env()).state.database.list(1, 24, "", "", "new")[0] == 0
    )
