# Feature Specification: Timeline Overview Heatmap Widget

**Feature Branch**: `003-timeline-overview-heatmap`  
**Created**: 2026-06-20  
**Status**: Draft  
**Input**: User description: "Timeline overview widget showing an overview of the entire trace file using a heatmap/color scale to show message intensity across time. Shows CAN and Ethernet message rates. User-selectable data sources displayed as multiple narrow heatmap lanes."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Trace-wide activity overview at a glance (Priority: P1)

As a trace analyst, I want to see a compact heatmap overview of the entire trace duration showing message density over time, so that I can quickly identify busy and quiet periods without scrolling through the full timeline.

**Why this priority**: This is the core value proposition of the widget — a "minimap" of the entire trace that enables spatial awareness. Without this, the widget has no purpose.

**Independent Test**: Can be fully tested by loading a trace file and verifying that a horizontal heatmap strip appears above the main timeline, colored by message density. Delivers immediate visual insight into trace activity patterns.

**Acceptance Scenarios**:

1. **Given** a trace file is loaded, **When** the Overview or Notebook view is active, **Then** a compact heatmap widget appears above the main timeline showing message density across the full trace duration.
2. **Given** a trace with varying message rates (bursts and quiet periods), **When** viewing the heatmap, **Then** dense periods appear as saturated color and sparse periods appear as light/transparent.
3. **Given** a trace file is loaded, **When** the widget is resized horizontally, **Then** the heatmap re-renders with bins matched to the new pixel width, maintaining visual accuracy.

---

### User Story 2 - Multi-lane data source selection (Priority: P1)

As a trace analyst, I want to independently toggle CAN and Ethernet message rates as separate heatmap lanes, so that I can compare activity patterns across different bus systems.

**Why this priority**: Multiple narrow lanes for different protocols is the key differentiator from a simple activity bar. It enables cross-protocol correlation analysis.

**Independent Test**: Can be fully tested by checking/unchecking CAN and Ethernet checkboxes and verifying that corresponding lanes appear/disappear. Each lane shows its own protocol's message density.

**Acceptance Scenarios**:

1. **Given** a trace with both CAN and Ethernet messages, **When** both CAN and Ethernet checkboxes are checked, **Then** two separate narrow heatmap lanes are displayed, one per protocol.
2. **Given** both lanes are visible, **When** the user unchecks the Ethernet checkbox, **Then** the Ethernet lane disappears and the widget height shrinks to show only the CAN lane.
3. **Given** no checkboxes are checked, **When** no data sources are selected, **Then** the heatmap area is empty or shows a "Select a data source" hint.

---

### User Story 3 - Navigate via the overview (Priority: P2)

As a trace analyst, I want to click on the overview heatmap to navigate the main timeline to that position, so that I can quickly jump to interesting regions identified in the overview.

**Why this priority**: Interactivity transforms the overview from a passive display into an active navigation tool, but the visual overview (P1) delivers value even without click-to-navigate.

**Independent Test**: Can be fully tested by clicking at various positions on the heatmap and verifying the main timeline centres on the clicked timestamp.

**Acceptance Scenarios**:

1. **Given** a trace is loaded and the overview heatmap is displayed, **When** the user clicks at a position on the heatmap, **Then** the main timeline navigates to the corresponding timestamp.
2. **Given** the main timeline is zoomed into a specific time window, **When** viewing the overview, **Then** a semi-transparent overlay rectangle indicates the currently visible time window on the heatmap.
3. **Given** the visible-window overlay is shown, **When** the user zooms or scrolls the main timeline, **Then** the overlay rectangle updates to reflect the new visible window.

---

### User Story 4 - Hover for details (Priority: P3)

As a trace analyst, I want to see detailed information when hovering over a specific region of the heatmap, so that I can understand the exact message count and time position without navigating.

**Why this priority**: Tooltips enhance the experience but are not essential for the core overview functionality.

**Independent Test**: Can be fully tested by hovering over different bins and verifying the tooltip displays the correct protocol name, message count, and timestamp.

**Acceptance Scenarios**:

1. **Given** the heatmap is displayed with data, **When** the user hovers over a bin in the CAN lane, **Then** a tooltip appears showing "CAN: 1,245 msgs @ 00:02:14.000" (protocol, count, time).
2. **Given** the heatmap has empty bins (zero messages), **When** the user hovers over an empty bin, **Then** the tooltip shows the time position and "0 msgs".

---

### Edge Cases

- What happens when a trace contains only CAN messages and no Ethernet? → The Ethernet lane shows all-transparent bins. The Ethernet checkbox is still available but the lane is empty.
- What happens when a trace is extremely short (< 1 second)? → The heatmap still renders with all available bins. Minimum bin width is 1 pixel.
- What happens when a trace is extremely long (> 1 hour)? → Each pixel covers a larger time range. The tooltip still shows the bin's time range.
- What happens when the widget is very narrow (< 100px)? → The heatmap renders with fewer bins. Lane labels may be truncated but remain visible.
- What happens when the trace has no messages at all? → The heatmap shows empty lanes. A "No data" or similar indication could be shown.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST display a standalone `TimelineOverviewWidget` above the main TimelineWidget in both Overview and Notebook views.
- **FR-002**: System MUST render one horizontal heatmap lane per selected data source, using custom QPainter rendering in `paintEvent()`.
- **FR-003**: System MUST provide a checkbox row above the heatmap lanes, with one checkbox per available data source (CAN, Ethernet). Checking/unchecking a checkbox adds or removes the corresponding lane.
- **FR-004**: Each heatmap lane MUST use a per-lane single-hue colour gradient where zero/empty bins are transparent, low-intensity bins are lightly tinted, and high-intensity bins are fully saturated. CAN lanes use blue hues; Ethernet lanes use teal hues.
- **FR-005**: Colour intensity MUST be normalised per-lane — each lane independently maps its own [0, max_count] range to the full colour gradient.
- **FR-006**: Each lane MUST have a fixed height of approximately 16–20 pixels. The widget's total height adjusts dynamically as lanes are toggled on/off.
- **FR-007**: Each lane MUST display a small text label on the left edge (e.g. "CAN", "ETH") approximately 40px wide, identifying the protocol.
- **FR-008**: System MUST display a time axis above the heatmap lanes showing 5–8 evenly spaced time labels spanning the full trace duration.
- **FR-009**: System MUST show a semi-transparent overlay rectangle on the heatmap indicating the currently visible time window of the main timeline.
- **FR-010**: System MUST support click-to-navigate: clicking on the heatmap navigates the main timeline to the corresponding timestamp.
- **FR-011**: System MUST show a tooltip on hover displaying the protocol name, message count, and time position for the hovered bin.
- **FR-012**: The Analyzer backend MUST pre-compute a time-bucketed histogram during `buildIndex()`, using fixed-duration bins (e.g. 100ms). The histogram is stored as a map of protocol group → bin count array.
- **FR-013**: Protocol grouping: CAN lane aggregates BLF object types CAN_MESSAGE (1), CAN_MESSAGE2 (86), CAN_FD_MESSAGE (100), CAN_FD_MESSAGE_64 (101). Ethernet lane aggregates ETHERNET_FRAME (71), ETHERNET_FRAME_EX (120), ETHERNET_FRAME_FORWARDED (121).
- **FR-014**: The UI widget MUST downsample the Analyzer's fine-grained histogram to match the current pixel width at paint time. On resize, bins are merged (summed) without re-scanning the trace data.
- **FR-015**: The `TimelineOverviewWidget` MUST be a shared widget that is re-parented between OverviewView and NotebookView, following the same pattern as the existing shared `TimelineWidget`.

### Key Entities

- **TimelineOverviewWidget**: A QWidget subclass that owns the checkbox row, time axis, and heatmap rendering area. Shared across views via re-parenting.
- **Histogram Data**: Pre-computed during trace indexing. Maps a protocol group identifier to a vector of message counts at fixed time intervals. Stored in the Analyzer.
- **Protocol Group**: A named collection of BLF object types that are aggregated into a single heatmap lane (e.g. "CAN" groups types 1, 86, 100, 101).
- **Heatmap Lane**: A single narrow horizontal strip within the overview widget, representing one protocol group's message density over time.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: After loading a trace file, the heatmap overview renders within 500ms, displaying message density for all enabled data sources.
- **SC-002**: The heatmap visually distinguishes at least 5 levels of message density through colour intensity gradation.
- **SC-003**: Resizing the widget re-renders the heatmap without any visible delay (< 100ms) — no re-computation of underlying data, only pixel-bin downsampling.
- **SC-004**: Users can identify the busiest and quietest periods of a trace within 2 seconds of viewing the heatmap.
- **SC-005**: Click-to-navigate positions the main timeline at the clicked timestamp within 200ms.
- **SC-006**: Tooltip appears within 200ms of hovering over a bin and displays accurate data (protocol name, count, time).

## Assumptions

- The existing `Analyzer::buildIndex()` method can be extended to compute per-protocol histograms without significant performance impact on trace loading.
- The trace file contains meaningful timestamp data for all messages (i.e. `TraceMessage::timestampUs` is populated correctly).
- The main `TimelineWidget` will eventually expose a signal/slot interface for cursor position and visible window changes, enabling the visible-window overlay. For the initial implementation, the overlay may use placeholder positioning if the TimelineWidget's zoom state is not yet exposed.
- Bookmarks, anomalies, DoIP, and SOME/IP lanes are explicitly out of scope for this initial implementation and will be added as follow-up features.
- The 100ms histogram bin granularity is sufficient for overview purposes. Finer granularity can be adjusted later if needed.
- The widget's lane colours (CAN=blue, Ethernet=teal) are fixed for this version. A configurable colour scheme is out of scope.
