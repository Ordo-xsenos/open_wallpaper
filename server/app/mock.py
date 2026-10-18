"""Run with: python -m uvicorn server.app.mock:application --factory --host 127.0.0.1 --port 8765"""

from pathlib import Path
import json
import tempfile
from .config import Config
from .main import create_app


def application():
    directory = tempfile.TemporaryDirectory(prefix="aetherwall-mock-")
    fixture = Path(__file__).parents[1] / "mock"
    app = create_app(
        Config(
            database_url="sqlite:///" + str(Path(directory.name) / "catalog.sqlite3"),
            storage_root=fixture,
        ),
        mock=True,
    )
    app.state.mock_directory = directory
    for item in json.loads((fixture / "catalog.json").read_text())["items"]:
        app.state.database.upsert(item, True)
    return app
