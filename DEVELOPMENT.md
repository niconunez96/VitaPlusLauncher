# Development

## Requirements

- `gcc`, `make`, Python 3, and Go for host checks.
- A local VitaSDK installation (default location: `~/.local/vitasdk`) or Docker to build the VPK.

## Test

```sh
make test
make test-assets
```

The host suite does not require VitaSDK or network access.

## Build a VPK

With a local VitaSDK (recommended):

```sh
./scripts/build-vpk-local.sh --clean
```

It defaults to `~/.local/vitasdk`; set `VITASDK` if your SDK is elsewhere. The artifact is `dist/vitashelflauncher.vpk`.

Docker alternative:

```sh
./scripts/build-vpk.sh --clean
```

For manual VitaSDK configuration:

```sh
cmake -S . -B build/vita -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake"
cmake --build build/vita
```

The direct-build artifact is `build/vita/vitashelflauncher.vpk`.

## Before committing

Install the versioned commit-message hook once:

```sh
./scripts/install-git-hooks.sh
```

Commit subjects must start with `PATCH`, `MINOR`, or `MAJOR`; CI enforces the same rule.
