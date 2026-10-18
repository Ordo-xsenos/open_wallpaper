# AetherWall catalog (M4)

Run commands from the repository root. Python 3.11+ is required. The API owns
catalog metadata and objects; it never reads a user's Core database.

## Local mock

```sh
python -m venv .venv
# Activate .venv (Windows: .venv\Scripts\Activate.ps1)
python -m pip install -e './server[test]'
python -m uvicorn server.app.mock:application --factory --host 127.0.0.1 --port 8765
```

In Manager Settings set catalog address `http://127.0.0.1:8765` and explicitly
enable local HTTP. Alternatively **Discover → Examples** or `--mock-catalog`
uses the same six embedded original packages without Python or a network socket.
Examples do not claim to be an external community catalog. Previews are static
reference illustrations, not Windows rendering captures.

## API and storage

`GET /api/v1/health`, `/categories`, `/wallpapers`, `/wallpapers/search?q=...`
and `/wallpapers/{uuid}`. Listings accept `page`, `limit` (1–48), `category`,
`sort=new|popular|trending`; search is case-folded title/author/description/tags.
Metadata carries license proof, SHA-256, byte sizes, version, schema support and
preview/package URLs. Ratings/download counts are null until measured; popularity
and trending currently use deterministic fallback ordering. Assets stream in
64 KiB chunks. Query parameters are bounded and SQL uses bound parameters.

SQLite is the local development adapter. PostgreSQL uses psycopg; ordered SQL
migration `migrations/001_catalog.sql` runs at startup and rejects unknown schema
versions. Back up PostgreSQL before upgrades. `ObjectStorage` supports a local
directory or S3-compatible storage through boto3. S3 credentials come from the
standard AWS environment/role chain; never put credentials into source or URLs.

Copy `.env.example` to an ignored `.env` and set real deployment values. Install
`./server[production]`, then run `server.app.main:application --factory` using
uvicorn behind a trusted HTTPS reverse proxy. Production mode requires PostgreSQL
and HTTPS public origins. `compose.yaml` supplies PostgreSQL and an API container
with local object storage; S3 configuration is optional. Provide TLS separately.
Use one API worker for the in-memory per-IP rate limit; multiple workers require
a shared proxy rate limiter. Configure trusted proxy addresses, never trust
arbitrary forwarded headers. Public read routes require no login. There is no
anonymous upload or public administration endpoint.

## Publication and moderation

Build `AetherWall.SceneCheck` first. To validate an original/authorized package
and create an unpublished draft:

```sh
python -m server.app.cli publish scene.awscene --validator out/build/portable-tests/AetherWall.SceneCheck --description "Original scene" --category nature --rights-proof permission.txt
python -m server.app.cli moderate SCENE_UUID --approve
```

On Windows use the built `.exe` validator. Publication validates the exact frozen
bytes with Core, ZIP budgets/paths, manifest, PNG preview, and explicit rights
proof. `publish --approve` is an explicit administrator approval. Missing rights
or failed validation prevents publication. `moderate UUID` withdraws a scene.
Keep the object bucket private when serving through this API so withdrawn drafts
cannot be retrieved directly. A public CDN needs its own purge/access policy.

## Verification

```sh
python -m pytest server/tests -q
python tools/Build-Catalog.py --validator out/build/portable-tests/AetherWall.SceneCheck
```

Tests use real ASGI handlers, SQLite and local objects without binding a socket.
PostgreSQL, S3, external HTTPS/CDN and deployment remain **NOT TESTED** here.
Public hosting, provider, domain and spending are intentionally unconfigured.
