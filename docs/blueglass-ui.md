# Blueglass UI: native layout, assets and verification

Blueglass implements the supplied VitaPlus renders on the native 960 × 544 canvas:
a console ribbon, seven-position game carousel, five-row list with detail panel,
and 5 × 2 cover mosaic. Inter and shared geometry drive both drawing and touch targets.

## Update an existing installation

1. Install the rebuilt `dist/vitaplus.vpk`.
2. Redeploy `services/imageproxy` to enable `mode=contain` for complete covers.
3. In VitaPlus, use **Ajustes → Conexión (RomM) → Sincronizar ahora**, followed by
   **Sincronizar metadata** to cache full covers and available game screenshots.

Existing `processed-*.png` covers remain visible until replacement `fit-v2-*.png`
files are synchronized. V1 catalog payloads are migrated in memory so the cached
library remains available offline. New saves use V2, which includes `background_url`.
Missing game artwork falls back to the bundled general wallpaper. The application
does not generate game artwork or fetch it from a new third-party provider.

## Typography

All interface text uses the bundled Inter files (Regular/Medium/SemiBold/Bold).
`ui_text.c` owns the exact eleven supplied roles: brand 18/700, navigation 17/500
(600 active), status 14/500, page title 30/700, subtitle 15/400, console title 15/600,
hero title 26/700, list title 15/500 (600 selected), grid title 14/500, description
14/400 and footer 14/500. Settings rows add one shared 20/500 role.

Titles use 1.15 leading; secondary text uses 1.35. Large titles keep normal spacing.
Text is measured at its final weight/size. UTF-8-safe truncation and bounded paragraph
layout caches avoid repeated wrapping work. Original console wordmarks and controller
symbols are graphics, not substitutions using font glyphs.

## Resource budget and ownership

| Resource | Contract |
| --- | --- |
| Platform logos | 25 transparent 128 × 128 PNGs, including Homebrew/generic |
| Backgrounds | 26 RGB 960 × 544 PNGs: one per mapped identity plus general |
| UI graphics | 20 PNGs; 64 × 64 symbols, a 96 × 96 focus and a 128 × 128 button atlas |
| Game covers | At most 12 cache slots shared by visible covers and game artwork |
| Remote images | Existing ≤512 × 512, 8-bit RGB/RGBA, ≤512 KiB PNG boundary |
| Bundled backgrounds | Exact 960 × 544, ≤2 MiB encoded; RGBA decode on heap |

The wallpaper worker only decodes CPU pixels. Main-thread preparation uploads and
retires textures after the previous GPU frame finishes, before `start_drawing`.
Only general/current/previous wallpapers are resident, plus one bounded handoff.
At most three small icons load per frame, outside the drawing scene. Console selection
is debounced for 120 ms before requesting a new wallpaper. Transitions are capped at
220 ms and `ui_motion_ms=0` disables them.

Glass uses one non-overlapping triangle fan per rounded surface; alpha-overlapping
circles would leave dark discs at corners. Focus glow is baked into a nine-slice PNG.
No live blur or reflection pass is used. RTC/battery refresh once per second;
storage information is queried only while its settings category is selected.
Grid view does not decode per-game background images it cannot display.

The footer uses classic DualShock-style dark caps with colored cross/circle/square/
triangle outlines. All four share one supersampled atlas, drawn as 24 × 24 sprites
with linear filtering. This avoids jagged runtime line primitives and extra texture
allocations. Settings restores the previously accepted circular RomM icon unchanged.

## Asset provenance

`assets/blueglass/NOTICE.md`, `provenance.json` and `generations.json` record sources.
Inter comes from its upstream static font files; platform logos and fallback hardware
illustrations come from attributed Carbon vectors/images. Sixteen FLUX 3 Image jobs
completed. Eight moderated submissions were not retried: their backgrounds instead
compose sourced hardware over the FLUX general wallpaper. Total settled image spend
was 76.8 credits. Homebrew/generic backgrounds compose their graphic identities on
that wallpaper. Signed URLs and game covers are not checked into the repository.

`scripts/prepare-blueglass.py` requires Pillow, CairoSVG, fonttools and Brotli.
`--static` builds upstream fonts, icons and compact logos; `--background NAME SOURCE`
converts a previously generated image; `--finish` composes the source-based backgrounds.
`--controls` restores RomM and rebuilds only the button atlas and their attribution.
Optional `--preview-fixtures DIRECTORY` downloads original Libretro cover samples for
host previews only. These fixtures never enter the VPK; the running app uses RomM.

## Verification

```sh
make test-ui test-renderer build/test_core
build/test_core
python3 -B tests/test_vita_render_lifecycle.py
python3 -B tests/test_vita_touch_navigation.py
python3 -B tests/test_romm_worker.py
python3 -B tests/test_vita_thread_priority.py
(cd services/imageproxy && go test ./...)
./scripts/build-vpk.sh --build-type Debug
python3 -B tests/test_vpk_blueglass.py
```

The Cairo/FreeType harness uses the actual Inter files and exercises 14 states,
long/incomplete UTF-8 text, final text bounds, missing icons, model immutability and
texture/font cleanup. `build/test_renderer <existing-output-directory> [cover-directory]`
writes previews. Host tinting approximates Vita rendering; these checks do not establish
Vita GPU behavior or a measured frame rate.

On real hardware, check cold/offline boot, touch/D-pad navigation across partial pages,
all three game views, RomM pairing, metadata sync, downloads with navigation audio,
suspend/resume, battery/status and storage. The frame-rate target is 60 FPS; it remains
a measurement target until verified on a physical Vita.
