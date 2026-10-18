from contextlib import contextmanager
from pathlib import Path
import json
import sqlite3


class Database:
    """Bound-parameter DB-API repository: SQLite development and psycopg PostgreSQL."""

    def __init__(self, url: str):
        self.url = url
        self.postgres = url.startswith("postgresql://")
        if not self.postgres:
            Path(url.removeprefix("sqlite:///")).parent.mkdir(
                parents=True, exist_ok=True
            )
        self.migrate()

    @contextmanager
    def connect(self):
        if self.postgres:
            import psycopg
            from psycopg.rows import dict_row

            connection = psycopg.connect(
                self.url, row_factory=dict_row, connect_timeout=5
            )
        else:
            connection = sqlite3.connect(self.url.removeprefix("sqlite:///"), timeout=5)
            connection.row_factory = sqlite3.Row
        try:
            yield connection
            connection.commit()
        except BaseException:
            connection.rollback()
            raise
        finally:
            connection.close()

    def execute(self, connection, sql, parameters=()):
        return connection.execute(
            sql.replace("?", "%s") if self.postgres else sql, parameters
        )

    def migrate(self):
        script = (Path(__file__).parents[1] / "migrations/001_catalog.sql").read_text()
        with self.connect() as connection:
            self.execute(
                connection,
                "CREATE TABLE IF NOT EXISTS schema_version(version INTEGER PRIMARY KEY)",
            )
            version = self.execute(
                connection, "SELECT version FROM schema_version"
            ).fetchone()
            if version is not None and version["version"] != 1:
                raise ValueError(
                    "Unsupported catalog database schema; refusing migration"
                )
            for statement in script.split(";"):
                if statement.strip():
                    self.execute(connection, statement)
            if version is None:
                self.execute(
                    connection, "INSERT INTO schema_version(version) VALUES(1)"
                )

    def upsert(self, item: dict, published: bool):
        search = " ".join(
            (item["title"], item["author"], item["description"], *item["tags"])
        ).casefold()
        with self.connect() as connection:
            existing = self.execute(
                connection,
                "SELECT created_at FROM wallpapers WHERE id=?",
                (item["id"],),
            ).fetchone()
            if existing:
                item = dict(item, createdAt=existing["created_at"])
            self.execute(
                connection,
                """INSERT INTO wallpapers(id,title,author,category,created_at,updated_at,search_text,metadata,revision,published)
                VALUES(?,?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET title=excluded.title,author=excluded.author,
                category=excluded.category,updated_at=excluded.updated_at,search_text=excluded.search_text,
                metadata=excluded.metadata,revision=excluded.revision,published=excluded.published""",
                (
                    item["id"],
                    item["title"],
                    item["author"],
                    item["category"],
                    item["createdAt"],
                    item["updatedAt"],
                    search,
                    json.dumps(item, ensure_ascii=False),
                    item["sha256"],
                    int(published),
                ),
            )

    def list(self, page: int, limit: int, query: str, category: str, sort: str):
        where, parameters = "published=1", []
        if query:
            where += " AND search_text LIKE ? ESCAPE '!'"
            parameters.append(
                "%"
                + query.casefold()
                .replace("!", "!!")
                .replace("%", "!%")
                .replace("_", "!_")
                + "%"
            )
        if category:
            where += " AND category=?"
            parameters.append(category)
        # Unmeasured popularity is NULL, never invented. Stable fallback ordering.
        order = (
            "created_at DESC,id"
            if sort == "new"
            else "CASE WHEN downloads IS NULL THEN 1 ELSE 0 END,downloads DESC,created_at DESC,id"
        )
        with self.connect() as connection:
            count = self.execute(
                connection,
                f"SELECT count(*) AS total FROM wallpapers WHERE {where}",
                parameters,
            ).fetchone()["total"]
            rows = self.execute(
                connection,
                f"SELECT metadata FROM wallpapers WHERE {where} ORDER BY {order} LIMIT ? OFFSET ?",
                (*parameters, limit, (page - 1) * limit),
            ).fetchall()
        return count, [json.loads(row["metadata"]) for row in rows]

    def get(self, scene_id: str):
        with self.connect() as connection:
            row = self.execute(
                connection,
                "SELECT metadata FROM wallpapers WHERE id=? AND published=1",
                (scene_id,),
            ).fetchone()
        return json.loads(row["metadata"]) if row else None

    def categories(self):
        with self.connect() as connection:
            return [
                dict(row)
                for row in self.execute(
                    connection,
                    "SELECT category AS id,count(*) AS count FROM wallpapers WHERE published=1 GROUP BY category ORDER BY category",
                )
            ]

    def set_published(self, scene_id: str, published: bool):
        with self.connect() as connection:
            self.execute(
                connection,
                "UPDATE wallpapers SET published=? WHERE id=?",
                (int(published), scene_id),
            )
