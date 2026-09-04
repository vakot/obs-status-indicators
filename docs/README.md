# OBS Status Indicators — MVP investigation

Date: 2026-09-04

## Verdict: POSSIBLE, with a capture-exclusion qualification

The MVP is technically feasible as a native Windows OBS plugin:

```text
OBS frontend/source/output APIs
        ↓
small state provider and indicator controller
        ↓
native Win32 overlay owned by the plugin
```

The design can provide recording, pause, Replay Buffer, microphone, and transient saving indicators without Python, an external process, IPC, scene items, or OBS output rendering.

The qualification is important: `SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)` is a Windows capture hint/capability, not a universal DRM guarantee. It must be verified independently with Display Capture, Window Capture, and Game Capture. If the requirement is “never visible in every possible capture implementation or exclusive fullscreen mode,” that requirement is **NOT-POSSIBLE to guarantee** with the supported Windows API alone. If the requirement is the planned best-effort exclusion on supported Windows capture paths, the MVP remains **POSSIBLE**.

## Recommended implementation

- Build from the official OBS plugin-template conventions with `libobs` and `obs-frontend-api`.
- Use frontend callbacks and direct queries for recording, pause, and Replay Buffer.
- Obtain a referenced Replay Buffer output and subscribe to its `saved()` signal for the transient saving indicator.
- Resolve the configured microphone as an OBS source, subscribe to source signals, and keep a weak/reference-safe lifetime model.
- Use one state snapshot and one complete layout computation per transition.
- Render a compact top-level Win32 layered window with GDI/`UpdateLayeredWindow` initially; do not add Qt or another rendering framework for the POC.
- Keep OBS callbacks separate from the Win32 message/render thread and dispatch a copied state snapshot to the overlay thread.
- Apply `WDA_EXCLUDEFROMCAPTURE` after window creation, log failure, and continue without making overlay startup fatal.

## Focused documents

- [Development plan](01-development-plan.md) — phased implementation and investigation sequence.
- [Technical architecture](02-technical-architecture.md) — OBS API mapping, state model, lifecycle, and boundaries.
- [Windows overlay](03-windows-overlay.md) — window styles, rendering, positioning, threading, and capture limits.
- [Runtime findings](04-runtime-findings.md) — evidence collected from the supplied `obs-dev` installation.
- [Test matrix](05-test-matrix.md) — automated, manual, and capture acceptance criteria.
- [Development conventions](06-development-conventions.md) — naming, ownership, logging, error handling, and build conventions.
- [Sources and assumptions](07-sources-and-assumptions.md) — primary references, decisions, and unresolved questions.

## Current repository state

Phases 0-7 are implemented or experimentally resolved on merged master. The plan is complete for the documented native Windows MVP scope; remaining work is limited to future product expansion or broader hardware/capture coverage.

Phase 1 implementation evidence is recorded in [08-phase-1-native-plugin.md](08-phase-1-native-plugin.md).

Phase 2 implementation evidence is recorded in [09-phase-2-state-provider.md](09-phase-2-state-provider.md).

Phase 3 implementation evidence is recorded in [10-phase-3-win32-overlay.md](10-phase-3-win32-overlay.md).

Phase 4 implementation evidence is recorded in [11-phase-4-indicator-controller.md](11-phase-4-indicator-controller.md).

Phase 5 implementation evidence is recorded in [12-phase-5-microphone-saving.md](12-phase-5-microphone-saving.md).

Phase 6 capture evidence is recorded in [13-phase-6-capture-matrix.md](13-phase-6-capture-matrix.md).

Phase 7 release-readiness evidence is recorded in [14-phase-7-stability-packaging.md](14-phase-7-stability-packaging.md).

The complete reproducible operator flow is recorded in [15-local-test-flow.md](15-local-test-flow.md).
