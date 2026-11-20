---
name: vitashelf-vita-ui
description: Use when changing Vita rendering, controls, touch behavior, or target-only packaging.
---

# Vita UI guide

- Keep renderer work presentation-only; update app state through existing actions and jobs.
- Reuse controller actions for touch input so confirmation and safety behavior stay consistent.
- Keep geometry and touch hitboxes aligned, then cover boundaries with host tests.
- Avoid large automatic buffers in Vita paths; target stacks are constrained.
- After target-only changes, build a fresh VPK and inspect the artifact when assets or runtime files changed.
