# Implementation Plan: Anomaly Detection Framework

**Branch**: `006-anomaly-detection` | **Date**: 2026-06-29 | **Spec**: [spec.md](file:///home/munu/playground/fastrace/specs/006-anomaly-detection/spec.md)
**Input**: Feature specification from `/specs/006-anomaly-detection/spec.md`

## Summary

Implement a new anomaly detection framework for PDU, SOME/IP-SD, and DoIP protocols. The technical approach involves a `DetectionEngine` orchestrating a registry of `Detector` implementations. It introduces a `ProtocolParser` to handle Ethernet payload parsing and simple TCP reassembly, producing a `ProtocolMessage` tagged union. Detection runs in two phases (parallel stateless, sequential stateful) and integrates directly into the existing `DetectionsWidget` in the UI, adding a "Run Detectors" action to the `TopBarWidget`.

## Technical Context

**Language/Version**: C++23
**Primary Dependencies**: Qt6 (for UI)
**Storage**: N/A (in-memory detection results)
**Testing**: ctest
**Target Platform**: Linux, Mac, Windows
**Project Type**: Desktop UI application (`fastrace_ui`) and core backend library (`fastrace_analyzer`)
**Performance Goals**: Detect 1 million messages in under 10 seconds.
**Constraints**: Non-blocking UI; long-running operations must use `QtConcurrent` and support cancellation.
**Scale/Scope**: ~40 requirements, 3 new detectors, protocol parsing for SOME/IP and DoIP.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] Backend independent from Qt UI (`fastrace_analyzer` vs `fastrace_ui`).
- [x] Async/Cancellation supported via `std::atomic<bool>`.
- [x] Memory management uses smart pointers (`std::unique_ptr` for Detectors).

## Project Structure

### Documentation (this feature)

```text
specs/006-anomaly-detection/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
└── tasks.md             # (will be Phase 2 output)
```

### Source Code (repository root)

```text
cpp/
├── include/
│   ├── Detection.h
│   ├── Detector.h
│   ├── DetectionEngine.h
│   ├── ProtocolMessage.h
│   ├── ProtocolParser.h
│   ├── NetTypes.h           (modified)
│   ├── Analyzer.h           (modified)
│   └── detectors/
│       ├── PduDetector.h
│       ├── SomeIpSdDetector.h
│       └── DoipDetector.h
└── src/
    ├── DetectionEngine.cpp
    ├── ProtocolParser.cpp
    ├── Analyzer.cpp         (modified)
    └── detectors/
        ├── PduDetector.cpp
        ├── SomeIpSdDetector.cpp
        └── DoipDetector.cpp

ui/
├── include/
│   ├── DetectionsWidget.h   (modified)
│   └── TopBarWidget.h       (modified)
└── src/
    ├── DetectionsWidget.cpp (modified)
    ├── DetectionsWidget.ui  (modified)
    ├── TopBarWidget.cpp     (modified)
    └── MainWindow.cpp       (modified)
```

**Structure Decision**: The backend code follows the existing pattern, placing core framework files in `cpp/include/` and `cpp/src/`, while detector implementations are nicely separated in their own `detectors/` subdirectory. UI changes modify existing widgets.

## Complexity Tracking

N/A - Architecture follows existing chunk-processing and map-reduce patterns already used in the Analyzer for histograms.
