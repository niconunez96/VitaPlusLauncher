---
name: vitashelf-conventions
description: Use when editing C code, configuration, assets, themes, or repository hygiene.
---

# Conventions guide

- Follow the style and naming of the file being changed; compile with warnings as errors.
- Keep configuration backward compatible and do not commit real `config.ini` files or credentials.
- Update packaged-resource declarations in `CMakeLists.txt` when adding a runtime Vita asset.
- Build target artifacts with `./scripts/build-vpk-local.sh --clean`; it defaults to the local VitaSDK at `~/.local/vitasdk` and accepts `VITASDK` for another location.
- Preserve third-party licenses and provenance alongside shipped assets.
- Keep generated output, IDE state, local captures, and task notes out of Git.
