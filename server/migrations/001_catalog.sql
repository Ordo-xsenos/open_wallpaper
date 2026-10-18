CREATE TABLE IF NOT EXISTS schema_version(version INTEGER PRIMARY KEY);
CREATE TABLE IF NOT EXISTS wallpapers(
    id TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    author TEXT NOT NULL,
    category TEXT NOT NULL,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    search_text TEXT NOT NULL,
    metadata TEXT NOT NULL,
    revision TEXT NOT NULL,
    published INTEGER NOT NULL DEFAULT 0,
    downloads INTEGER,
    rating REAL
);
CREATE INDEX IF NOT EXISTS wallpapers_category_created ON wallpapers(published,category,created_at,id);
CREATE INDEX IF NOT EXISTS wallpapers_created ON wallpapers(published,created_at,id);
CREATE INDEX IF NOT EXISTS wallpapers_popularity ON wallpapers(published,downloads,id);
