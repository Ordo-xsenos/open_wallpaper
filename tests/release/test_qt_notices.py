"""Qt source attribution compatibility without downloading the Qt source trees."""

from pathlib import Path
import importlib.util
import json

import pytest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "collect_qt_notices", ROOT / "tools/Collect-QtNotices.py"
)
notices = importlib.util.module_from_spec(spec)
spec.loader.exec_module(notices)


@pytest.mark.parametrize("control", ["\n", "\r", "\t"])
def test_accepts_literal_controls_in_attribution_strings(tmp_path, control):
    path = tmp_path / "qt_attribution.json"
    raw = (
        '{"Copyright":"First author'
        + control
        + 'Second author","LicenseFile":"LICENSE"}'
    )
    path.write_text(raw, encoding="utf-8")
    with pytest.raises(json.JSONDecodeError, match="Invalid control character"):
        json.loads(raw)
    assert notices.read_attributions(path)[0]["Copyright"] == (
        "First author" + control + "Second author"
    )


def test_accepts_utf8_bom_and_escaped_strings(tmp_path):
    path = tmp_path / "qt_attribution.json"
    path.write_text(
        json.dumps([{"Copyright": 'Nazar\n"Author"', "LicenseFile": ["LICENSE"]}]),
        encoding="utf-8-sig",
    )
    assert notices.read_attributions(path)[0]["Copyright"] == 'Nazar\n"Author"'


@pytest.mark.parametrize("raw", ['{"LicenseFile":"LICENSE",}', "[42]", "null"])
def test_does_not_silently_skip_broken_metadata(tmp_path, raw):
    path = tmp_path / "qt_attribution.json"
    path.write_text(raw, encoding="utf-8")
    with pytest.raises(ValueError, match="qt_attribution.json"):
        notices.read_attributions(path)


@pytest.fixture
def qt_sources(tmp_path, monkeypatch):
    root = tmp_path / "sources/qtbase"
    licenses = root / "LICENSES"
    licenses.mkdir(parents=True)
    (licenses / "LGPL-3.0-only.txt").write_text("Synthetic license fixture")
    vendor = root / "src/3rdparty/vendor"
    vendor.mkdir(parents=True)
    (vendor / "LICENSE.vendor").write_text("Synthetic vendor license")
    metadata = vendor / "qt_attribution.json"
    metadata.write_bytes(
        b'{"Copyright":"Author one\nAuthor two", "LicenseFile":"LICENSE.vendor"}'
    )
    monkeypatch.setattr(notices, "MODULES", ("qtbase",))

    def git_output(command, **kwargs):
        # Only Git identity queries are stubbed; parsing, bounds and copying are real.
        if command[3:] == ["remote", "get-url", "origin"]:
            return "https://code.qt.io/qt/qtbase.git\n"
        if command[3:] == ["describe", "--exact-match", "--tags", "HEAD"]:
            return "v" + notices.VERSION + "\n"
        if command[3:] == ["status", "--porcelain"]:
            return ""
        if command[3:] == ["rev-parse", "HEAD"]:
            return "a" * 40 + "\n"
        raise AssertionError(command)

    monkeypatch.setattr(notices.subprocess, "check_output", git_output)
    return root.parent, metadata, tmp_path / "collected"


def test_collect_preserves_original_metadata_and_license_bytes(qt_sources):
    source, metadata, output = qt_sources
    notices.collect(source, output)
    copied = output / "qtbase/src/3rdparty/vendor"
    assert (copied / "qt_attribution.json").read_bytes() == metadata.read_bytes()
    assert (copied / "LICENSE.vendor").read_bytes() == (
        metadata.parent / "LICENSE.vendor"
    ).read_bytes()
    manifest = json.loads((output / "SOURCES.json").read_text(encoding="utf-8"))
    assert manifest["qtVersion"] == notices.VERSION
    assert manifest["modules"][0]["tag"] == "v" + notices.VERSION


@pytest.mark.parametrize("license_file", ["missing.txt", "../../../../outside.txt"])
def test_license_path_checks_remain_strict(qt_sources, license_file):
    source, metadata, output = qt_sources
    (source / "outside.txt").write_text("Outside fixture")
    metadata.write_text(json.dumps({"LicenseFile": license_file}), encoding="utf-8")
    with pytest.raises(ValueError, match="Missing/outside attributed license"):
        notices.collect(source, output)
    assert not output.exists()
