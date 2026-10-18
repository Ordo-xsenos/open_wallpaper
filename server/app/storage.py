from pathlib import Path, PurePosixPath
from typing import Protocol
import os
import tempfile


def safe_key(key: str) -> str:
    path = PurePosixPath(key)
    if (
        not key
        or path.is_absolute()
        or any(part in ("", ".", "..") for part in key.split("/"))
        or "\\" in key
    ):
        raise ValueError("Unsafe object key")
    return key


class ObjectStorage(Protocol):
    def put(self, key: str, value: bytes, content_type: str): ...
    def chunks(self, key: str): ...


class LocalStorage:
    def __init__(self, root: Path):
        self.root = root.resolve()
        self.root.mkdir(parents=True, exist_ok=True)

    def path(self, key: str):
        target = self.root / safe_key(key)
        if not target.resolve().is_relative_to(self.root):
            raise ValueError("Object path escapes storage root")
        return target

    def put(self, key: str, value: bytes, content_type: str):
        target = self.path(key)
        target.parent.mkdir(parents=True, exist_ok=True)
        handle, temporary = tempfile.mkstemp(prefix=".object-", dir=target.parent)
        try:
            with os.fdopen(handle, "wb") as stream:
                stream.write(value)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, target)
        finally:
            Path(temporary).unlink(missing_ok=True)

    def chunks(self, key: str):
        # Open before constructing the streaming response, so missing objects return 404.
        stream = self.path(key).open("rb")

        def iterator():
            with stream:
                while block := stream.read(65536):
                    yield block

        return iterator()


class S3Storage:
    def __init__(self, bucket: str, endpoint: str = ""):
        import boto3
        from botocore.config import Config

        self.bucket = bucket
        self.client = boto3.client(
            "s3",
            endpoint_url=endpoint or None,
            config=Config(
                connect_timeout=5, read_timeout=15, retries={"max_attempts": 2}
            ),
        )

    def put(self, key: str, value: bytes, content_type: str):
        self.client.put_object(
            Bucket=self.bucket,
            Key=safe_key(key),
            Body=value,
            ContentType=content_type,
            CacheControl="public,max-age=31536000,immutable",
        )

    def chunks(self, key: str):
        body = self.client.get_object(Bucket=self.bucket, Key=safe_key(key))["Body"]

        def iterator():
            try:
                yield from body.iter_chunks(chunk_size=65536)
            finally:
                body.close()

        return iterator()
