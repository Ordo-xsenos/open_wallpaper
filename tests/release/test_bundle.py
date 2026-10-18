"""Synthetic file fixtures test packaging policy, never executable compatibility."""

from pathlib import Path
import importlib.util
import json
import struct
import zipfile

import pytest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "release_bundle", ROOT / "tools/Release-Bundle.py"
)
bundle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bundle)


def put(root, name, data=b"fixture"):
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.suffix.lower() in (".exe", ".dll"):
        data = b"MZ" + bytes(58) + struct.pack("<I", 64) + b"PE\0\0\x64\x86"
    path.write_bytes(data)
    return path


@pytest.fixture
def payload(tmp_path):
    root = tmp_path / "payload"
    for name in (
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
        "qml/QtQuick/qmldir",
        "licenses/Microsoft/terms.txt",
        "licenses/Qt/qtbase/LICENSES/LGPL-3.0-only.txt",
        "licenses/Qt/qtbase/LICENSES/GPL-3.0-only.txt",
    ):
        put(root, name)
    for package in ("nlohmann-json", "libarchive", "libwebp", "zlib", "sqlite3"):
        put(root, "licenses/" + package + "/copyright")
    put(
        root,
        "licenses/Qt/SOURCES.json",
        json.dumps(
            {
                "qtVersion": bundle.METADATA["qtVersion"],
                "modules": ["synthetic test only"],
            }
        ).encode(),
    )
    for i in range(6):
        put(root, f"scenes/bundled/{i}/manifest.json", b"{}")
    return root


def test_seal_zip_and_detect_changed_or_extra_files(payload, tmp_path):
    bundle.seal(payload, "a" * 40)
    bundle.verify(payload)
    output = tmp_path / "release.zip"
    bundle.archive(payload, output)
    with zipfile.ZipFile(output) as archive:
        assert archive.testzip() is None
        assert "AetherWall/release-manifest.json" in archive.namelist()
    assert (
        output.with_suffix(".zip.sha256").read_text().startswith(bundle.digest(output))
    )
    (payload / "LICENSE").write_text("changed")
    with pytest.raises(ValueError, match="differs"):
        bundle.verify(payload)
    bundle.seal(payload, "unversioned-source")
    put(payload, "unexpected.txt")
    with pytest.raises(ValueError, match="differs"):
        bundle.verify(payload)


@pytest.mark.parametrize(
    "name",
    [
        "tls/qschannelbackend.dll",
        "platforms/qwindows.dll",
        "licenses/libarchive/copyright",
        "licenses/Qt/SOURCES.json",
        "licenses/Qt/qtbase/LICENSES/LGPL-3.0-only.txt",
        "licenses/Qt/qtbase/LICENSES/GPL-3.0-only.txt",
        "msvcp140.dll",
    ],
)
def test_missing_dependency_fails_closed(payload, name):
    (payload / name).unlink()
    with pytest.raises(ValueError):
        bundle.seal(payload, "a" * 40)


@pytest.mark.parametrize(
    "name",
    [
        "Qt6Cored.dll",
        "vcruntime140d.dll",
        "settings.json",
        ".env",
        "library.sqlite3",
        "core.pdb",
    ],
)
def test_debug_or_private_files_never_ship(payload, name):
    put(payload, name)
    with pytest.raises(ValueError):
        bundle.seal(payload, "a" * 40)


def test_rejects_wrong_architecture_and_linked_files(payload):
    path = payload / "AetherWall.Core.exe"
    data = bytearray(path.read_bytes())
    data[-2:] = b"\x4c\x01"  # i386
    path.write_bytes(data)
    with pytest.raises(ValueError, match="AMD64"):
        bundle.seal(payload, "a" * 40)
    put(payload, "AetherWall.Core.exe")
    (payload / "linked").symlink_to(payload / "LICENSE")
    with pytest.raises(ValueError, match="Linked"):
        bundle.seal(payload, "a" * 40)


def test_wrong_qt_and_false_acceptance_metadata_are_rejected(payload):
    bundle.seal(payload, "a" * 40)
    path = payload / bundle.MANIFEST
    manifest = json.loads(path.read_text())
    manifest["acceptance"] = "PASS"
    path.write_text(json.dumps(manifest))
    with pytest.raises(ValueError):
        bundle.verify(payload)
    put(payload, "licenses/Qt/SOURCES.json", b'{"qtVersion":"wrong","modules":[1]}')
    with pytest.raises(ValueError):
        bundle.seal(payload, "a" * 40)
