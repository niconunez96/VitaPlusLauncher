# Metadata cover synchronization

Objective: add a user-triggered, serialized bulk cover synchronization job for RomM.
Branch: `feat/metadata-cover-sync`. Delivery: one reviewable work unit; host test runner `make test` and imageproxy test runner `go test ./...`.

## Specs

- S1: Add another button labelled exactly `sincronizar metadata` that processes the catalog's cover art in bulk and in the background.
- S2: For each eligible RomM item, download its source image, send it to the image service, then save the returned PNG in `cache covers` without blocking the UI.
- S3: Address the reported race-condition concern: `algunas caratulas que son de otros sistemas estan pisando a otro`, preserving the association between an item ID, its cache file, and its displayed texture.

## Tasks

- T1 — S1-S3 — inline — completed — Mapped the serialized worker, cache naming, slot eviction, and test seams; evidence: `make test` and VPK build.
- T2 — S1-S3 — inline — completed — Implemented the bulk cover job, Settings control, status UI, and temporary-file cache promotion; commit pending.
- T3 — S1-S3 — inline — completed — `make test` passed and `scripts/build-vpk.sh` produced `dist/vitashelflauncher.vpk` (SHA-256 `58ff152be707a2d876566405f5b9f892dab54c9bd30b8a35139e09cc0da1537e`); commit pending.

## Log

- L1 — User request: `claro exacto quiero otro boton que diga "sincronizar metadata" y haga el trabajo de mandar la imagen a image service y luego descargarla en cache covers. Fijate tambien el tema de race condition porque vi que algunas caratulas que son de otros sistemas estan pisando a otro por ejemplo las caratulas de un juego de nintendo ds se esta mostrando en la caratula de un jeugo de sega.`
- L2 — Existing cover work is serialized through the sole `background_job`, so source-image staging is not concurrently written. Cache identity is derived from sanitized item IDs; the current slot cache has eight entries and falls back to evicting slot zero when every slot is visible. The reported cross-system artwork requires a dedicated ownership/eviction audit before implementation.
