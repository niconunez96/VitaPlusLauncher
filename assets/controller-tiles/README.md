# Controller tile artwork — integrated platform set

The launcher selects this artwork during VPK packaging. CMake installs the 29 selected PNGs at the existing `assets/aurora/` runtime paths, retaining other Aurora artwork. Source files remain here rather than being copied over the original generation set. The system PGF font and theme palette are retained.

## Ready for integration

- Twelve `platform_*.png` files: 128×128 controller-face square tiles. Arcade, Genesis and SNES were generated with FLUX 3 Image; the other nine use FLUX 3 Image Klein with accepted tiles as references.
- `ui_*.png`: sixteen 64×64 system icons generated as a FLUX atlas, separated, dark-background keyed and resized. `ui_romm.png` is the exception: it is downsampled from RomM's official circular symbol, not AI-generated.
- `platform-preview.png`: all twelve platforms shown at the renderer's 84px presentation size.
- `platform-preview-128.png` and `system-preview.png`: full-size platform and system readback sheets, not runtime textures.

All runtime candidates are bounded, non-interlaced PNGs. Preserve the existing 128px platform / 64px system texture contract. Palette and active theme configuration have not changed.

## Platform set

The preview follows the requested platform order, left to right:

1. Arcade, Dreamcast, Game Boy Advance, Nintendo 3DS.
2. Nintendo DS, GameCube, PS1, PS2.
3. PSP, Sega Genesis, SNES, Wii.

Nintendo 3DS uses `platform_n3ds.png`; PS1 uses `platform_psx.png`. Earlier Dreamcast/DS/PSP drafts that depicted hardware inside a tile were rejected and are not included. The completed candidates use square housings with controls embedded into their faces. Generation history and failed request IDs are retained in the feature log and memory.

Platform aliases and texture registrations include all twelve requested platforms, and the RomM settings section uses `ui_romm`. Small labels have a minimum `.75` scale with final-scale width measurement and UTF-8-safe truncation; card titles and notices can use two lines. Host tests and previews check the selected artwork and text boundaries. Physical-Vita PGF readability has not been verified; host previews cannot certify it.

The local Debug build at `dist/vitaplus.vpk` passed archive integrity and source-byte checks for all selected images and RomM notices. Host suites and independent ASan/UBSan renderer probes passed. The package contains 42 runtime PNGs total; source previews and generation-account metadata are excluded. Install and inspect the trial package on physical Vita before treating native readability or performance as verified.

## Provenance

| Candidate | Model | FLUX request ID |
|---|---|---|
| Arcade | FLUX 3 Image | `3f4b3b6e-41a6-4228-be1d-951c97e3b7fd` |
| Dreamcast | FLUX 3 Image Klein | `c102a51b-0a97-44de-99d6-92cc5ffcbfd0` |
| Game Boy Advance | FLUX 3 Image Klein | `9ebfaa8a-a375-4524-87c3-85ceadcf2182` |
| Nintendo 3DS | FLUX 3 Image Klein | `235550fe-04cc-4199-a4d2-37f904b1eb3a` |
| Nintendo DS | FLUX 3 Image Klein | `249fc1a1-c8dd-4269-91a7-455b10f02096` |
| GameCube | FLUX 3 Image Klein | `f9e9561b-0874-4021-b39d-04f089c2722d` |
| PS1 | FLUX 3 Image Klein | `c5a5f16e-b9f5-4aaf-a4e6-c8646ad7ece7` |
| PS2 | FLUX 3 Image Klein | `746e63f7-abc6-4d66-b469-5a5217b32cf2` |
| PSP | FLUX 3 Image Klein | `c64f9ec5-0e94-44dd-ab3e-d57b2a4a20fc` |
| Genesis | FLUX 3 Image | `3cf87e0c-56f4-4863-8215-d64e7db3d8cc` |
| SNES | FLUX 3 Image | `21856b06-c755-4ece-93a7-8ca3f087fa4d` |
| Wii | FLUX 3 Image Klein | `8b26b2ca-5462-4a0f-98fe-2ae2896793d3` |
| System atlas | FLUX 3 Image | `05cf0491-c61b-4e3e-957f-63b112dbbe7b` |

Generation date: 2026-11-17. Total observed consumption is 46.9 paid credits: 38.8 for the initial stage (including rejected visual drafts) plus 8.1 for the nine successful Klein outputs. The first Klein batch rendered three icons; one final controls-only reformulation rendered the other six. Moderated attempts did not reduce the observed balance. Starting balance: 899.2; final observed balance: 852.3. Image free pool: exhausted. All submitted jobs are terminal; no renders remain pending.

## RomM attribution

The circular symbol belongs to the RomM project and is used to identify its integration. Source: [official isotipo SVG](https://github.com/rommapp/romm/blob/master/frontend/assets/isotipo.svg), retrieved 2026-11-17. Git blob: `bc212d664f66168853e43545efb6489d00b07b09`. The unchanged SVG is retained as `romm-isotipo.svg`.

`ui_romm.png` is downsampled from the official circular `frontend/assets/isotipo.png` (Git blob `491e995d35661dbdc4f87cd2960f2ff87a336cf5`), without recoloring or a wordmark. The upstream raster is 8334×8334, above the normal assetprep source limit; it was reduced to a 512px intermediate before passing through the unmodified assetprep converter.

RomM's repository includes the GNU Affero General Public License v3, retained here as `RomM-AGPL-3.0.txt`. Attribution and license retention are not a grant of trademark endorsement. No project-wide licensing change is implied, and no distribution was performed.
