"""Validate/seal an x64 Release payload or create its portable ZIP. No downloads."""

from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
METADATA = json.loads((ROOT / "packaging/release.json").read_text())
MANIFEST = "release-manifest.json"


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def files(root):
    if (
        root.is_symlink()
        or not root.is_dir()
        or getattr(root.lstat(), "st_file_attributes", 0) & 0x400
    ):
        raise ValueError("Payload must be an ordinary directory")
    found = []
    for directory, dirs, names in os.walk(root, followlinks=False):
        for name in dirs + names:
            path = Path(directory) / name
            info = path.lstat()
            if path.is_symlink() or getattr(info, "st_file_attributes", 0) & 0x400:
                raise ValueError("Linked/reparse-point payload entry")
        for name in names:
            path = Path(directory) / name
            if not path.is_file():
                raise ValueError("Non-file payload entry")
            if name.lower() in (
                ".env",
                "settings.json",
                "settings.json.bak",
                "library.sqlite3",
            ) or name.lower().endswith((".pdb", ".log")):
                raise ValueError("Private/development data in release payload")
            found.append(path)
    return sorted(found)


def check_pe(path):
    with path.open("rb") as stream:
        if stream.read(2) != b"MZ":
            raise ValueError(f"Not a PE image: {path.name}")
        stream.seek(60)
        header = stream.read(4)
        if len(header) != 4:
            raise ValueError("Truncated PE image")
        offset = struct.unpack("<I", header)[0]
        if offset < 64 or offset > path.stat().st_size - 6:
            raise ValueError("Invalid PE header offset")
        stream.seek(offset)
        if stream.read(6) != b"PE\0\0\x64\x86":
            raise ValueError(f"Expected native AMD64 image: {path.name}")


def validate(root):
    payload = files(root)
    for required in (
        "AetherWall.Core.exe",
        "AetherWall.Manager.exe",
        "AetherWall.SceneCheck.exe",
        "qt.conf",
        "Qt6Core.dll",
        "Qt6Gui.dll",
        "Qt6Quick.dll",
        "Qt6Qml.dll",
        "Qt6Network.dll",
        "platforms/qwindows.dll",
        "tls/qschannelbackend.dll",
        "msvcp140.dll",
        "vcruntime140.dll",
        "LICENSE",
        "THIRD_PARTY_NOTICES.md",
        "PREVIEW_NOTES.md",
        "DEPENDENCY_SOURCES.md",
        "licenses/Qt/SOURCES.json",
        "licenses/Qt/qtbase/LICENSES/LGPL-3.0-only.txt",
        "licenses/Qt/qtbase/LICENSES/GPL-3.0-only.txt",
    ):
        if not (root / required).is_file() or not (root / required).stat().st_size:
            raise ValueError(f"Missing release dependency/notice: {required}")
    for directory in ("qml", "licenses/Qt", "licenses/Microsoft"):
        if not (root / directory).is_dir() or not any((root / directory).rglob("*")):
            raise ValueError(f"Missing runtime/license directory: {directory}")
    for package in ("nlohmann-json", "libarchive", "libwebp", "zlib", "sqlite3"):
        if not (root / "licenses" / package / "copyright").is_file():
            raise ValueError(f"Missing dependency notice: {package}")
    manifests = list((root / "scenes/bundled").glob("*/manifest.json"))
    if len(manifests) != 6:
        raise ValueError("Expected all six original bundled scenes")
    sources = json.loads((root / "licenses/Qt/SOURCES.json").read_text())
    if sources.get("qtVersion") != METADATA["qtVersion"] or not sources.get("modules"):
        raise ValueError("Qt source notices must match the pinned runtime")
    for path in payload:
        lower = path.name.lower()
        if (
            lower.startswith("qt6")
            and lower.endswith("d.dll")
            and not lower.endswith("3d.dll")
        ):
            raise ValueError("Debug Qt runtime in Release payload")
        if re.match(r"(?:msvcp|vcruntime|ucrtbase).*d\.dll$", lower):
            raise ValueError("Debug CRT in Release payload")
        if path.suffix.lower() in (".exe", ".dll"):
            check_pe(path)
    return payload


def seal(root, revision):
    if not re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}|unversioned-source", revision):
        raise ValueError("Use a source commit SHA or explicit unversioned-source")
    payload = validate(root)
    manifest = dict(
        METADATA,
        platform="windows",
        buildType="Release",
        sourceRevision=revision,
        files=[
            {
                "path": p.relative_to(root).as_posix(),
                "size": p.stat().st_size,
                "sha256": digest(p),
            }
            for p in payload
            if p.relative_to(root).as_posix() != MANIFEST
        ],
    )
    (root / MANIFEST).write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


def verify(root):
    payload = validate(root)
    manifest = json.loads((root / MANIFEST).read_text())
    expected = {
        p.relative_to(root).as_posix(): (p.stat().st_size, digest(p))
        for p in payload
        if p.relative_to(root).as_posix() != MANIFEST
    }
    entries = manifest["files"]
    actual = {p["path"]: (p["size"], p["sha256"]) for p in entries}
    if (
        len(actual) != len(entries)
        or actual != expected
        or any(manifest.get(k) != v for k, v in METADATA.items())
        or manifest["buildType"] != "Release"
    ):
        raise ValueError("Payload differs from release manifest; refusing packaging")


def archive(root, output):
    verify(root)
    if output.resolve().is_relative_to(root.resolve()):
        raise ValueError("Archive output must be outside payload")
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as target:
        for path in files(root):
            entry = zipfile.ZipInfo(
                "AetherWall/" + path.relative_to(root).as_posix(), (2026, 1, 1, 0, 0, 0)
            )
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            target.writestr(entry, path.read_bytes())
    output.with_suffix(output.suffix + ".sha256").write_text(
        digest(output) + "  " + output.name + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("seal", "verify", "zip"))
    parser.add_argument("--payload", type=Path, required=True)
    parser.add_argument("--revision", default="unversioned-source")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.command == "seal":
        seal(args.payload, args.revision)
    elif args.command == "verify":
        verify(args.payload)
    else:
        if args.output is None:
            parser.error("zip requires --output")
        archive(args.payload, args.output)
    print(f"Release payload {args.command}: OK")
