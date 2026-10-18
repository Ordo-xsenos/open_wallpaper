"""Build deterministic mock catalog packages from the six original bundled scenes."""

from pathlib import Path
import hashlib
import io
import json
import subprocess
import zipfile
import argparse

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument(
    "--validator",
    type=Path,
    required=True,
    help="Built AetherWall.SceneCheck executable",
)
args = parser.parse_args()
out = root / "server/mock"
items = []
categories = {
    "stars": "space",
    "gradient": "abstract",
    "minimal_particles": "minimal",
    "rain": "nature",
    "mountains": "nature",
    "still": "nature",
}
for folder in sorted((root / "scenes/bundled").iterdir()):
    if not folder.is_dir():
        continue
    subprocess.run(
        [str(args.validator.resolve()), str(folder)], check=True, capture_output=True
    )
    manifest = json.loads((folder / "manifest.json").read_text())
    buffer = io.BytesIO()
    with zipfile.ZipFile(
        buffer, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9
    ) as archive:
        for path in sorted(folder.rglob("*")):
            if path.is_file():
                if path.is_symlink():
                    raise ValueError("Linked bundled asset")
                info = zipfile.ZipInfo(
                    path.relative_to(folder).as_posix(), (2026, 1, 1, 0, 0, 0)
                )
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                archive.writestr(info, path.read_bytes())
    package = buffer.getvalue()
    sha = hashlib.sha256(package).hexdigest()
    prefix = f"assets/{manifest['id']}/{sha}"
    target = out / prefix
    target.mkdir(parents=True, exist_ok=True)
    (target / "package.awscene").write_bytes(package)
    preview = (folder / manifest["preview"]).read_bytes()
    (target / "preview.png").write_bytes(preview)
    proof = (
        f"{manifest['title']}\nAuthor: {manifest['author']}\nLicense: CC0-1.0\n"
        f"Original AetherWall bundled artwork, source scenes/bundled/{folder.name}.\n"
        "Procedural artwork and manifests are supplied with the AetherWall source.\n"
        "See scenes/README.md and tools/Generate-BundledScenes.py.\n"
    )
    (target / "license.txt").write_text(proof, encoding="utf-8")
    items.append(
        {
            "id": manifest["id"],
            "title": manifest["title"],
            "description": f"Original bundled example: {manifest['title']}. Static reference preview; animation runs in Core.",
            "author": manifest["author"],
            "license": "CC0-1.0",
            "licenseProofUrl": prefix + "/license.txt",
            "tags": manifest["tags"],
            "category": categories[folder.name],
            "version": "1.0.0",
            "createdAt": "2026-10-18T00:00:00Z",
            "updatedAt": "2026-10-18T00:00:00Z",
            "previewUrl": prefix + "/preview.png",
            "previewSha256": hashlib.sha256(preview).hexdigest(),
            "previewSize": len(preview),
            "packageUrl": prefix + "/package.awscene",
            "sha256": sha,
            "packageSize": len(package),
            "supportedSchemaVersions": [1],
            "rating": None,
            "downloadCount": None,
        }
    )
(out / "catalog.json").write_text(
    json.dumps({"schemaVersion": 1, "demo": True, "items": items}, indent=2) + "\n",
    encoding="utf-8",
)
print(f"Built {len(items)} validated original catalog examples in {out}")
