# RomM Authenticated Catalog and Transfers Implementation Plan

> **For Hermes:** Use spec-drive-development skill to implement this plan task-by-task.

**Goal:** Make VitaPlus use the configured RomM Bearer token for every protected RomM request, reject non-success HTTP responses, and import the paginated catalog without silently truncating it.

**Architecture:** Keep the core transport-agnostic. Extend the transfer contract to accept an optional bearer token, carry the RomM token only at RomM call sites, and make the Vita libcurl adapter construct the real `Authorization: Bearer <token>` header. The core will request bounded RomM pages until the API reports that the catalog is exhausted or `VP_MAX_ITEMS` is reached.

**Tech Stack:** C11 core tests with `make test`; Vita libcurl adapter; RomM REST API.

**Verified context:** The development-profile secret handoff is readable and has both required variables. A real authenticated request to `{ROMM_BASE_URL}/api/roms?limit=1&offset=0` returned HTTP 200 JSON with `items`, `total`, `limit`, and `offset`. No secret values were emitted. Current Vita code mistakenly formats `Authorization: Bearer ***` literally and transfer calls have no bearer-token parameter.

---

### Task 1: Specify pagination and authenticated-transfer contracts in tests

**Objective:** Make the missing required behavior executable before modifying production code.

**Files:**
- Modify: `tests/test_core.c`
- Modify: `include/vitaplus.h` only after the RED test identifies the public signature delta

**Step 1: Write failing tests**

Add fake backend state that can return multiple RomM JSON pages and records each request URL plus each transfer bearer token. Cover:

1. `vp_romm_sync()` requests sequential `limit`/`offset` pages until either the remote `total` is reached or the fixed catalog capacity is reached.
2. Each catalog fetch receives `config.token`.
3. `vp_cache_cover()` and `vp_queue_download()` pass the configured RomM token to the transfer backend for RomM items.
4. Homebrew installation passes an empty token.

Use a small page size fixture (for example two pages of one item) so request order is deterministic. Keep fake response parsing free of production-only hooks.

**Step 2: Run tests to verify RED**

Run: `make test`

Expected: compile failure from the new transfer/pagination contract or an assertion failure showing only the first page/empty transfer credential is used.

**Step 3: Commit after a coherent red milestone**

```bash
git add tests/test_core.c
git commit -m "test: cover RomM authenticated paging and transfers"
```

### Task 2: Implement core pagination and token propagation

**Objective:** Preserve the transport abstraction while ensuring all RomM-originated traffic receives the token.

**Files:**
- Modify: `include/vitaplus.h`
- Modify: `src/core/vitaplus.c`
- Test: `tests/test_core.c`

**Step 1: Update the public transfer contract minimally**

Change `VpTransferFn` to accept `const char *bearer_token` between URL and destination. Update `vp_cache_cover()` and `vp_queue_download()` to receive the token at their public call sites. Pass `""` for `vp_install_homebrew()`.

**Step 2: Implement bounded RomM pagination**

Refactor `vp_romm_sync()` to:

1. Build `GET /api/roms?limit=<page-size>&offset=<offset>` requests.
2. Fetch each page through the existing `VpFetchBackend` with `config->token`.
3. Import the page and normalize that page's relative cover/download URLs before the next request.
4. Read the JSON `total` only as a bounded stop signal; stop when fewer than a full page imports, `total` is reached, or `VP_MAX_ITEMS` is reached.
5. Return `false` if the first page cannot be fetched/imported; return success after at least one page imports.

Use a page size and response capacity that are safe for the existing fixed-buffer architecture. Do not allocate an unbounded remote response or grow the public API beyond this requirement.

**Step 3: Run targeted tests to verify GREEN**

Run: `make test`

Expected: `vitaplus core tests: PASS`.

**Step 4: Commit**

```bash
git add include/vitaplus.h src/core/vitaplus.c tests/test_core.c
git commit -m "feat: page RomM catalog and propagate transfer tokens"
```

### Task 3: Correct Vita libcurl authentication and HTTP status handling

**Objective:** Make the target-side adapter send the actual token and report HTTP failures rather than accepting error documents as successful data/files.

**Files:**
- Modify: `src/vita/vita_backend.h`
- Modify: `src/vita/vita_backend.c`
- Modify: `src/vita/main.c`
- Test: `tests/test_core.c` for contract call plumbing; target adapter compilation where VitaSDK is available

**Step 1: Update adapter calls for the new contract**

Change `vp_vita_transfer()` to accept `const char *bearer_token`; update the `VpTransferBackend` construction/calls in `src/vita/main.c` so the active RomM configuration token is passed for cover/download operations and the homebrew path remains unauthenticated.

**Step 2: Implement the minimal safe curl behavior**

- Format `Authorization: Bearer %s` using the supplied token; never log it.
- Attach the header only when non-empty.
- Set `CURLOPT_FAILONERROR` (or explicitly query `CURLINFO_RESPONSE_CODE`) for both fetch and transfer paths so 4xx/5xx responses return `false`.
- Keep TLS verification and redirects enabled.
- Remove a partial destination file when transfer fails if the target filesystem API supports it in the existing code style.

**Step 3: Run tests/build checks**

Run: `make test`

Expected: `vitaplus core tests: PASS`.

If `VITASDK` is configured, run the configured CMake/Vita target build. Otherwise record that the host only configures `vitaplus_core` and do not claim a Vita binary build.

**Step 4: Commit**

```bash
git add src/vita/vita_backend.h src/vita/vita_backend.c src/vita/main.c
git commit -m "fix: authenticate RomM transfers in Vita backend"
```

### Task 4: Update the provider contract and prove the real service seam

**Objective:** Keep the docs aligned and perform a secret-safe final integration proof.

**Files:**
- Modify: `docs/providers.md`

**Step 1: Document the behavior**

State that RomM catalog, cover, and ROM-download requests use the configured bearer token, pagination is bounded by `VP_MAX_ITEMS`, and HTTP 4xx/5xx counts as a failed backend operation.

**Step 2: Verify from the active worktree**

Run:

```bash
make test
git diff --check
```

Then perform the already-established authenticated real-service probe without printing `ROMM_BASE_URL`, `ROMM_TOKEN`, or catalog contents. Report only: credential presence, HTTP status, JSON validity, response shape, and count metadata.

**Step 3: Fresh-context review**

Review the diff against this plan with two lenses: contract/spec compliance and C/security quality. Pay particular attention to token leakage in logs, literal redaction in HTTP headers, pagination stop conditions, fixed capacities, and partial downloads.

**Step 4: Commit docs/final cohesive changes**

```bash
git add docs/providers.md
git commit -m "docs: clarify authenticated RomM provider behavior"
```

## Risks and non-goals

- This does not add UI rendering or caching policy for downloaded cover artwork; it makes the existing cache/download seam authenticated and correct.
- This does not make the catalog dynamically sized beyond `VP_MAX_ITEMS`.
- Real device/VitaSDK verification may be unavailable in this host environment; host core tests and the authenticated HTTP probe are the minimum required evidence.
- No token, URL, catalog title, or download URL may appear in test logs, commits, or the final report.
