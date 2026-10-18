# Scene format v1 (M1)

A scene is an ordinary directory or a `.awscene` ZIP with `manifest.json` at its
root and PNG/JPEG/static WebP files beneath `assets/`. ZIPs are read into bounded
memory, never extracted to disk. See [the schema](scene.schema.json) and the six
working [bundled examples](../scenes/README.md).

## Manifest and rendering

Required metadata: integer `schemaVersion: 1`, UUID `id`, nonempty `title`, `author`,
`license`, existing image `preview`, `canvas`, and 1–64 `layers`. `tags` and `fpsHint`
are optional. `fpsHint` is advisory metadata; the explicit `--fps` cap wins.
No legacy format exists yet: unknown schema versions are rejected, never guessed
or silently migrated. Unknown properties, duplicate JSON keys/IDs, invalid UTF-8,
deep JSON, and inappropriate layer-specific fields are rejected.

Canvas dimensions are 1–8192. `fit` is `cover` (crop), `contain` (letterbox), or
`stretch`; it controls the canvas-to-display mapping and image-to-canvas mapping.
`background` is an optional linear RGBA clear color, default `[0.02,0.03,0.06,1]`.
Images are treated as SDR/sRGB, sampled to linear space, alpha blended, then
encoded through an sRGB render target. HDR and ICC color-management are deferred.

Layers are sorted stably by integer `zIndex` (default 0). Common optional fields:

| Field | Meaning / default |
| --- | --- |
| `opacity` | 0–1, default 1 |
| `tint` | Linear RGBA multipliers, each 0–1; default white |
| `transform.position` | Center in canvas pixels, origin top-left; default canvas center |
| `transform.scale` | Positive `[x,y]`, each 0.01–16; default `[1,1]` |
| `transform.rotation` | Clockwise degrees, default 0 |
| `motion` | `{ "amplitude": [24,0], "speed": 0.2 }`; sinusoidal canvas-pixel motion, speed in radians/sec |
| `effect` | `none` or trusted built-in `pulse` (animated opacity); default `none` |

- `gradient`: optional linear `colorA`/`colorB`, and `speed` 0–10. Default speed
  0.65; set 0 to make it static. Uses the bundled shader, never external HLSL.
- `image`: requires `source`, which must reference an existing image under assets.
  A single texture is cached and shared by all layers using the same asset.
- `particles`: requires `preset`: `stars`, `rain`, or `snow`. Optional `parameters`:
  `count` 1–2048 (default 120), `speed` 0–10 (default 0.65), and `size` 1–64 canvas
  pixels (default 3). Total particle count across layers cannot exceed 4096.
  One instanced draw per particle layer; positions are generated on the GPU.

Zero speed freezes a procedural layer. Any nonzero motion or pulse makes it
animated. Fully transparent layers do not force animation. Once a static scene
has presented successfully, it draws again only after invalidation (paint, resize,
reload, recovery). A 250 ms Shell/stop watchdog remains; it does not call Present
on a stable static scene. Animations stop advancing while locked/suspended/minimized.

## Package validation and limits

Paths use portable ASCII letters/digits, `_`, `-`, `.`, and `/`, with at most
200 bytes. Reject absolute paths, traversal, empty components, backslashes,
Windows device names, alternate streams, trailing dots, links, encryption,
case-colliding entries and executable/script/shader files. Text metadata is UTF-8;
native string limits count encoded bytes (schema character limits are an initial
structural check). Additional semantic checks, including budgets and references,
are enforced by the C++ loader beyond JSON Schema.

| Budget | Limit |
| --- | --- |
| Encoded ZIP / individual asset | 32 MiB |
| Total expanded package | 64 MiB |
| Manifest | 256 KiB; JSON depth at most 16 |
| Package entries, including directories | 256 |
| Compression expansion | At most 200× ZIP bytes + 1 MiB |
| Individual image | At most 8192×8192 |
| Aggregate decoded RGBA | 128 MiB, including previews |

Core fully decodes every image before committing a scene, including previews.
PNG/JPEG use WIC; WebP uses libwebp. Package checks inspect headers before any
pixel allocation. `SceneCheck` validates manifests, packages and headers; full
pixel decoding is verified in Core on Windows. During transactional replacement,
old and new GPU textures can temporarily coexist (each scene is individually
bounded). Loading is synchronous in this diagnostic M1, without background I/O.

## Use and reload

```powershell
$exe = '.\out\build\win-msvc-debug\AetherWall.Core.exe'
& $exe --mode window --scene scenes/bundled/stars --duration 120
& $exe --mode window --scene scenes/bundled/still --duration 120
.\out\build\win-msvc-debug\AetherWall.SceneCheck.exe scenes/bundled/mountains
```

Press F5 while the diagnostic window has focus to reload the same path. Validate
and prepare all resources first; if any step fails, the previous scene stays
active. At startup, a bad scene leaves the built-in gradient active with an error
log. Logs must show `Scene committed` for the intended scene before claiming success.
Desktop mode never captures keyboard input; use a new process to change its scene.

Create `.awscene` ZIPs with `manifest.json` and `assets/` at archive root, not a
containing scene directory. For example, from PowerShell:

```powershell
Compress-Archive -Path scenes/bundled/stars/manifest.json,scenes/bundled/stars/assets -DestinationPath out/stars.zip -Force
Move-Item out/stars.zip out/stars.awscene -Force
.\out\build\win-msvc-debug\AetherWall.SceneCheck.exe out/stars.awscene
```
