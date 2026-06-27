# Implementation Plan: Analyzer Reuse & Trace Loading UI Disable

**Branch**: `005-analyzer-reuse` | **Date**: 2026-06-27 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/005-analyzer-reuse/spec.md`

## Summary

The feature prevents invalid user interactions by disabling the trace loading combo box while an asynchronous load is ongoing. Furthermore, to avoid invalid state and dangling pointers across UI widgets, it modifies the backend trace loading flow to reuse the existing `Analyzer` object by clearing its state in-place, rather than creating a new instance.

## Technical Context

**Language/Version**: C++23, Qt6
**Primary Dependencies**: Standard library, Qt Widgets
**Storage**: N/A (in-memory)
**Testing**: Manual testing based on acceptance criteria (no test suite provided for the UI)
**Target Platform**: Linux Desktop (Fastrace UI)
**Project Type**: Desktop UI + C++ Backend Library
**Performance Goals**: N/A (UI state management)
**Constraints**: Must not block the UI thread during reset.
**Scale/Scope**: Impacts `Analyzer` state cleanup and `TopBarWidget` UI toggling.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **Non-blocking UI**: The reset operation will be executed on the UI thread, but it only clears in-memory standard library containers (`std::vector::clear()`, etc.), which is O(N) for elements with non-trivial destructors, but for trivially destructible types or pre-allocated vectors, it's fast. We ensure `mf_` (memory mapped file) cleanup doesn't block noticeably. (Passes)
- **Modularity**: Separation of concern is maintained. UI code toggles widgets, Analyzer manages its own data reset. (Passes)

## Project Structure

### Documentation (this feature)

```text
specs/005-analyzer-reuse/
├── plan.md              # This file
├── research.md          # Phase 0 output
└── data-model.md        # Phase 1 output
```

### Source Code (repository root)

```text
cpp/
├── include/
│   └── Analyzer.h       # Modified to add reset()
└── src/
    └── Analyzer.cpp     # Modified to implement reset()

ui/
├── include/
│   └── TopBarWidget.h   # Modified to add setTraceComboEnabled
└── src/
    ├── TopBarWidget.cpp # Modified to implement setTraceComboEnabled
    └── MainWindow.cpp   # Modified to use reset() and disable UI during load
```

**Structure Decision**: Code modifications are strictly confined to the existing files mapped above. No structural changes are needed.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No violations.
