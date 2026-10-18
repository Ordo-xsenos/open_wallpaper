# Bundled scenes

`bundled/` contains five animated examples and one static regression scene.
Each subdirectory is a development scene package: `manifest.json` plus `assets/`.
The original procedural PNG artwork and scene manifests are dedicated under
[CC0-1.0](https://creativecommons.org/publicdomain/zero/1.0/). Generator code is
covered by the repository MIT license. No third-party wallpaper assets are used.

Regenerate deterministically with `python tools/Generate-BundledScenes.py`.
Preview PNGs are static illustrative thumbnails; they do not claim to capture
actual GPU output. `still` should stop presenting once its first frame is shown.

Previews are distinct original static reference artwork generated from each scene
by `tools/Generate-BundledScenes.py` (gradient/particle math at time zero and image
layer composition). They identify the scene; they are not Windows render captures
or compatibility evidence.
