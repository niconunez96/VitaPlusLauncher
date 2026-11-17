# Feature: libretro-thumbnails

Objective: use Libretro Thumbnails as VitaPlus's direct artwork source while rejecting unsafe image inputs before native decoding.
Branch: main; delivery strategy: single work unit; test runner: `make test` and `./scripts/build-vpk.sh --clean`.

## Specs

S1. "ok empecemos solo con libretro entonces nomas"
Acceptance: RomM remains the ROM catalog source; VitaPlus derives artwork URLs from the Libretro Thumbnails CDN rather than `cover_url` metadata.
Check: core URL-builder tests and manual Vita browse.

S2. "se activen automaticamente pero con la validacion si o si"
Acceptance: Artwork is enabled by default, but only validated Libretro thumbnail PNGs are decoded; rejected or missing images preserve library navigation without artwork.
Check: core validation tests, Vita build, manual Vita browse.

## Tasks

- [x] T1 - Add Libretro platform/title URL generation and strict PNG-header validation. Specs S1-S2. Route: inline (understood core behavior). Evidence: RED linker failure, then `make test` PASS.
- [x] T2 - Use validated Libretro artwork in the Vita frontend and update configuration/docs. Specs S1-S2. Route: inline (depends on T1). Evidence: Libretro-only cache path and validation before decode.
- [x] T3 - Run host and Vita build verification. Specs S1-S2. Route: inline. Evidence: `make test` PASS; `./scripts/build-vpk.sh --clean` PASS (`dist/vitaplus.vpk`, sha256 `463065888c5b3d4a11dcd0c38dea31442b0210f67e9a62bc29a158e056953fcb`).

## Log

L1 (user, 2026-11-17): "ok empecemos solo con libretro entonces nomas"
L2 (user, 2026-11-17): "se activen automaticamente pero con la validacion si o si"
L3 (2026-11-17): Libretro is now the only artwork source. The core maps common platform slugs and builds `Named_Boxarts` URLs using `fs_name` when available. The Vita frontend only decodes PNGs that pass a 512 KiB, 512×512, RGB/RGBA 8-bit, non-interlaced header check; invalid cache entries are removed. Physical-Vita behavior remains to be validated.
