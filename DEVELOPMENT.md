# Development

## Requirements

- `gcc`, `make`, Python 3, and Go for host checks.
- Docker **or** a local VitaSDK installation to build the VPK.

## Test

```sh
make test
make test-assets
```

The host suite does not require VitaSDK or network access.

## Build a VPK

Docker (recommended):

```sh
./scripts/build-vpk.sh
```

The artifact is `dist/vitashelflauncher.vpk`.

With VitaSDK installed:

```sh
cmake -S . -B build/vita -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake"
cmake --build build/vita
```

The artifact is `build/vita/vitashelflauncher.vpk`.

## Before committing

Install the versioned commit-message hook once:

```sh
./scripts/install-git-hooks.sh
```

Commit subjects must start with `PATCH`, `MINOR`, or `MAJOR`; CI enforces the same rule.
