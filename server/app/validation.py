from pathlib import Path, PurePosixPath
from uuid import UUID
import hashlib
import io
import json
import stat
import subprocess
import tempfile
import zipfile

PACKAGE_LIMIT = 32 * 1024 * 1024
EXPANDED_LIMIT = 64 * 1024 * 1024


def validate_package(path: Path, validator: Path):
    if (
        path.is_symlink()
        or not path.is_file()
        or not 0 < path.stat().st_size <= PACKAGE_LIMIT
    ):
        raise ValueError("Package size/type rejected")
    with path.open("rb") as stream:
        data = stream.read(PACKAGE_LIMIT + 1)
    if not 0 < len(data) <= PACKAGE_LIMIT:
        raise ValueError("Package changed or exceeds size limit")
    if not validator.is_file():
        raise ValueError("Built AetherWall.SceneCheck is required before publication")
    # Validate the exact immutable bytes later uploaded, without shell execution.
    with tempfile.TemporaryDirectory(prefix="aetherwall-publish-") as temporary:
        snapshot = Path(temporary) / "package.awscene"
        snapshot.write_bytes(data)
        result = subprocess.run(
            [str(validator.resolve()), str(snapshot)], capture_output=True, timeout=15
        )
        if result.returncode:
            raise ValueError(
                "Core validator rejected package: "
                + result.stderr[:1024].decode("utf-8", "replace")
            )
    names, total = set(), 0
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        if not 0 < len(archive.infolist()) <= 256:
            raise ValueError("Too many ZIP entries")
        for info in archive.infolist():
            name = info.filename
            parts = PurePosixPath(name)
            if (
                parts.is_absolute()
                or "\\" in name
                or any(p in (".", "..", "") for p in name.rstrip("/").split("/"))
            ):
                raise ValueError("Unsafe ZIP path")
            folded = name.casefold()
            if (
                folded in names
                or info.flag_bits & 1
                or stat.S_ISLNK(info.external_attr >> 16)
            ):
                raise ValueError("Duplicate/encrypted/linked ZIP entry")
            names.add(folded)
            if info.is_dir():
                continue
            total += info.file_size
            if (
                info.file_size > PACKAGE_LIMIT
                or total > EXPANDED_LIMIT
                or total > len(data) * 200 + 1024 * 1024
            ):
                raise ValueError("ZIP expansion limit exceeded")
        manifest = json.loads(archive.read("manifest.json"))
        scene_id = str(UUID(manifest["id"]))
        preview = archive.read(manifest["preview"])
        if (
            not preview.startswith(b"\x89PNG\r\n\x1a\n")
            or len(preview) > 2 * 1024 * 1024
        ):
            raise ValueError("Catalog publication requires a PNG preview up to 2 MiB")
        width, height = (
            int.from_bytes(preview[16:20], "big"),
            int.from_bytes(preview[20:24], "big"),
        )
        if not 0 < width <= 2048 or not 0 < height <= 2048:
            raise ValueError("Preview dimensions exceed catalog profile")
    return {
        "id": scene_id,
        "manifest": manifest,
        "data": data,
        "preview": preview,
        "sha256": hashlib.sha256(data).hexdigest(),
    }
