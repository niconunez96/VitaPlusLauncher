# Aurora UI: assets and verification

Aurora replaces presentation, not the launcher model. Providers, download actions,
pairing, the persisted catalog, the system PGF font and controller mappings stay in
their existing layers. The store remains a placeholder.

## Quick checks

```sh
make test             # Portable core, platform aliases, geometry and theme defaults
make test-assets      # Go tool, rejected-input preservation, 36 bundled PNGs
make test-renderer    # Optional: Cairo + pkg-config; 11 read-only renderer scenarios
./scripts/build-vpk.sh
```

The headless renderer checks model immutability, empty states, all screens/views,
last-page geometry, text bounds, missing-asset fallbacks and texture cleanup. Its
Cairo text approximates PGF and its texture tint is approximate: it is **not** proof
of Vita GPU behavior, frame rate or font metrics. `build/test_renderer <existing-dir>`
writes PNG previews outside the workspace if requested.

## Asset inventory

| Group | Files | Runtime size |
| --- | --- | --- |
| Platform controllers/handhelds | 18 | 128×128 RGBA |
| UI symbols | 16 | 64×64 RGBA |
| Emblem | 1 | 128×128 RGBA |
| Wallpaper | 1 | 480×272 RGB |

`assets/aurora/provenance.tsv` records the FLUX model and request ID for every file.
Prompts and originals can be recovered from those IDs through FLUX history. Signed
download URLs are deliberately not stored. These are original generated illustrations,
not copied Cocoon assets or official manufacturer logos. The full set includes reserved
symbols such as search; a symbol does not introduce an unimplemented feature.

Controllers share icons only for related slug aliases (for example `psx`/`playstation`,
`genesis`/`mega-drive`/`megadrive`, and `gb`/`gbc`). Unknown slugs use the generic
controller. Aliases affect presentation only, never launch matching or grouping.

## Prepare a replacement

Generate isolated icons on a plain magenta backdrop with generous margins, consistent
upper-left studio lighting and unlabelled surfaces. Download the original locally, then:

```sh
go run ./scripts/assetprep/main.go -in /tmp/controller.png \
  -out assets/aurora/platform_snes.png -width 128 -height 128 -key
go run ./scripts/assetprep/main.go -in /tmp/wallpaper.png \
  -out assets/aurora/wallpaper.png -width 480 -height 272
```

`-key` removes only border-connected magenta, crops the subject and fits it within
88% of the canvas. Area averaging uses premultiplied channels to retain smooth alpha
edges. Source dimensions and bytes are bounded; errors leave existing outputs unchanged.
The tool writes 8-bit non-interlaced RGB/RGBA PNGs. It does not submit paid generations.

The CMake asset inventory is explicit. All PNGs are bundled under `assets/aurora/`;
the provenance document is source metadata, not a runtime dependency. The frontend
accepts only the exact expected dimensions and ≤512 KiB per bundled image. Loading
is one-shot and limited to one new bundled texture per frame. Files cannot add scripts
or change the catalog. All loaded bundled textures and covers are freed before shutdown.

## Theme controls

Existing `background`, `surface`, `accent`, `text`, `muted` and `view` keys still work.
The portable `VpTheme` and catalog layout are unchanged. New presentation-only keys:

| Key | Default | Accepted values |
| --- | --- | --- |
| `ui_motion_ms` | 180 | 0–600; 0 disables focus/dock animation |
| `ui_radius` | 16 | 6–20 pixels |
| `ui_wallpaper_alpha` | 130 | 0–255 |

Missing values retain defaults; malformed/out-of-range values are ignored. User theme
overrides are still read from `ux0:data/vitaplus/themes/<theme>/theme.ini`. Bundled
icon replacement from external theme directories is not implemented in this pass.

## On-device acceptance (pending)

- Boot with and without a catalog/cover cache; navigate both platform pages.
- Open a console and cycle grid/list/carousel. Check long and non-ASCII game names.
- Verify `X`, circle, L/R, triangle and square retain their documented actions.
- Pair through the system IME; check no token appears on screen.
- Download, cancel and retry while browsing; inspect progress and failure messages.
- Check the installation confirmation remains visible beyond six seconds.
- Check all icons, alpha edges, PGF text, idle/scroll frame rate and suspend/resume on Vita.

Host tests and a successful VPK build cannot replace these device checks.
