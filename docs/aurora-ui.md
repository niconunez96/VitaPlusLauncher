# Aurora UI: assets and verification

Aurora replaces presentation, not the launcher model. Providers, download actions,
pairing, the persisted catalog, the system PGF font and controller mappings stay in
their existing layers. The store remains a placeholder.

## Quick checks

```sh
make test             # Core, aliases, selected VPK artwork, geometry, theme and lifecycle
make test-assets      # Go converter and the original 36-image source inventory
make test-renderer    # Cairo: 11 states, all 12 requested platforms, RomM and bounded text
./scripts/build-vpk.sh --build-type Debug
```

The headless renderer checks model immutability, empty states, all screens/views,
last-page geometry, minimum .75 text scale, long ASCII/UTF-8 truncation, narrow text
boxes, missing-asset fallbacks and texture cleanup. Its
Cairo text approximates PGF and its texture tint is approximate: it is **not** proof
of Vita GPU behavior, frame rate or font metrics. `build/test_renderer <existing-dir>`
writes PNG previews outside the workspace if requested.

## Asset inventory

| Group | Source location | Files | Runtime size |
| --- | --- | --- | --- |
| Platform controllers/handhelds | `assets/aurora/console_icons/` | 23 | 128×128 RGB/RGBA |
| UI symbols, including official RomM | `assets/aurora/ui_icons/` | 17 | 64×64 RGB/RGBA |
| Emblem | `assets/aurora/emblem.png` | 1 | 128×128 RGBA |
| Wallpaper | `assets/aurora/wallpaper.png` | 1 | 480×272 RGB |

`assets/legacy/provenance.tsv` records the original FLUX source inventory preserved in the repo.
`assets/aurora/controller-tiles.md` records the replacement artwork and provenance notes for the
integrated controller/system set. CMake packages all runtime PNGs under flat `assets/aurora/*.png`
paths inside the VPK, regardless of the grouped source folders used in the repository. The runtime
contains 42 PNGs in total. RomM attribution, its original SVG and upstream license are included
under `assets/aurora/licenses/` in the repo and packaged to `assets/licenses/` in the VPK. Preview
sheets are kept in `assets/aurora/previews/` and are not bundled.

For generated images, prompts and originals can be recovered from those IDs through FLUX history. Signed
download URLs are deliberately not stored. These are original generated illustrations,
not copied Cocoon assets or official manufacturer logos; the identified RomM symbol
is the explicitly attributed exception. The full set includes reserved
symbols such as search; a symbol does not introduce an unimplemented feature.

Controllers share icons only for related slug aliases (for example `psx`/`playstation`,
`genesis`/`mega-drive`/`megadrive`, and `gb`/`gbc`). Unknown slugs use the generic
controller. Aliases affect presentation only, never launch matching or grouping.

## Readable text on the native canvas

The existing system PGF remains in use. Labels now have a minimum scale of `.75`,
approximately 18px in the Cairo preview rather than the earlier 10–14px small labels.
Native PGF metrics still require a physical-Vita check. Text is measured at its final
scale and shortened only at UTF-8 boundaries, with space reserved for the ellipsis;
boxes too narrow for an ellipsis draw empty text instead of overflowing.

Platform and grid titles use up to two lines. The footer has wider navigation labels
and two lines of help, while download rows reserve separate title/error baselines and
show four entries per page. Long notices also use two lines. The palette, navigation,
download actions, catalog records and GPU synchronization order are unchanged.
The compact `O` and `[]` hints mean circle/back and square/favorite respectively.

## Prepare a replacement

Generate isolated icons on a plain magenta backdrop with generous margins, consistent
upper-left studio lighting and unlabelled surfaces. Download the original locally, then place
the result in the matching grouped source folder:

```sh
go run ./scripts/assetprep \
  -in /tmp/platform-snes.png \
  -out assets/aurora/console_icons/platform_snes.png -width 128 -height 128 -key
go run ./scripts/assetprep \
  -in /tmp/ui-home.png \
  -out assets/aurora/ui_icons/ui_home.png -width 64 -height 64 -key
go run ./scripts/assetprep \
  -in /tmp/wallpaper.png \
  -out assets/aurora/wallpaper.png -width 480 -height 272
```

`-key` removes only border-connected magenta, crops the subject and fits it within
88% of the canvas. Area averaging uses premultiplied channels to retain smooth alpha
edges. Source dimensions and bytes are bounded; errors leave existing outputs unchanged.
The tool writes 8-bit non-interlaced RGB/RGBA PNGs. It does not submit paid generations.

The CMake asset inventory is explicit. Runtime PNGs are bundled as flat `assets/aurora/*.png`
entries in the VPK, even though source artwork is grouped in `console_icons/` and `ui_icons/`.
The original provenance document is kept as source metadata in `assets/legacy/provenance.tsv`.
The frontend accepts only the exact expected dimensions and ≤512 KiB per bundled image. Loading
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
