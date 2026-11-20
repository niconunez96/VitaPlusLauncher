---
name: vitashelf-testing
description: Use when adding, changing, or verifying behavior in VitaShelf Launcher.
---

# Testing guide

- Read the closest test before adding a new one.
- Prefer host tests: they are deterministic and do not need VitaSDK or network access.
- Test observable behavior and boundary conditions rather than implementation details.
- Run the narrowest relevant target first; use `make test` for the host suite and `make test-assets` for asset tooling.
- A Vita SDK build validates packaging and linkage; it does not replace host regressions.
