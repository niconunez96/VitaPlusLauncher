# Diagnose cover-loading crashes on a physical Vita

The UI now waits for the previous GPU frame before preparing cover textures or
reusing temporary drawing memory. Cover loading and eviction happen before the
next drawing scene begins. This addresses a resource-lifetime hazard; it does
not establish the cause of every image-specific crash.

## Hardware check

1. Install a build containing this change. Keep the existing configuration and
   cover cache so the original failure remains comparable.
2. Open Arcade and wait at least 30 seconds without navigating.
3. Navigate through several pages, leave Arcade, and re-enter. Repeat with a
   catalog that previously worked.
4. If the app closes, copy `ux0:data/vitashelflauncher/covers.log` before reproducing again,
   along with the error screen or crash dump if available.

## Read the log

Each event includes process time, the catalog item ID, and the local cache path.
No source URLs, tokens, or image contents are logged. Events are closed to disk
individually; the log restarts once it reaches approximately 64 KiB.

| Event | Meaning |
| --- | --- |
| `load-start` | About to open and validate this cached cover. |
| `load-open-failed` | The cached cover could not be opened. |
| `load-size-rejected` | File-size probing failed or the file exceeds 512 KiB. |
| `load-header-rejected` | The header does not meet the supported PNG constraints. |
| `load-ok` | The PNG loader returned a texture; this does not prove GPU completion. |
| `load-failed` | The PNG loader returned no texture. |
| `evict` | A previous cover texture is about to be freed. |

A final `load-start` without a matching result is a candidate for investigation,
not proof that the named image is corrupt. Network failures are recorded
separately in `ux0:data/vitashelflauncher/network.log`.

## Local checks

Run `make test` (requires a C compiler and Python 3). The lifecycle tests enforce
source-level ordering; they do not emulate asynchronous GPU access or the native
PNG decoder. A VitaSDK build and the hardware check above are still required.
