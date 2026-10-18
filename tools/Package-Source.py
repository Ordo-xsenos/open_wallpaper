"""Create a source-only shareable ZIP from an explicit repository allowlist."""
from pathlib import Path
import argparse
import hashlib
import zipfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, default=root / "out/share/AetherWall-M5.zip")
args = parser.parse_args()
files = [root / name for name in (
    "CMakeLists.txt", "CMakePresets.json", "vcpkg.json", "README.md", "AGENTS.md",
    "WINDOWS11_WALLPAPER_ENGINE_SPEC.md", "LICENSE", "THIRD_PARTY_NOTICES.md",
    "PRODUCT.md", "DESIGN.md", ".clang-format", ".gitignore", ".dockerignore",
    ".impeccable/config.json", ".impeccable/design.json",
)]
for name in (".github", "apps", "src", "shaders", "scenes", "tests", "tools", "docs", "server", "packaging"):
    files += [p for p in (root / name).rglob("*") if p.is_file() and not any(part in ("__pycache__", ".pytest_cache", ".venv", ".env", "build", "dist") or part.endswith(".egg-info") for part in p.relative_to(root).parts)]
files = sorted(set(files))
for path in files:
    if not path.is_file() or path.is_symlink():
        raise SystemExit(f"Missing or linked source file: {path}")
args.output.parent.mkdir(parents=True, exist_ok=True)
sums = []
with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for path in files:
        name = "AetherWall/" + path.relative_to(root).as_posix()
        data = path.read_bytes()
        info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o100644 << 16
        archive.writestr(info, data)
        sums.append(hashlib.sha256(data).hexdigest() + "  " + path.relative_to(root).as_posix())
    archive.writestr("AetherWall/SOURCE_SHA256SUMS", "\n".join(sums) + "\n")
with zipfile.ZipFile(args.output) as archive:
    if archive.testzip() is not None:
        raise SystemExit("ZIP validation failed")
digest = hashlib.sha256(args.output.read_bytes()).hexdigest()
args.output.with_suffix(args.output.suffix + ".sha256").write_text(digest + "  " + args.output.name + "\n", encoding="utf-8")
print(f"{args.output}: {len(files)} source files + hash manifest, {args.output.stat().st_size} bytes")
