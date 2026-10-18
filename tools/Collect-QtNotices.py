"""Collect notices from exact Qt source checkouts; never fetch or infer licenses."""

from pathlib import Path
import argparse
import json
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
VERSION = json.loads((ROOT / "packaging/release.json").read_text())["qtVersion"]
MODULES = ("qtbase", "qtdeclarative", "qtsvg", "qtshadertools", "qttranslations")


def read_attributions(path):
    # This reader is only for the pinned, origin/tag/cleanliness-checked Qt sources.
    # Some attribution strings contain literal control characters (e.g. line breaks).
    # Permit those inside strings without changing the copied source bytes or the
    # strict parsers used for application settings, scenes and catalog responses.
    try:
        entries = json.loads(path.read_bytes().decode("utf-8-sig"), strict=False)
    except json.JSONDecodeError as error:
        raise ValueError(
            f"Invalid Qt attribution {path}:{error.lineno}:{error.colno}: {error.msg}"
        ) from error
    if isinstance(entries, dict):
        entries = [entries]
    if not isinstance(entries, list) or any(
        not isinstance(entry, dict) for entry in entries
    ):
        raise ValueError(f"Expected attribution object or object array: {path}")
    return entries


def collect(source, output):
    if output.exists():
        raise ValueError("Use a fresh notices destination")
    gathered, modules = [], []
    for module in MODULES:
        root = (source / module).resolve()
        repository = f"https://code.qt.io/qt/{module}.git"
        origin = subprocess.check_output(
            ["git", "-C", str(root), "remote", "get-url", "origin"], text=True
        ).strip()
        if origin != repository:
            raise ValueError(f"Unexpected Qt source origin: {module}")
        tag = subprocess.check_output(
            ["git", "-C", str(root), "describe", "--exact-match", "--tags", "HEAD"],
            text=True,
        ).strip()
        if tag != "v" + VERSION:
            raise ValueError(f"{module}: expected exact v{VERSION} source tag")
        dirty = subprocess.check_output(
            ["git", "-C", str(root), "status", "--porcelain"], text=True
        )
        if dirty.strip():
            raise ValueError("Qt source checkout has local modifications")
        selected = set()
        for path in root.rglob("*"):
            if ".git" in path.parts or not path.is_file():
                continue
            if path.is_symlink():
                continue
            upper = path.name.upper()
            if (
                # Qt uses SPDX names such as LGPL-3.0-only.txt inside LICENSES.
                # Copy that entire tree, including names without a license prefix.
                path.is_relative_to(root / "LICENSES")
                or upper.startswith(
                    ("LICENSE", "LICENCE", "COPYING", "NOTICE", "COPYRIGHT")
                )
                or path.name == "qt_attribution.json"
            ):
                selected.add(path)
            if path.name == "qt_attribution.json":
                for entry in read_attributions(path):
                    license_files = entry.get("LicenseFile", [])
                    if isinstance(license_files, str):
                        license_files = [license_files]
                    for value in license_files:
                        license_path = (path.parent / value).resolve()
                        if (
                            not license_path.is_relative_to(root)
                            or not license_path.is_file()
                        ):
                            raise ValueError(
                                f"Missing/outside attributed license: {path}"
                            )
                        selected.add(license_path)
        if not (root / "LICENSES").is_dir():
            raise ValueError(f"Missing Qt license terms: {module}/LICENSES")
        if module == "qtbase":
            for name in ("LGPL-3.0-only.txt", "GPL-3.0-only.txt"):
                path = root / "LICENSES" / name
                if path not in selected or not path.stat().st_size:
                    raise ValueError(
                        f"Missing Qt license terms: {module}/LICENSES/{name}"
                    )
        gathered += [(p, output / module / p.relative_to(root)) for p in selected]
        modules.append(
            {
                "module": module,
                "tag": tag,
                "repository": f"https://code.qt.io/qt/{module}.git",
                "commit": subprocess.check_output(
                    ["git", "-C", str(root), "rev-parse", "HEAD"], text=True
                ).strip(),
            }
        )
    output.mkdir(parents=True)
    for path, destination in gathered:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, destination)
    (output / "SOURCES.json").write_text(
        json.dumps(
            {
                "qtVersion": VERSION,
                "modules": modules,
                "scope": "All collected module notices; exact deployed third-party audit and source distribution still required",
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sources", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    collect(args.sources, args.output)
