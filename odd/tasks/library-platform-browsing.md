# Feature: library-platform-browsing

Objective: group the library by console before listing ROMs, add 2D arrow navigation, leave Stores empty for now, and surface the RomM metadata already available (cover, background, summary).
Branch: main (changes left uncommitted to preserve pre-existing worktree edits)
Delivery strategy: single-pr (uncommitted working tree)
Test runner: `make test` (host core) and `./scripts/build-vpk.sh` (Vita UI compile)

## Specs

S1. Stores screen is empty for now.
Acceptance: The Stores screen renders only a "Proximamente" placeholder and no store or catalog content.
Check: `draw_shell` Stores branch; manual.

S2. Library groups by console first, then shows that console's ROMs.
Acceptance: Library level 1 lists unique platforms (for example `nds`, `snes`, `psp`) with item counts; Cross on a platform lists only that platform's ROMs; Circle returns to the platform list.
Check: `vp_catalog_platforms`, `vp_catalog_platform_items`, `vp_app_open`, `vp_app_back` host tests.

S3. All four arrow directions navigate the library.
Acceptance: Up/down/left/right move the selection in both the platform grid and the ROM grid, wrapping at the edges; list and carousel views also respond to left/right.
Check: `vp_app_move` host tests.

S4. The selected game shows RomM metadata.
Acceptance: The selected ROM shows its cover image, a dimmed cover background, its `summary` text, and `platform_display_name` when provided.
Check: `vp_romm_import`/`vp_romm_sync` parse `summary`, `platform_display_name`, and `path_cover_small`/`path_cover_large`; manual Vita render.

## Tasks

- [x] T1 - Core platform grouping and RomM metadata parsing. Specs S2, S4. Route: inline. Tests: `tests/test_core.c` (`test_platform_grouping`, `test_romm_metadata`). RED observed as compile failure, then `make test` PASS.
- [x] T2 - Core library-level navigation (platform grid and ROM grid). Specs S2, S3. Route: inline. Tests: `tests/test_core.c` (`test_library_navigation`). RED observed as compile failure, then `make test` PASS.
- [x] T3 - Vita UI: platform/ROM levels, Stores placeholder, all-arrow input. Specs S1, S2, S3. Route: inline. Check: `./scripts/build-vpk.sh --clean` PASS.
- [x] T4 - Vita UI: cover cache, dimmed background, detail metadata. Spec S4. Route: inline. Check: `./scripts/build-vpk.sh --clean` PASS; added `png jpeg` to CMake link libraries.
- [x] T5 - Docs and full verification. Route: inline. Check: `make test` PASS and `./scripts/build-vpk.sh --clean` PASS; `README.md` and `docs/providers.md` updated.

## Log

L1 (user, 2026-11-16): "Algunos ajustes que quiero que hagamos:
1. La seccion de tiendas tienen que ir otras cosas por ahora dejala vacia
2. En la seccion de biblioteca primero quiero que agrupemos por consola "nds", "snes", "psp" y luego cuando entramos a una consola vemos las roms
3. Poder movernos por la biblioteca usando todas las flechas (arriba, abajo, izq, der)
4. Si no es muy dificil podriamos mostrar la metadata de los juegos que ya nos proporciona romm ? como caratulas, imagen de fondo etc"

L2 (2026-11-16): Implemented S1-S4. Core adds `vp_catalog_platforms`, `vp_catalog_platform_items`, `vp_app_visible`, `vp_app_move`, `vp_app_open`, `vp_app_back`, plus `platform_display`, `summary`, and `path_cover_small`/`path_cover_large` parsing. Vita UI adds platform/ROM levels, all-arrow movement, Stores placeholder, cover cache under `ux0:data/vitashelflauncher/cache/covers/`, dimmed cover background, and detail metadata. Evidence: `make test` -> `vitashelflauncher core tests: PASS`; `./scripts/build-vpk.sh --clean` -> `dist/vitashelflauncher.vpk` (2.5 MB, sha256 `669939057839ddd51db869ca559af159e28eac67f2e483c781e0a75cf2129ab9`). Added `png jpeg` to the Vita link libraries because vita2d's PNG/JPEG loaders now get used. No commits created: pre-existing worktree edits remain untouched.
