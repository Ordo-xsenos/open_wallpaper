from datetime import datetime, timezone
from pathlib import Path
import argparse
import hashlib
import re

from .config import Config
from .main import create_app
from .validation import validate_package


def main():
    parser = argparse.ArgumentParser(
        description="Administrator-only catalog validation/publication; no anonymous upload API"
    )
    commands = parser.add_subparsers(dest="command", required=True)
    publish = commands.add_parser("publish")
    publish.add_argument("package", type=Path)
    publish.add_argument("--validator", type=Path, required=True)
    publish.add_argument("--description", required=True)
    publish.add_argument("--category", required=True)
    publish.add_argument("--rights-proof", type=Path, required=True)
    publish.add_argument("--version", default="1.0.0")
    publish.add_argument(
        "--approve",
        action="store_true",
        help="Explicit moderator approval; otherwise create an unpublished draft",
    )
    moderation = commands.add_parser("moderate")
    moderation.add_argument("scene_id")
    moderation.add_argument("--approve", action="store_true")
    args = parser.parse_args()
    app = create_app(Config.from_env())
    if args.command == "moderate":
        from uuid import UUID

        scene_id = str(UUID(args.scene_id))
        # Only validator-created drafts exist; no unauthenticated upload route.
        with app.state.database.connect() as connection:
            found = app.state.database.execute(
                connection, "SELECT id FROM wallpapers WHERE id=?", (scene_id,)
            ).fetchone()
        if not found:
            parser.error("Unknown draft")
        app.state.database.set_published(scene_id, args.approve)
        return
    if (
        not re.fullmatch(r"[a-z0-9-]{1,32}", args.category)
        or not 1 <= len(args.description) <= 2000
        or not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", args.version)
    ):
        parser.error("Invalid category, description or semantic version")
    if args.rights_proof.stat().st_size > 16384:
        parser.error("Rights proof exceeds limit")
    proof = args.rights_proof.read_bytes()
    if not proof.strip():
        parser.error("Author permission/license source is required")
    proof.decode("utf-8")
    checked = validate_package(args.package, args.validator)
    scene, sha = checked["manifest"], checked["sha256"]
    prefix = f"assets/{checked['id']}/{sha}"
    now = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
    item = {
        "id": checked["id"],
        "title": scene["title"],
        "description": args.description,
        "author": scene["author"],
        "license": scene["license"],
        "licenseProofUrl": prefix + "/license.txt",
        "tags": scene.get("tags", []),
        "category": args.category,
        "version": args.version,
        "createdAt": now,
        "updatedAt": now,
        "previewUrl": prefix + "/preview.png",
        "previewSha256": hashlib.sha256(checked["preview"]).hexdigest(),
        "previewSize": len(checked["preview"]),
        "packageUrl": prefix + "/package.awscene",
        "sha256": sha,
        "packageSize": len(checked["data"]),
        "supportedSchemaVersions": [1],
        "rating": None,
        "downloadCount": None,
    }
    for name, content, mime in (
        ("package.awscene", checked["data"], "application/octet-stream"),
        ("preview.png", checked["preview"], "image/png"),
        ("license.txt", proof, "text/plain"),
    ):
        app.state.storage.put(prefix + "/" + name, content, mime)
    app.state.database.upsert(item, args.approve)
    print(
        f"{item['id']} {'published' if args.approve else 'draft; moderator approval required'} {sha}"
    )


if __name__ == "__main__":
    main()
