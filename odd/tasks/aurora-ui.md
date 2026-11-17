# VitaPlus Aurora
Objective: Cocoon-inspired presentation with custom FLUX assets, preserving the core/Vita frontend structure and controls.
Branch: `feat/aurora-ui`; delivery: local, uncommitted (no publishing authorized); runner: `make test`, asset-tool tests, Vita Docker build.

## Specs
- S1: "I would like to improve the UI 100x, making it look like a modern launcher. I like something like cocoon launcher maintining our FE structure." Preserve providers, catalog formats, PGF font and existing controls; redesign the existing screens for 960×544.
- S2: "Also let's use flux to build our own assets for all the icons, on the console \"folders\" let's also build our own icons using the native controller of each platform as inspiration." Generate 18 platform icons, 16 UI icons, a wallpaper and emblem; validate/downsize PNGs and bundle them in the VPK. Unknown platforms use a generic controller fallback.
- S3: "Let's improve every aspect of our UI. Use flux mcp to create all the assets needed." Cohesive library, ROM grid/list/carousel, settings, downloads and store placeholder; hero, dock, focus motion, readable copy and loading/empty/failure states.
- S4: "full set" and "do it" authorize the proposed local implementation and FLUX generation. Generated textures must remain bounded, failure-safe and independent of remote game artwork.

## Tasks
- [x] T1 / S2,S4 / inline (asset generation is one bounded independent operation) / All 36 FLUX PNGs generated, processed and recorded in provenance.tsv; no commit.
- [x] T2 / S1,S2,S4 / inline (frontend-only presentation helpers and asset preparation) / Observed RED (missing tool/helpers), then GREEN (Go conversion tests and C presentation tests); legacy themes, aliases, geometry and new keys covered; no commit.
- [x] T3 / S1,S3 / inline (shared renderer changes, no disjoint writer units) / All existing screens/views use ui_renderer.c; read-only frame, bounded textures, eased focus/dock, toast and progress; 11 host-rendered states pass; no commit.
- [ ] T4 / S1-S4 / inline checks; independent verification unavailable / Host tests, PNG checks, VPK build/contents and docs complete. Device PGF/GPU/performance and independent verification still pending; no commit.

## Log
- L1 (original request): "I would like to improve the UI 100x, making it look like a modern launcher. I like something like cocoon launcher maintining our FE structure. Also let's use flux to build our own assets for all the icons, on the console \"folders\" let's also build our own icons using the native controller of each platform as inspiration. Let's improve every aspect of our UI. Use flux mcp to create all the assets needed."
- L2: User selected "full set", then "do it". No commit, push, PR or remote host operations authorized.
- L3: Baseline 756427b; clean worktree. Existing core APIs, cache layout and worker ownership remain unchanged. PGF is the stable font path. Rendering-only helpers will be kept in `src/vita/` rather than changing persisted/catalog contracts.
- L4: Forecast exceeds 400 authored lines; keep coherent local edit surfaces, no PR or commit planned. Native review will be checked if tooling is available. No delegation tool is exposed in this runtime; do not claim independent verification.
- L5: FLUX previous plan confused generations with credits; actual per-image cost is unknown up front. Initial balance previously observed: 1,000 credits, 2 free images. Use one full set, no unbounded retries.
- L6: Completed 18 controller icons with FLUX 3 Image, 16 UI icons with FLUX 3 Image Klein, wallpaper and emblem with FLUX 3 Image. Endpoint allows 8 active requests; 6 rejected submissions were retried only after previous jobs completed. Final balance: 899.2 credits, 0 free images; paid cost 100.8 credits and 2 free generations. No paid variations/regenerations.
- L7: Presentation-only modules avoid changing VpTheme, VpItem or catalog cache layout. Optional ui_motion_ms, ui_radius and ui_wallpaper_alpha are parsed separately; legacy themes remain valid. Fixed geometry keeps D-pad columns unchanged. Carousel prefetch now requests actual previous/selected/next covers (including wraparound), matching rendering.
- L8: Observed RED for tool/helpers and optional theme keys before implementation. GREEN: make test, make test-assets, make test-renderer. Cairo harness exercised 11 screen states, long UTF-8 strings, model immutability, missing assets and texture cleanup; inspected platform/grid/RomM/download PNG previews. Cairo approximates PGF/tint and is not Vita GPU proof.
- L9: ./scripts/build-vpk.sh succeeded using cached vitasdk/vitasdk:2026.08; existing crtn.o GNU-stack linker warning only. dist/vitaplus.vpk SHA256 6c988bc178224aee8edd4b67c3d38d2ddc61bfd4e13283b52308cb172a3fb25e. All 36 packaged PNGs match source bytes; ZIP integrity OK. PNG total 425,510 bytes; nominal RGBA texture payload 2,029,568 bytes (GPU allocation can be higher).
- L10: Review mode is clone-local OFF; untouched. No commits, pushes or PRs. Risk: item 5 (VPK build/asset packaging); independent verifier unavailable in this runtime. Device/emulator acceptance remains in docs/aurora-ui.md. Deliver the local VPK for testing, not a claim of hardware validation.
- L11: Baseline/current portable-core stdout, stderr and exit status are identical. git diff confirms core/header/backend/core-tests are unchanged. Layout previews delivered under dist/previews/ (Cairo approximation). Temporary source images and test binaries can be recovered from FLUX IDs and were removed after conversion; pre-existing scratch files preserved.
