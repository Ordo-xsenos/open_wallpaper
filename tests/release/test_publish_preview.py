"""Release publication uses a fake gh process; tests never contact GitHub."""

from pathlib import Path
import importlib.util
import subprocess
import zipfile

import pytest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "publish_preview", ROOT / "tools/Publish-Preview.py"
)
preview = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preview)


@pytest.fixture
def assets(tmp_path):
    directory = tmp_path / "assets"
    directory.mkdir()
    version, qt = preview.METADATA["version"], preview.METADATA["qtVersion"]
    for name in (
        f"AetherWall-{version}-win-x64-setup.exe",
        f"AetherWall-{version}-win-x64.zip",
        "AetherWall-M5.zip",
    ):
        path = directory / name
        if name.endswith(".zip"):
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr(
                    "fixture.txt", "Synthetic publication test, not executable"
                )
        else:
            path.write_bytes(b"Synthetic installer fixture, not executable")
        path.with_name(name + ".sha256").write_text(
            preview.sha256(path) + "  " + name + "\n"
        )
    for module in (
        "qtbase",
        "qtdeclarative",
        "qtsvg",
        "qtshadertools",
        "qttranslations",
    ):
        with zipfile.ZipFile(directory / f"{module}-{qt}-source.zip", "w") as archive:
            archive.writestr("fixture.txt", "Synthetic source fixture")
    return directory


@pytest.fixture
def environment(tmp_path):
    return {
        "GITHUB_REF": "refs/heads/main",
        "GITHUB_EVENT_NAME": "push",
        "GITHUB_REPOSITORY": "example/wallpaper",
        "GITHUB_SHA": "a" * 40,
        "GITHUB_RUN_ID": "1234",
        "GITHUB_RUN_ATTEMPT": "2",
        "GITHUB_STEP_SUMMARY": str(tmp_path / "summary.md"),
    }


def test_publishes_only_after_draft_upload_with_all_companion_sources(
    assets, environment, monkeypatch
):
    commands = []

    def run(command, **kwargs):
        assert kwargs["check"]
        commands.append(command)
        if command[2] == "create":
            assert "--draft" in command and "--target" in command
            body = Path(command[command.index("--notes-file") + 1]).read_text()
            assert environment["GITHUB_SHA"] in body
            sums = Path(command[-1]).read_text().splitlines()
            assert len(sums) == 11
            assert all(len(line.split()[0]) == 64 for line in sums)
            assert (
                len([value for value in command if value.endswith("-source.zip")]) == 5
            )

    monkeypatch.setattr(preview.subprocess, "run", run)
    url = preview.publish(assets, environment)
    assert len(commands) == 2
    assert commands[0][3] == "v" + preview.METADATA["version"] + "-build.1234.2"
    assert commands[1][2] == "edit" and "--draft=false" in commands[1]
    assert all(
        "--prerelease" in command and "--latest=false" in command
        for command in commands
    )
    assert url in Path(environment["GITHUB_STEP_SUMMARY"]).read_text()


@pytest.mark.parametrize("change", ["missing", "corrupt", "duplicate", "bad-zip"])
def test_rejects_incomplete_or_changed_downloads(
    assets, environment, monkeypatch, change
):
    path = assets / "AetherWall-M5.zip"
    if change == "missing":
        path.unlink()
    elif change == "corrupt":
        path.with_name(path.name + ".sha256").write_text("0" * 64 + "  " + path.name)
    elif change == "duplicate":
        duplicate = assets / "other" / path.name
        duplicate.parent.mkdir()
        duplicate.write_bytes(path.read_bytes())
    else:
        path.write_bytes(b"not zip")
    monkeypatch.setattr(
        preview.subprocess,
        "run",
        lambda *a, **kw: pytest.fail("Must not contact GitHub"),
    )
    with pytest.raises((ValueError, zipfile.BadZipFile)):
        preview.publish(assets, environment)


@pytest.mark.parametrize(
    "event,ref",
    [("pull_request", "refs/heads/main"), ("workflow_dispatch", "refs/heads/feature")],
)
def test_never_publishes_pull_requests_or_non_main_branches(
    assets, environment, event, ref
):
    environment.update(GITHUB_EVENT_NAME=event, GITHUB_REF=ref)
    with pytest.raises(ValueError, match="Only main"):
        preview.publish(assets, environment)


def test_failed_asset_upload_never_publishes(assets, environment, monkeypatch):
    commands = []

    def run(command, **kwargs):
        commands.append(command)
        raise subprocess.CalledProcessError(1, command)

    monkeypatch.setattr(preview.subprocess, "run", run)
    with pytest.raises(subprocess.CalledProcessError):
        preview.publish(assets, environment)
    assert len(commands) == 1 and "--draft" in commands[0]
    assert not Path(environment["GITHUB_STEP_SUMMARY"]).exists()
