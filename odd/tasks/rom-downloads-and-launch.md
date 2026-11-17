# Feature: rom-downloads-and-launch

Objective: track which RomM ROMs are downloaded, show a download arrow or a white check, keep the real ROM extension, and launch downloaded ROMs through a configurable emulator profile (RetroArch by default).
Branch: main
Delivery strategy: single-pr (same working tree)
Test runner: `make test` (host core) and `./scripts/build-vpk.sh` (Vita UI compile)

## Specs

S1. Not-downloaded ROMs show a download arrow and downloaded ROMs show a white check inside the library.
Acceptance: The ROM grid, list and carousel render a downward arrow badge while `item.installed` is false and a white check badge when it is true; the detail panel says `Sin descargar` / `Descargado`.
Check: Vita UI render; `item.installed` host assertions.

S2. The download uses the real RomM file name and extension, and works when the API omits `download_url`.
Acceptance: `fs_name` is parsed; the saved file is `<downloads>/<source>_<id>.<extension>`; when `download_url` is absent, the core synthesizes `/api/roms/<id>/content/<fs_name>` and resolves it against the RomM base URL.
Check: `vp_romm_import` + `vp_item_extension` + `vp_item_download_path` host tests.

S3. Downloaded ROMs launch through a configurable emulator profile.
Acceptance: Pressing X on an installed ROM builds `psgm:play?titleid=<titleid>&param=<core>&param2=<rom>` when a core is configured, or `psgm:play?titleid=<titleid>&param=<rom>` without one, then calls `sceAppMgrLaunchAppByUri(0x20000, uri)`. A missing platform profile shows a clear notice instead of failing silently.
Check: `vp_build_launch_uri` host tests; Vita build.

S4. Downloaded state survives a RomM resync and app restart.
Acceptance: After synchronization the UI checks the computed download path on `ux0:data/vitaplus/downloads/`; existing files mark the ROM as installed and set `launch_target`. Incomplete downloads are written to `<path>.part` and renamed only on success.
Check: Vita build; manual restart check.

S5. Emulator mapping is configurable without recompiling.
Acceptance: `config.ini` accepts `launch_<platform>=<titleid>[:<core>]`; defaults cover common RetroArch platforms (snes, nes, gba, gb, gbc, genesis/mega-drive, mastersystem, gamegear, psx, arcade, neogeo); `save_romm_config` preserves the mapping.
Check: `vp_launch_config_init` / `vp_launch_config_set` host tests.

S6. Controls and documentation reflect the new behavior.
Acceptance: The header hint says X downloads or plays; README and provider docs describe the download path, extensions and launch configuration.
Check: Review of `README.md`, `docs/providers.md`.

## Tasks

- [x] T1 - Core download naming and RomM `fs_name` parsing. Specs S2. Route: inline. Tests: `tests/test_core.c` (`test_download_paths`, `test_romm_content_download`). RED: compile failure `VpItem has no member named fs_name`; GREEN: `make test` PASS.
- [x] T2 - Core launch profile config and URI builder. Specs S3, S5. Route: inline. Tests: `tests/test_core.c` (`test_launch_profiles`). RED: `VpLaunchConfig undeclared`; GREEN: `make test` PASS.
- [x] T3 - Vita download state, icons and activation flow. Specs S1, S3, S4. Route: inline. Check: `./scripts/build-vpk.sh --clean` PASS (VPK 2.5 MiB).
- [x] T4 - Config load/save and launch backend. Specs S3, S5. Route: inline. Added `vp_vita_launch_uri` and `launch_*` config persistence. Check: `./scripts/build-vpk.sh --clean` PASS.
- [x] T5 - Docs and full verification. Spec S6. Check: `make test` PASS; `./scripts/build-vpk.sh --clean` PASS; README and `docs/providers.md` updated.

## Log

L1 (user, 2026-11-17): "me gustaria que trabajemos ahora en gestionar las descarga de las roms, me gustaria que mostremos con un icono de flechita esas roms que todavia no estan descargadas y las que ya estan descargadas con un "white check".
La idea es que una vez descargadas estas roms puedan ser ejecutadas por un emulador como retroarch"

L2 (2026-11-17): Plan accepted inline. Assumption: launch profiles are configured per platform so NDS/PSP can use DSVita/Adrenaline-style title IDs instead of hardcoding RetroArch for every console.

L3 (2026-11-17): RomM `SimpleRomSchema` has no `download_url` field, so S2 synthesizes `/api/roms/{id}/content/{fs_name}`. Verified against demo `openapi.json`.
L4 (2026-11-17): Core RED observed as compile errors, then `make test` GREEN; Vita build produced `dist/vitaplus.vpk`.
L5 (2026-11-17): Added `multi_file` parsing for `has_multiple_files`; the UI explains that RomM returns a multi-file ZIP and suggests extracting it as a local `.cue`/`.m3u` game. Download failures now surface the curl/HTTP detail. Installed homebrew is marked `Instalado`.
L6 (2026-11-17): Final verification: `make test` PASS; `./scripts/build-vpk.sh --clean` PASS; VPK sha256 `83cc286c9e8bf240501f0ced1d8a0c597e4a1c438cf5fb415adcc48cb4813d73`.
L7 (2026-11-17): Vita3K diagnosis: the generic fallback to `url` matched nested LaunchBox image metadata, so ROM `86` tried to download a JPEG instead of its RomM content. RomM URLs are now always synthesized from `id` + `fs_name`; duplicate `.part` cleanup was removed. Regression test, `make test`, and clean VPK build PASS; VPK sha256 `d8df58feb198a908eff633e69ab44ffa0a8a9054e92ba72b4cd524ee709fde67`.
