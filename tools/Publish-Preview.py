"""Verify preview assets and publish a unique GitHub pre-release using GH_TOKEN."""

from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
METADATA = json.loads((ROOT / "packaging/release.json").read_text())


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def release_assets(directory):
    version, qt = METADATA["version"], METADATA["qtVersion"]
    names = [
        f"AetherWall-{version}-win-x64-setup.exe",
        f"AetherWall-{version}-win-x64.zip",
        "AetherWall-M5.zip",
    ]
    checksummed = list(names)
    names += [name + ".sha256" for name in checksummed]
    names += [
        f"{module}-{qt}-source.zip"
        for module in (
            "qtbase",
            "qtdeclarative",
            "qtsvg",
            "qtshadertools",
            "qttranslations",
        )
    ]
    found = {}
    for path in directory.rglob("*"):
        if path.name not in names:
            continue
        if (
            path.name in found
            or path.is_symlink()
            or not path.is_file()
            or not path.stat().st_size
        ):
            raise ValueError(f"Duplicate, empty or unsafe release asset: {path.name}")
        found[path.name] = path
    for name in names:
        if name not in found:
            raise ValueError(f"Missing release asset: {name}")
        if name.endswith(".zip"):
            with zipfile.ZipFile(found[name]) as archive:
                if not archive.namelist() or archive.testzip() is not None:
                    raise ValueError(f"Invalid ZIP asset: {name}")
    for name in checksummed:
        fields = found[name + ".sha256"].read_text(encoding="utf-8-sig").split()
        if fields != [sha256(found[name]), name]:
            raise ValueError(f"Release checksum mismatch: {name}")
    return [found[name] for name in names]


def publish(directory, environment=None):
    env = os.environ if environment is None else environment
    if env.get("GITHUB_REF") != "refs/heads/main" or env.get(
        "GITHUB_EVENT_NAME"
    ) not in ("push", "workflow_dispatch"):
        raise ValueError("Only main push/manual builds may publish")
    repository, revision = env.get("GITHUB_REPOSITORY", ""), env.get("GITHUB_SHA", "")
    run, attempt = env.get("GITHUB_RUN_ID", ""), env.get("GITHUB_RUN_ATTEMPT", "")
    if not re.fullmatch(
        r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository
    ) or not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("Missing/invalid GitHub repository or source SHA")
    if not run.isdigit() or not attempt.isdigit():
        raise ValueError("Missing GitHub run identity")
    assets = release_assets(directory)
    tag = f"v{METADATA['version']}-build.{run}.{attempt}"
    version = METADATA["version"]
    notes = f"""# AetherWall {version} — Windows x64 preview

Автоматическая предварительная сборка. Для установки скачайте
`AetherWall-{version}-win-x64-setup.exe` из **Assets** ниже.
Для запуска без установки распакуйте `AetherWall-{version}-win-x64.zip` целиком
и откройте `AetherWall.Manager.exe`. Windows 11 24H2+ x64, Direct3D 11.

Исходники приложения: `AetherWall-M5.zip`; соответствующие исходники Qt приложены
отдельными `qt…-source.zip`. Все файлы проверяются по `SHA256SUMS.txt`.

Коммит: `{revision}`.
[Лог сборки](https://github.com/{repository}/actions/runs/{run}).

---

{(ROOT / "docs/PREVIEW_NOTES.md").read_text(encoding="utf-8")}
"""
    with tempfile.TemporaryDirectory(prefix="aetherwall-release-") as temporary:
        root = Path(temporary)
        sums, body = root / "SHA256SUMS.txt", root / "release-notes.md"
        sums.write_text(
            "".join(f"{sha256(p)}  {p.name}\n" for p in assets), encoding="utf-8"
        )
        body.write_text(notes, encoding="utf-8")
        # Build the release privately first. A failed upload leaves a draft,
        # never a published release with an incomplete set of downloads.
        subprocess.run(
            [
                "gh",
                "release",
                "create",
                tag,
                "--repo",
                repository,
                "--target",
                revision,
                "--title",
                f"AetherWall {version} · build {run}.{attempt}",
                "--prerelease",
                "--latest=false",
                "--draft",
                "--notes-file",
                str(body),
                *map(str, assets),
                str(sums),
            ],
            check=True,
        )
        subprocess.run(
            [
                "gh",
                "release",
                "edit",
                tag,
                "--repo",
                repository,
                "--draft=false",
                "--prerelease",
                "--latest=false",
            ],
            check=True,
        )
    url = f"https://github.com/{repository}/releases/tag/{tag}"
    if env.get("GITHUB_STEP_SUMMARY"):
        with Path(env["GITHUB_STEP_SUMMARY"]).open("a", encoding="utf-8") as summary:
            summary.write(f"## Windows preview\n\n[Скачать установщик и ZIP]({url})\n")
    print(url)
    return url


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", required=True, type=Path)
    publish(parser.parse_args().assets)
