# Implementation Plan: Timeline Overview Heatmap Widget

**Branch**: `003-timeline-overview-heatmap` | **Date**: 2026-06-20 | **Spec**: [spec.md](file:///home/munu/playground/fastrace/specs/003-timeline-overview-heatmap/spec.md)  
**Input**: Feature specification from `/specs/003-timeline-overview-heatmap/spec.md`

## Summary

Add a compact, interactive heatmap "minimap" widget (`TimelineOverviewWidget`) that displays message density across the entire trace duration. The widget shows multiple narrow horizontal lanes — one per protocol group (CAN, Ethernet) — with colour intensity encoding message rate. The backend Analyzer pre-computes per-protocol histograms during `buildIndex()`, and the UI widget downsamples these to pixel-width bins at paint time using custom QPainter rendering.

## Technical Context

**Language/Version**: C++20, Qt 6.x  
**Primary Dependencies**: Qt Widgets, Qt Concurrent (existing), fastrace_analyzer (existing)  
**Storage**: N/A (in-memory histogram computed during trace indexing)  
**Testing**: Manual visual verification (no automated test framework in project currently)  
**Target Platform**: Linux desktop (Qt6)  
**Project Type**: Desktop application  
**Performance Goals**: Heatmap renders in < 500ms after load; resize repaints < 100ms  
**Constraints**: No external libraries; QPainter-only rendering; minimal memory overhead for histogram  
**Scale/Scope**: Traces up to ~15 minutes / millions of messages

## Constitution Check

No constitution file found. No gates to check.

## Project Structure

### Documentation (this feature)

```text
specs/003-timeline-overview-heatmap/
├── spec.md              # Feature specification
├── plan.md              # This file
├── checklists/
│   └── requirements.md  # Quality checklist
└── tasks.md             # Task breakdown (to be generated)
```

### Source Code (repository root)

```text
# Backend changes (existing files)
cpp/
├── include/
│   └── Analyzer.h              # Add HistogramData struct, histogram getter
└── src/
    └── Analyzer.cpp            # Add histogram computation in buildIndex()

# UI changes
ui/
├── include/
│   ├── TimelineOverviewWidget.h  # NEW — widget header
│   ├── MainWindow.h              # Add m_timelineOverview member
│   ├── OverviewView.h            # Add overview widget parameter
│   └── NotebookView.h            # Add overview widget parameter
├── src/
│   ├── TimelineOverviewWidget.cpp # NEW — widget implementation
│   ├── MainWindow.cpp             # Wire up shared overview widget
│   ├── OverviewView.cpp           # Insert overview widget into layout
│   └── NotebookView.cpp           # Insert overview widget into layout
└── CMakeLists.txt                 # Add new source files
```

**Structure Decision**: This feature adds 2 new files (header + source for `TimelineOverviewWidget`) and modifies 6 existing files across the `cpp/` and `ui/` directories. No new directories needed.

## Architecture

### Component Interaction

```
┌─────────────────────────────────────────────────────┐
│  Analyzer::buildIndex()                              │
│  ├── Existing: chunkIndex_, totalMessages_           │
│  └── NEW: histogramData_ (per-protocol bin counts)   │
└──────────────────────┬──────────────────────────────┘
                       │ shared_ptr<Analyzer>
                       ▼
┌─────────────────────────────────────────────────────┐
│  TimelineOverviewWidget (shared, re-parentable)      │
│  ├── Checkbox row (CAN ☑, Ethernet ☑)               │
│  ├── Time axis (above lanes)                         │
│  ├── Heatmap lanes (QPainter paintEvent)             │
│  │   ├── Lane "CAN"  [label | ████████████████]     │
│  │   └── Lane "ETH"  [label | ████████████████]     │
│  ├── Visible-window overlay (translucent rect)       │
│  └── Tooltip on hover                                │
└──────────────────────┬──────────────────────────────┘
                       │ re-parented on view switch
            ┌──────────┴──────────┐
            ▼                     ▼
    OverviewView            NotebookView
```

### Data Flow

1. `Analyzer::buildIndex()` iterates messages and increments per-protocol counters in fixed 100ms bins
2. Histogram stored as `std::unordered_map<ProtocolGroup, std::vector<uint32_t>>`
3. `MainWindow::onLoadFinished()` passes the `shared_ptr<Analyzer>` to `TimelineOverviewWidget::attachAnalyzer()`
4. On paint, the widget reads the fine-grained histogram and downsamples to pixel-width bins
5. Colour intensity computed per-lane: `intensity = count / max_count_in_lane`

### Protocol Group Mapping

| Protocol Group | BLF Object Types |
|---|---|
| CAN | CAN_MESSAGE (1), CAN_MESSAGE2 (86), CAN_FD_MESSAGE (100), CAN_FD_MESSAGE_64 (101) |
| Ethernet | ETHERNET_FRAME (71), ETHERNET_FRAME_EX (120), ETHERNET_FRAME_FORWARDED (121) |

### Key Design Decisions

1. **Histogram bin size: 100ms** — Provides ~10 bins/second. For a 15-minute trace, that's 9,000 bins per protocol. At 4 bytes each, this is ~72KB total — negligible memory overhead.

2. **Downsampling strategy**: Multiple fine-grained bins map to one pixel. Sum the counts in the merged bins. This preserves total message count per visual bin.

3. **Re-parenting pattern**: Follows the existing `TimelineWidget` pattern — the `MainWindow` owns the widget instance and passes it to views, which re-parent it during `activate()`/`deactivate()`.

4. **Colour computation**: `QColor::fromHslF(hue, saturation, lightness)` where hue is fixed per lane and lightness varies with intensity (light for low, dark for high).

## Complexity Tracking

No constitution violations to justify.
