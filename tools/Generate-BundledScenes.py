"""Generate original M1 fixtures/artwork with Python's standard library only."""

from pathlib import Path
import json
import math
import struct
import uuid
import zlib

ROOT = Path(__file__).resolve().parents[1] / "scenes" / "bundled"
WIDTH, HEIGHT = 640, 360


def png(path, pixel):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    rows = b"".join(b"\0" + bytes(c for x in range(WIDTH) for c in pixel(x, y)) for y in range(HEIGHT))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 6, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def sky(x, y):
    t = y / HEIGHT
    return (int(12 + 30 * t), int(22 + 44 * t), int(50 + 45 * t), 255)


def mountain(x, y, foreground=False):
    horizon = (240 if foreground else 165) + 32 * math.sin(x * 0.012) + 18 * math.sin(x * 0.027 + 1.7)
    if y < horizon:
        return (0, 0, 0, 0)
    return (14, 31, 42, 255) if foreground else (43, 70, 87, 255)


def still(x, y):
    front = mountain(x, y, True)
    back = mountain(x, y)
    return front if front[3] else back if back[3] else sky(x, y)


def encode_srgb(value):
    value = max(0.0, min(1.0, value))
    return round(255 * (12.92 * value if value <= 0.0031308 else 1.055 * value ** (1 / 2.4) - 0.055))


def decode_srgb(value):
    value /= 255
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def shader_hash(value):
    value = (value ^ 61) ^ (value >> 16)
    value = (value * 9) & 0xffffffff
    value ^= value >> 4
    value = (value * 0x27d4eb2d) & 0xffffffff
    value ^= value >> 15
    return (value & 0x00ffffff) / 16777216


def preview(name):
    """Original static reference artwork, derived from trusted scene parameters.

    Mirrors the gradient/particle math at t=0, and image-layer composition.
    These are generated previews, not screenshots or Windows acceptance evidence.
    """
    def background(x, y):
        if name in ("rain", "still"):
            return still(x, y)
        if name == "mountains":
            sx = (x - WIDTH / 2) / 1.1 + WIDTH / 2
            sy = (y - HEIGHT / 2) / 1.1 + HEIGHT / 2
            front, back = mountain(sx, sy, True), mountain(sx, sy)
            return front if front[3] else back if back[3] else sky(x, y)
        px, py = ((x + 0.5) / WIDTH - 0.5) * (16 / 9), (y + 0.5) / HEIGHT - 0.5
        wave = 0.5 + 0.5 * math.sin(px * 3 + py * 2)
        glow = math.exp(-(px * px + py * py) * 2)
        a, b, light = (0.025, 0.05, 0.16), (0.08, 0.40, 0.56), (0.15, 0.07, 0.20)
        return tuple(encode_srgb(a[k] * (1 - wave) + b[k] * wave + light[k] * glow) for k in range(3)) + (255,)

    pixels = [[background(x, y) for x in range(WIDTH)] for y in range(HEIGHT)]
    if name in ("stars", "rain", "minimal_particles"):
        count, size, elongation = {"stars": (180, 4, 1), "rain": (500, 3, 6), "minimal_particles": (32, 6, 1)}[name]
        rx, ry = size * WIDTH / 1920, size * HEIGHT / 1080 * elongation
        for index in range(count):
            sx, sy = shader_hash(index * 2 + 1), shader_hash(index * 2 + 2)
            cx, cy = sx * WIDTH, sy * HEIGHT
            brightness = 0.6 + 0.4 * math.sin(sx * 6.283)
            for y in range(max(0, int(cy - ry)), min(HEIGHT, math.ceil(cy + ry))):
                for x in range(max(0, int(cx - rx)), min(WIDTH, math.ceil(cx + rx))):
                    q = ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2
                    t = max(0.0, min(1.0, (q - 0.25) / 0.75))
                    alpha = 1 - t * t * (3 - 2 * t)
                    base = pixels[y][x]
                    pixels[y][x] = tuple(encode_srgb(decode_srgb(base[k]) * (1 - alpha) + brightness * alpha) for k in range(3)) + (255,)
    return lambda x, y: pixels[y][x]


def gradient(speed=0):
    return {"id": "background", "type": "gradient", "speed": speed, "zIndex": 0}


def particles(preset, count, speed, size=3):
    return {"id": "particles", "type": "particles", "preset": preset, "zIndex": 10,
            "parameters": {"count": count, "speed": speed, "size": size}}


definitions = {
    "stars": ("Starry night", [gradient(), particles("stars", 180, 0.25, 4)]),
    "rain": ("Mountain rain", [{"id": "background", "type": "image", "source": "assets/still.png"},
                                particles("rain", 500, 0.5, 3)]),
    "gradient": ("Floating gradient", [gradient(0.65)]),
    "mountains": ("Mountain parallax", [
        {"id": "sky", "type": "image", "source": "assets/sky.png"},
        {"id": "back", "type": "image", "source": "assets/back.png", "zIndex": 1,
         "transform": {"scale": [1.1, 1.1]}, "motion": {"amplitude": [24, 0], "speed": 0.2}},
        {"id": "front", "type": "image", "source": "assets/front.png", "zIndex": 2,
         "transform": {"scale": [1.1, 1.1]}, "motion": {"amplitude": [-40, 0], "speed": 0.2}},
    ]),
    "minimal_particles": ("Quiet snow", [gradient(), particles("snow", 32, 0.15, 6)]),
    "still": ("Still mountains", [{"id": "background", "type": "image", "source": "assets/still.png"}]),
}

for name, (title, layers) in definitions.items():
    folder = ROOT / name
    png(folder / "assets" / "preview.png", preview(name))
    if name in ("rain", "still"):
        png(folder / "assets" / "still.png", still)
    if name == "mountains":
        png(folder / "assets" / "sky.png", sky)
        png(folder / "assets" / "back.png", mountain)
        png(folder / "assets" / "front.png", lambda x, y: mountain(x, y, True))
    manifest = {
        "schemaVersion": 1, "id": str(uuid.uuid5(uuid.NAMESPACE_URL, "aetherwall/bundled/" + name)),
        "title": title, "author": "AetherWall contributors", "license": "CC0-1.0",
        "tags": [name], "preview": "assets/preview.png",
        "canvas": {"width": 1920, "height": 1080, "fit": "cover"}, "fpsHint": 30,
        "layers": layers,
    }
    (folder / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
print(f"Generated {len(definitions)} original scenes under {ROOT}")
