from dataclasses import dataclass
from pathlib import Path
import os
from urllib.parse import urlparse


@dataclass(frozen=True)
class Config:
    database_url: str = "sqlite:///./out/catalog.sqlite3"
    storage_root: Path = Path("out/catalog-objects")
    public_base_url: str = "http://127.0.0.1:8765"
    asset_base_url: str = ""
    development: bool = True
    storage_backend: str = "local"
    s3_bucket: str = ""
    s3_endpoint: str = ""
    requests_per_minute: int = 240

    def validate(self):
        for value in (
            self.public_base_url,
            self.asset_base_url or self.public_base_url,
        ):
            url = urlparse(value)
            local = url.hostname in ("127.0.0.1", "localhost", "::1")
            if (
                url.username
                or url.password
                or url.query
                or url.fragment
                or url.path.rstrip("/")
                or not url.hostname
            ):
                raise ValueError(
                    "Public origins must contain scheme, host and optional port only"
                )
            if url.scheme != "https" and not (
                self.development and local and url.scheme == "http"
            ):
                raise ValueError(
                    "Public origins require HTTPS; development permits explicit loopback HTTP"
                )
        if not self.database_url.startswith(("sqlite:///", "postgresql://")):
            raise ValueError("Expected SQLite development or PostgreSQL database URL")
        if not self.development and not self.database_url.startswith("postgresql://"):
            raise ValueError("Production mode requires PostgreSQL")
        if self.storage_backend not in ("local", "s3"):
            raise ValueError("Unsupported object storage backend")
        if self.storage_backend == "s3" and not self.s3_bucket:
            raise ValueError("S3 bucket is required")
        if (
            self.s3_endpoint
            and urlparse(self.s3_endpoint).scheme != "https"
            and not self.development
        ):
            raise ValueError("Production S3 endpoint requires HTTPS")
        if not 1 <= self.requests_per_minute <= 10000:
            raise ValueError("Invalid rate limit")
        return self

    @classmethod
    def from_env(cls):
        return cls(
            database_url=os.getenv("AETHERWALL_DATABASE_URL", cls.database_url),
            storage_root=Path(
                os.getenv("AETHERWALL_STORAGE_ROOT", "out/catalog-objects")
            ),
            public_base_url=os.getenv(
                "AETHERWALL_PUBLIC_BASE_URL", cls.public_base_url
            ),
            asset_base_url=os.getenv("AETHERWALL_ASSET_BASE_URL", ""),
            development=os.getenv("AETHERWALL_DEVELOPMENT", "1") == "1",
            storage_backend=os.getenv("AETHERWALL_STORAGE_BACKEND", "local"),
            s3_bucket=os.getenv("AETHERWALL_S3_BUCKET", ""),
            s3_endpoint=os.getenv("AETHERWALL_S3_ENDPOINT", ""),
            requests_per_minute=int(os.getenv("AETHERWALL_RATE_LIMIT", "240")),
        ).validate()
