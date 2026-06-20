# Tasks: Timeline Overview Heatmap Widget

**Input**: Design documents from `/specs/003-timeline-overview-heatmap/`  
**Prerequisites**: plan.md (required), spec.md (required for user stories)

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2)
- Include exact file paths in descriptions

---

## Phase 1: Setup

**Purpose**: Register the new widget in the build system and create skeleton files.

- [x] T001 [P] Create skeleton `TimelineOverviewWidget` header in `ui/include/TimelineOverviewWidget.h` — declare the class inheriting `QWidget` with stub `paintEvent`, `attachAnalyzer()`, `activate()`/`deactivate()` methods, and signal/slot declarations.
- [x] T002 [P] Create skeleton `TimelineOverviewWidget` source in `ui/src/TimelineOverviewWidget.cpp` — implement the constructor (creates checkbox row layout, sets minimum height), stub `paintEvent`, and stub `attachAnalyzer`.
- [x] T003 Add `TimelineOverviewWidget.cpp` and `TimelineOverviewWidget.h` to `ui/CMakeLists.txt` in the source list. Verify the project compiles.

---

## Phase 2: Backend Histogram (Blocking Prerequisite)

**Purpose**: Extend `Analyzer` to pre-compute per-protocol time-bucketed histograms during `buildIndex()`. This MUST be complete before any UI rendering work.

**⚠️ CRITICAL**: The UI widget cannot render meaningful data until this phase is complete.

- [x] T004 Define `ProtocolGroup` enum and `HistogramData` struct in `cpp/include/Analyzer.h`:
  - `enum class ProtocolGroup : uint8_t { CAN, Ethernet, COUNT };`
  - `struct HistogramData { uint64_t binWidthUs = 100'000; uint64_t traceStartUs = 0; uint64_t traceEndUs = 0; std::array<std::vector<uint32_t>, static_cast<size_t>(ProtocolGroup::COUNT)> bins; };`
  - Add `HistogramData histogram_` member and `const HistogramData& histogram() const` getter to `Analyzer`.

- [x] T005 Implement histogram computation in `Analyzer::buildIndex()` (`cpp/src/Analyzer.cpp`):
  - After scanning all containers, determine `traceStartUs` and `traceEndUs` from the first and last message timestamps.
  - Compute `numBins = (traceEndUs - traceStartUs) / binWidthUs + 1`.
  - Resize each protocol group's bin vector to `numBins`, zero-filled.
  - During the message iteration loop (or in a second pass), for each message: map `objectType` → `ProtocolGroup` (using a helper function `protocolGroupOf(uint32_t objectType)`), compute `binIndex = (timestampUs - traceStartUs) / binWidthUs`, and increment `bins[group][binIndex]`.
  - The mapping function: types {1, 86, 100, 101} → CAN; types {71, 120, 121} → Ethernet; all others → skip (no group).

**Checkpoint**: `Analyzer::buildIndex()` now populates `histogram_` with per-protocol bin counts. Can be verified by printing bin arrays after loading a test trace.

---

## Phase 3: User Story 1 — Trace-wide activity overview (Priority: P1) 🎯 MVP

**Goal**: Display a compact heatmap overview of the entire trace duration showing message density per protocol lane.

**Independent Test**: Load a trace file → verify heatmap lanes appear above the main timeline with colour intensity reflecting message density.

### Implementation for User Story 1

- [x] T006 [US1] Implement the checkbox row in `TimelineOverviewWidget` constructor (`ui/src/TimelineOverviewWidget.cpp`):
  - Create a `QHBoxLayout` at the top of the widget.
  - Add a `QLabel` "OVERVIEW" as header.
  - Add `QCheckBox` for "CAN" (checked by default) and "Ethernet" (checked by default).
  - Connect checkbox `toggled(bool)` signals to a slot `onLaneToggled()` that stores which lanes are enabled and calls `update()` (repaint).

- [x] T007 [US1] Implement `attachAnalyzer(std::shared_ptr<fastrace::Analyzer>)` in `TimelineOverviewWidget`:
  - Store the `shared_ptr<Analyzer>`.
  - Read `histogram()` from the analyzer.
  - Cache the downsampled display bins (call a private method `rebuildDisplayBins()`).
  - Call `update()` to trigger repaint.

- [x] T008 [US1] Implement `rebuildDisplayBins()` private method in `TimelineOverviewWidget`:
  - For each enabled protocol group, take the fine-grained histogram bins and downsample to match the current paint area width in pixels.
  - Downsampling: if N fine bins map to 1 pixel, sum their counts.
  - Store the result in a member `std::array<std::vector<uint32_t>, ...> m_displayBins`.
  - Also compute per-lane `m_maxCount` for normalisation.

- [x] T009 [US1] Implement the time axis rendering in `paintEvent()`:
  - Compute the paint area (below checkbox row, full width minus label margin).
  - Draw 5–8 evenly spaced time labels above the heatmap lanes area.
  - Format time as `HH:MM:SS` or `MM:SS.mmm` depending on trace duration.
  - Use `QPainter::drawText()`.

- [x] T010 [US1] Implement the heatmap lane rendering in `paintEvent()`:
  - For each enabled lane, draw a row of filled rectangles (one per pixel-bin).
  - Colour: `QColor::fromHslF(hue, 0.7, lerp(0.95, 0.25, intensity))` where `intensity = count / maxCount`.
  - Lane height: 18px. 1px gap between lanes.
  - Draw lane label ("CAN" / "ETH") on the left edge, 40px wide.
  - Call `rebuildDisplayBins()` on `resizeEvent()` to recompute on resize.

- [x] T011 [US1] Wire `TimelineOverviewWidget` into `MainWindow` (`ui/include/MainWindow.h` and `ui/src/MainWindow.cpp`):
  - Add `TimelineOverviewWidget* m_timelineOverview` member.
  - Construct it in `MainWindow()` constructor.
  - Pass it to both `OverviewView` and `NotebookView` constructors (as a shared widget, same as `m_timeline`).
  - In `onLoadFinished()`, call `m_timelineOverview->attachAnalyzer(m_analyzer)`.

- [x] T012 [US1] Update `OverviewView` to accept and re-parent `TimelineOverviewWidget` (`ui/include/OverviewView.h` and `ui/src/OverviewView.cpp`):
  - Add `TimelineOverviewWidget*` parameter to constructor.
  - In `activate()`, insert the overview widget at index 0 of `m_centreSplitter` (above the timeline).
  - In `deactivate()`, detach the overview widget.

- [x] T013 [US1] Update `NotebookView` to accept and re-parent `TimelineOverviewWidget` (`ui/include/NotebookView.h` and `ui/src/NotebookView.cpp`):
  - Add `TimelineOverviewWidget*` parameter to constructor.
  - In `activate()`, insert the overview widget at the top of the notebook layout.
  - In `deactivate()`, detach the overview widget.

**Checkpoint**: After loading a trace, the heatmap overview appears at the top of both Overview and Notebook views, showing CAN and Ethernet density lanes with proper colour gradients.

---

## Phase 4: User Story 3 — Navigate via the overview (Priority: P2)

**Goal**: Enable click-to-navigate and display a visible-window indicator on the heatmap.

**Independent Test**: Click on different positions on the heatmap → verify the main timeline navigates to the clicked timestamp.

### Implementation for User Story 3

- [x] T014 [US3] Implement `mousePressEvent(QMouseEvent*)` in `TimelineOverviewWidget`:
  - Map the click x-coordinate to a timestamp using the trace start/end and paint area width.
  - Emit a signal `navigateRequested(uint64_t timestampUs)`.

- [x] T015 [US3] Connect `navigateRequested` signal in `MainWindow`:
  - Connect `m_timelineOverview->navigateRequested` to a slot that updates the main timeline cursor position.
  - (Note: the main `TimelineWidget` may not yet have a `setCursor` method — if not, add a stub or placeholder connection that can be completed when the TimelineWidget is fully implemented.)

- [x] T016 [US3] Implement visible-window overlay rendering in `paintEvent()`:
  - Draw a semi-transparent rectangle (e.g. `QColor(255,255,255,80)` with a 1px border) over the heatmap area corresponding to the currently visible time window.
  - Add `setVisibleWindow(uint64_t startUs, uint64_t endUs)` public method that stores the window bounds and calls `update()`.
  - (Note: actual integration with the main timeline's zoom state is deferred until `TimelineWidget` exposes its visible range.)

**Checkpoint**: Clicking on the heatmap emits a navigation signal. The visible-window overlay renders at the correct position when `setVisibleWindow()` is called.

---

## Phase 5: User Story 4 — Hover for details (Priority: P3)

**Goal**: Show a tooltip on hover with protocol name, message count, and time position.

**Independent Test**: Hover over different bins → verify tooltip displays correct information.

### Implementation for User Story 4

- [x] T017 [US4] Implement `mouseMoveEvent(QMouseEvent*)` in `TimelineOverviewWidget`:
  - Enable mouse tracking (`setMouseTracking(true)` in constructor).
  - On mouse move, determine which lane and which bin the cursor is over.
  - Compute the timestamp from the x-coordinate.
  - Look up the message count for that bin and lane.
  - Call `QToolTip::showText()` with formatted text: e.g. "CAN: 1,245 msgs @ 00:02:14.000".
  - If the cursor is outside any lane, hide the tooltip.

**Checkpoint**: Hovering over any bin shows an informative tooltip. Moving off the heatmap hides it.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Visual refinement and edge case handling.

- [x] T018 [P] Handle edge case: empty trace (no messages) — show an empty heatmap area with a "No data" label.
- [x] T019 [P] Handle edge case: trace with only one protocol — ensure single-lane rendering works correctly.
- [x] T020 [P] Handle edge case: very short trace (< 1 second) — ensure heatmap renders without division-by-zero or visual artifacts.
- [x] T021 Verify that view switching (Overview ↔ Notebook) correctly re-parents the `TimelineOverviewWidget` without visual glitches or data loss.
- [x] T022 Test with a real trace file to verify visual accuracy and performance.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately.
- **Backend Histogram (Phase 2)**: Depends on Phase 1 (T003 specifically for compilation). BLOCKS all UI rendering work.
- **User Story 1 (Phase 3)**: Depends on Phase 2 completion. This is the MVP.
- **User Story 3 (Phase 4)**: Depends on Phase 3 (T010 for paint infrastructure). Can start after T010.
- **User Story 4 (Phase 5)**: Depends on Phase 3 (T010 for paint infrastructure). Can start after T010.
- **Polish (Phase 6)**: Depends on all prior phases.

### Parallel Opportunities

- T001 and T002 can run in parallel (different files).
- T006, T009, T010 are sequential (build on each other within the widget).
- T011, T012, T013 can partially overlap (different files) but T011 should land first to define the shared widget.
- T014 and T017 are independent of each other (Phase 4 and Phase 5 can run in parallel).
- All Phase 6 tasks marked [P] can run in parallel.

### Critical Path

```
T001/T002 → T003 → T004 → T005 → T006 → T007 → T008 → T010 → T011 → T012/T013 → T022
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (skeleton files, build system)
2. Complete Phase 2: Backend histogram (Analyzer changes)
3. Complete Phase 3: US1 (heatmap rendering, wiring into views)
4. **STOP and VALIDATE**: Load a trace, verify heatmap appears with correct colouring
5. Demonstrate and gather feedback

### Incremental Delivery

1. Setup + Backend → Foundation ready
2. Add US1 (heatmap display) → Test visually → Demo (MVP!)
3. Add US3 (click-to-navigate) → Test interactivity → Demo
4. Add US4 (hover tooltips) → Test hover feedback → Demo
5. Polish → Final review

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- The existing `TimelineWidget` is still a placeholder — click-to-navigate (T015) may need stubs until that widget is fully implemented
- Bookmark lane support is explicitly out of scope; the checkbox framework is designed to easily add new lanes later
- Commit after each task or logical group
