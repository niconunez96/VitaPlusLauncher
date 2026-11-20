# VitaShelf Launcher: agent guide

Start with the smallest change that fits the existing code. Source and focused tests are the truth; do not add design or iteration documents to the repository.

## Repository map

- `src/core/`: portable domain and provider logic.
- `src/vita/`: Vita2D adapter, input, storage, networking, and rendering.
- `include/`: public portable API.
- `tests/`: host-native regression tests and static contract checks.
- `assets/` and `themes/`: shipped resources; retain licenses and provenance.
- `scripts/`: build, assets, and release helpers.

## Route by task

| Task | Load first |
| --- | --- |
| Boundaries, state, providers, persistence | `.agents/skills/architecture/SKILL.md` |
| C style, changes to shipped resources or config | `.agents/skills/conventions/SKILL.md` |
| Any behavior change or regression | `.agents/skills/testing/SKILL.md` |
| Vita UI, controls, rendering, target build | `.agents/skills/vita-ui/SKILL.md` |

Load every matching skill before editing. Keep skills as guides, not copies of source code.

## Working agreement

1. Read adjacent code and its existing tests before changing behavior.
2. Add or adjust the narrowest regression test when practical.
3. Keep portable behavior in `src/core`; use `src/vita` for platform integration.
4. Never log or expose RomM tokens or pair codes. Do not add credentials, local config, generated builds, captures, or planning files to Git.
5. Run the relevant test target and inspect `git diff --check` before finishing.

## Common commands

```sh
make test
make test-assets
./scripts/build-vpk.sh
```

Use `DEVELOPMENT.md` for the concise human workflow and `README.md` for end users.
