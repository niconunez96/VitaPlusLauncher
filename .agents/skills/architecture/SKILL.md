---
name: vitashelf-architecture
description: Use when changing domain state, providers, jobs, persistence, or platform boundaries.
---

# Architecture guide

- Keep domain rules and normalized catalog behavior portable in `src/core`.
- Put Vita2D, Vita SDK calls, filesystem paths, curl transport, threads, and controller integration in `src/vita`.
- Preserve `VpItem` and backend contracts unless the task requires an intentional compatible migration.
- Treat network work as asynchronous; the render/input path must stay responsive.
- Keep secrets transient where possible and never expose them in UI, logs, fixtures, or docs.
