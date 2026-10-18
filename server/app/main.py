from collections import OrderedDict, deque
from typing import Literal
from uuid import UUID
from threading import Lock
import copy
import math
import re
import time
from urllib.parse import urljoin

from fastapi import FastAPI, HTTPException, Query, Request
from fastapi.responses import StreamingResponse, JSONResponse

from .config import Config
from .database import Database
from .storage import LocalStorage, S3Storage


class RateLimiter:
    def __init__(self, limit: int):
        self.limit, self.clients, self.lock = limit, OrderedDict(), Lock()

    def allowed(self, client: str):
        now = time.monotonic()
        with self.lock:
            while self.clients and next(iter(self.clients.values()))[-1] < now - 60:
                self.clients.popitem(last=False)
            bucket = self.clients.pop(client, deque())
            while bucket and bucket[0] <= now - 60:
                bucket.popleft()
            # Bound memory without permitting new identities to evict active limits.
            if len(self.clients) >= 4096:
                return False
            allowed = len(bucket) < self.limit
            if allowed:
                bucket.append(now)
            if bucket:
                self.clients[client] = bucket
            return allowed


def create_app(config: Config | None = None, *, mock=False):
    config = (config or Config.from_env()).validate()
    database = Database(config.database_url)
    storage = (
        LocalStorage(config.storage_root)
        if config.storage_backend == "local"
        else S3Storage(config.s3_bucket, config.s3_endpoint)
    )
    limiter = RateLimiter(config.requests_per_minute)
    app = FastAPI(
        title="AetherWall Catalog",
        version="1.0.0",
        docs_url="/docs" if config.development else None,
    )
    app.state.database, app.state.storage, app.state.config = database, storage, config

    def public(item):
        result = copy.deepcopy(item)
        origin = (config.asset_base_url or config.public_base_url).rstrip("/") + "/"
        for key in ("previewUrl", "packageUrl", "licenseProofUrl"):
            result[key] = urljoin(origin, result[key])
        return result

    @app.middleware("http")
    async def limits(request: Request, call_next):
        # Do not trust X-Forwarded-For from arbitrary clients; configure the trusted proxy separately.
        client = request.client.host if request.client else "unknown"
        if not limiter.allowed(client):
            return JSONResponse(
                {"detail": "Request limit exceeded"},
                status_code=429,
                headers={"Retry-After": "60"},
            )
        response = await call_next(request)
        response.headers["X-Content-Type-Options"] = "nosniff"
        response.headers["Referrer-Policy"] = "no-referrer"
        return response

    @app.get("/api/v1/health")
    def health():
        with database.connect() as connection:
            database.execute(connection, "SELECT 1")
        return {"status": "ok", "apiVersion": 1, "demo": mock}

    @app.get("/api/v1/categories")
    def categories():
        return {"items": database.categories()}

    def page_result(page, limit, q, category, sort):
        total, items = database.list(page, limit, q.strip(), category, sort)
        return {
            "schemaVersion": 1,
            "page": page,
            "limit": limit,
            "total": total,
            "pages": max(1, math.ceil(total / limit)),
            "demo": mock,
            "popularityAvailable": False,
            "items": [public(item) for item in items],
        }

    @app.get("/api/v1/wallpapers")
    def wallpapers(
        page: int = Query(1, ge=1, le=100000),
        limit: int = Query(24, ge=1, le=48),
        q: str = Query("", max_length=128),
        category: str = Query("", max_length=32, pattern=r"^[a-z0-9-]*$"),
        sort: Literal["new", "popular", "trending"] = "new",
    ):
        return page_result(page, limit, q, category, sort)

    @app.get("/api/v1/wallpapers/search")
    def search(
        q: str = Query(..., min_length=1, max_length=128),
        page: int = Query(1, ge=1, le=100000),
        limit: int = Query(24, ge=1, le=48),
        category: str = Query("", max_length=32, pattern=r"^[a-z0-9-]*$"),
        sort: Literal["new", "popular", "trending"] = "new",
    ):
        return page_result(page, limit, q, category, sort)

    @app.get("/api/v1/wallpapers/{scene_id}")
    def detail(scene_id: UUID):
        item = database.get(str(scene_id))
        if item is None:
            raise HTTPException(404, "Wallpaper not found")
        return public(item)

    @app.get("/assets/{scene_id}/{revision}/{filename}")
    def asset(scene_id: UUID, revision: str, filename: str):
        item = database.get(str(scene_id))
        if (
            item is None
            or not re.fullmatch(r"[0-9a-f]{64}", revision)
            or revision != item["sha256"]
            or filename not in ("package.awscene", "preview.png", "license.txt")
        ):
            raise HTTPException(404, "Asset not found")
        try:
            stream = storage.chunks(f"assets/{scene_id}/{revision}/{filename}")
        except FileNotFoundError:
            raise HTTPException(404, "Asset missing") from None
        except Exception:
            raise HTTPException(503, "Object storage unavailable") from None
        content_type = {
            "package.awscene": "application/octet-stream",
            "preview.png": "image/png",
            "license.txt": "text/plain; charset=utf-8",
        }[filename]
        headers = {"Cache-Control": "public,max-age=31536000,immutable"}
        if filename == "package.awscene":
            headers.update(
                {
                    "Content-Length": str(item["packageSize"]),
                    "Content-Disposition": 'attachment; filename="package.awscene"',
                    "ETag": '"' + revision + '"',
                }
            )
        elif filename == "preview.png":
            headers.update(
                {
                    "Content-Length": str(item["previewSize"]),
                    "ETag": '"' + item["previewSha256"] + '"',
                }
            )
        return StreamingResponse(stream, media_type=content_type, headers=headers)

    return app


def application():
    return create_app()
