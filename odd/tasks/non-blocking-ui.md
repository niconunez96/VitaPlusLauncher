# Feature: non-blocking-ui

Objective: keep VitaPlus navigation responsive while remote work runs in the background, and store downloaded RomM ROMs under their platform.
Branch: feat/non-blocking-ui; delivery strategy: ask-on-risk; test runner: `make test` and `./scripts/build-vpk.sh --clean`.

## Specs

S1. "la app en general se \"bloquea\" mucho, pareciera como que se pone a hacer algo y se queda estatica."
Acceptance: rendering and controller navigation stay on the foreground loop; operations that can wait on I/O or do substantial work are scheduled in the background and expose a visible in-progress/completion/failure state.
Check: host state-machine tests, Vita build, and manual hardware navigation while work is active.

S2. "Mientras se sincroniza la biblioteca de romm se queda bloqueado"
Acceptance: RomM synchronization runs off the UI loop and atomically publishes its completed catalog to the UI.
Check: manual Vita sync while moving through the UI.

S3. "Cuando entro a una consola por ej \"snes\" se queda bloqueado por un par de segundos"
Acceptance: opening and browsing a platform never performs network or synchronous artwork transfers from the render/input loop.
Check: manual Vita platform navigation with uncached artwork.

S4. "Cuando empiezo a descargar un romm se queda bloqueado sin poder navegar"
Acceptance: RomM download and install transfers run in the background; users can keep navigating and can see the transfer state.
Check: manual Vita download while moving between screens and platform lists.

S5. "tratemos de agrupar los roms descargados por plataforma y no simplemente la romm en \"downloads\""
Acceptance: each downloaded RomM file is saved beneath `ux0:data/vitaplus/downloads/<safe-platform>/`, and existing downloaded-state detection uses that same path.
Check: core download-path tests and manual Vita download verification.

S6. "siempre deberiamos priorizar la navegabilidad de la UI sin bloquearla todo el trabajo se deberia mover al segundo plano"
Acceptance: expensive network, disk, decode, and catalog preparation work is never invoked from the foreground render/input path; only bounded state publication and drawing remain there.
Check: code review against foreground call paths plus manual hardware smoke test.

## Tasks

- [x] T1 - Add a Vita background-job state machine and foreground-safe status publication. Specs S1, S6. Route: inline (large tracked task; no parallel unit). Evidence: serialized pthread worker with immutable job inputs and foreground-only result publication. Commit: pending.
- [x] T2 - Move provider sync and deferred artwork loading out of the foreground UI loop. Specs S1-S3, S6. Route: inline (depends on T1). Evidence: provider sync and thumbnail transfers only execute in `background_job_run`; render queues at most one cover request. Commit: pending.
- [x] T3 - Move download/install transfers to the job system and group ROM paths by sanitized platform. Specs S1, S4-S6. Route: inline (depends on T1). Evidence: `make test` RED → GREEN for platform paths; ROM and homebrew transfers run through the worker. Commit: pending.
- [x] T4 - Add regression coverage and run host/Vita verification. Specs S1-S6. Route: inline. Evidence: `make test` PASS; `./scripts/build-vpk.sh --clean` PASS (VPK SHA-256 `172227660c60508a8a45dcd27352c5aa3dd65f85542c4ee7cf989ddbdf0cff45`). Manual physical-Vita smoke test pending. Commit: pending.

## Log

L1 (user, 2026-11-17): "Estuve probando vitaplus en una vita real y hay algo en general que creo que deberiamos modificar, la app en general se \"bloquea\" mucho, pareciera como que se pone a hacer algo y se queda estatica. Por ej:
1. Mientras se sincroniza la biblioteca de romm se queda bloqueado
2. Cuando entro a una consola por ej \"snes\" se queda bloqueado por un par de segundos
3. Cuando empiezo a descargar un romm se queda bloqueado sin poder navegar
4. Y asi con casi todas las interacciones

Aparte de ese problema de que la ui se bloquea constantemente tenemos que arreglar las descargas, tratemos de agrupar los roms descargados por plataforma y no simplemente la romm en \"downloads\""

L2 (user, 2026-11-17): "ok, volviendo a lo que estabamos hablando siempre deberiamos priorizar la navegabilidad de la UI sin bloquearla todo el trabajo se deberia mover al segundo plano"

L3 (2026-11-17): Current foreground paths synchronously call RomM fetches during startup/settings sync, transfer a ROM from `activate_selected`, and fetch/decode artwork from `draw_shell`. Platform grouping itself is CPU-bounded, but it repeatedly rebuilds visible platform/item lists during rendering. The implementation must make all unbounded I/O and decoding background work, publish completed results safely, and leave rendering/input foreground-only.

L4 (2026-11-17): Implementation plan: (1) introduce one serialized worker job with immutable inputs and mutex-protected terminal results; (2) schedule sync, transfers, and artwork fetches from foreground input/render code without executing them there; (3) apply completed catalog/item updates only on the foreground thread; (4) cache platform groups/visible indexes when catalog/navigation state changes; (5) store download paths below sanitized platform folders. The single worker avoids sharing the core response buffer or backend error state across concurrent transfers.
L5 (2026-11-17): T3 started with RED/GREEN coverage for platform-grouped download paths. `make test` first failed at `test_download_paths`, then passed after `vp_item_download_path` added the sanitized platform directory. Existing RomM transfer expectation was updated to the new path.
L6 (2026-11-17): A single pthread worker now owns all provider syncs, pair-code exchange, ROM transfers, VPK installs, and Libretro thumbnail HTTP transfers. The UI queues work and polls terminal results; it never calls a network backend in input/render paths. The catalog is published only after a completed sync, and lists are cached rather than re-sorted on every frame. Texture creation remains on the UI thread because Vita2D owns rendering resources there; network and file transfer no longer stall navigation.
L7 (2026-11-17): Verification: `make test` PASS and `./scripts/build-vpk.sh --clean` PASS. The VPK must still be smoke-tested on physical hardware while syncing, opening platforms, downloading, and switching screens.
