# Feature Specification: Anomaly Detection Framework

**Feature Branch**: `006-anomaly-detection`  
**Created**: 2026-06-29  
**Status**: Draft  
**Input**: User description: "Anomaly detection framework for PDU, SOME/IP-SD, and DoIP protocols. An anomaly is anything that deviates from expected behaviour. Framework designed for extensibility with future user-scripted custom detectors."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Detect Protocol Anomalies in a Trace (Priority: P1)

As an automotive network analyst, I want to run anomaly detection on a loaded trace to automatically identify protocol violations and deviations in PDU, SOME/IP-SD, and DoIP messages, so I can quickly find issues without manually inspecting thousands of messages.

**Why this priority**: This is the core value proposition. Without detection execution, nothing else matters.

**Independent Test**: Can be fully tested by loading a trace file containing known protocol anomalies (e.g., malformed SOME/IP-SD entries, DoIP version mismatches), clicking "Run Detectors", and verifying that detections appear in the DetectionsWidget table with correct severity, detector name, timestamp, and description.

**Acceptance Scenarios**:

1. **Given** a trace is loaded, **When** I click "Run Detectors" in the top bar, **Then** the system iterates all trace messages and populates the detection table with any anomalies found.
2. **Given** detection is running, **When** I observe the status bar, **Then** I see a progress indicator showing "Detecting: chunk X/Y" and a cancel button.
3. **Given** detection is running, **When** I click the cancel button, **Then** detection stops, partial results are shown, and the UI returns to an interactive state.
4. **Given** a trace with no protocol anomalies, **When** I run detection, **Then** the detection table is empty and the statistics show zero detections.
5. **Given** detection has completed, **When** I load a new trace, **Then** previous detection results are cleared.

---

### User Story 2 - Navigate from Detection to Source Message (Priority: P2)

As an analyst, I want to click on a detection in the detection table to navigate directly to the source message in the message list, so I can inspect the raw data that triggered the anomaly.

**Why this priority**: Click-to-navigate connects detections to their evidence, enabling efficient root-cause analysis.

**Independent Test**: Can be fully tested by running detection on a trace, clicking a detection row, and verifying that the message list scrolls to and selects the corresponding trace message.

**Acceptance Scenarios**:

1. **Given** detections are displayed, **When** I click a detection row, **Then** the MessageListWidget scrolls to and selects the trace message at the detection's `messageIndex`.
2. **Given** I click a detection, **When** the message is selected, **Then** the MessageDetailsWidget shows the details of that message (payload, decoded signals if DB loaded).

---

### User Story 3 - Filter Detections by Severity and Detector (Priority: P3)

As an analyst, I want to filter the detection list by severity level and detector name, so I can focus on the most critical issues or on a specific protocol layer.

**Why this priority**: Filtering is important for usability with large numbers of detections, but the core detection and navigation features must work first.

**Independent Test**: Can be fully tested by running detection on a trace that produces multiple detection types and severities, then using the severity dropdown to show only Errors, and the detector dropdown to show only DoIP detections.

**Acceptance Scenarios**:

1. **Given** detections are displayed, **When** I select "Error" from the severity dropdown, **Then** only Error-severity detections are shown.
2. **Given** detections are displayed, **When** I select "SomeIpSdDetector" from the detector dropdown, **Then** only SOME/IP-SD detections are shown.
3. **Given** filters are applied, **When** I change the severity filter to "All", **Then** all detections from the selected detector are shown.
4. **Given** no trace is loaded, **When** I view the filter dropdowns, **Then** they are empty and disabled.

---

### User Story 4 - Run Detection Without a Database (Priority: P3)

As an analyst, I want to run detection even without an ARXML database loaded, receiving structural anomaly results, so I get value from detection regardless of database availability.

**Why this priority**: Reduces friction for getting started. Database-dependent rules are a subset; structural checks provide value alone.

**Independent Test**: Can be fully tested by loading a trace without loading any database, running detection, and verifying that structural anomalies (malformed headers, length violations, version mismatches) are detected while database-dependent rules (unknown PDU ID, unknown target address) are silently skipped.

**Acceptance Scenarios**:

1. **Given** a trace is loaded but no database, **When** I run detection, **Then** structural anomalies are detected (e.g., malformed DoIP version, truncated SOME/IP-SD entries).
2. **Given** no database is loaded, **When** detection runs, **Then** database-dependent rules are silently skipped without error.
3. **Given** a database is loaded after detection, **When** I re-run detection, **Then** database-dependent rules now produce results (e.g., unknown PDU ID detections).

---

### Edge Cases

- What happens when the trace contains only CAN messages and no Ethernet? Detection runs successfully with zero detections from PDU/SOME/IP-SD/DoIP detectors.
- What happens when the trace is very large (millions of messages)? Detection uses the existing chunk-based iteration with progress reporting and cancellation support. The two-phase threading model (parallel stateless + sequential stateful) ensures acceptable performance.
- What happens if detection is cancelled mid-run? Partial results from completed chunks are displayed. Statistics reflect only the partial results.
- What happens if a detector encounters malformed data it cannot parse? The detector emits an Error-severity detection for the malformed data and continues processing subsequent messages.

## Requirements *(mandatory)*

### Functional Requirements

#### Framework

- **FR-001**: System MUST provide a `DetectionEngine` that orchestrates running a registry of `Detector` instances against all trace messages.
- **FR-002**: System MUST provide a `Detector` abstract base class with the methods: `inspect(const ProtocolMessage&)`, `finalize()`, `name()`, `reset()`, and a `isStateful()` query.
- **FR-003**: System MUST define a `Detection` result type with fields: `timestampUs`, `detectorName`, `severity` (Error/Warning/Info), `message`, `messageIndex`, `relatedBytes` (offset+length), and `context` (key-value map).
- **FR-004**: System MUST define a `ProtocolMessage` type as a tagged union with common fields (timestamp, source message indices, protocol type) and protocol-specific data for SOME/IP, SOME/IP-SD, DoIP, and PDU.
- **FR-005**: System MUST execute detection in two phases: Phase 1 = parallel stateless checks across chunks, Phase 2 = sequential stateful checks in chunk order.
- **FR-006**: System MUST support cancellation of detection via an atomic boolean flag, consistent with the existing Analyzer cancellation pattern.
- **FR-007**: System MUST report detection progress via atomic chunk counters, consistent with the existing Analyzer progress pattern.
- **FR-008**: System MUST store detection results in a timestamp-sorted vector owned by the `DetectionEngine`. Results are cleared when a new detection run starts or a new trace is loaded.
- **FR-009**: System MUST silently skip database-dependent detection rules when no ARXML database is loaded.

#### Protocol Parsing

- **FR-010**: System MUST provide a `ProtocolParser` class that accepts raw Ethernet payload bytes and emits `ProtocolMessage` objects.
- **FR-011**: System MUST parse full SOME/IP headers (16 bytes): Service ID, Method ID, Length, Client ID, Session ID, Protocol Version, Interface Version, Message Type, Return Code, plus payload offset/length.
- **FR-012**: System MUST parse full DoIP headers (8 bytes): Protocol Version, Inverse Version, Payload Type, Payload Length. For diagnostic message payload types (0x8001/0x8002), additionally parse Source Address and Target Address (4 bytes).
- **FR-013**: System MUST add SOME/IP and DoIP wire-format header structs to the existing `NetTypes.h` header.
- **FR-014**: System MUST perform simple sequential TCP stream reassembly for DoIP messages (buffer per TCP stream keyed by src:port to dst:port). Assume TCP segments arrive in capture order.
- **FR-015**: System MUST emit an Info-level detection when out-of-order TCP segments are detected, flagging traces where the simple reassembly approach may produce incorrect results.

#### PDU Detector

- **FR-016**: System MUST detect PDUs where the declared length exceeds the remaining UDP payload bounds (truncated/malformed).
- **FR-017**: System MUST detect PDUs with zero or unreasonably small length.
- **FR-018**: System MUST detect unknown PDU IDs not found in the loaded ARXML database (skipped if no DB).
- **FR-019**: System MUST detect frames with an unusually high number of PDUs packed in a single UDP payload (count anomaly).
- **FR-020**: System MUST detect PDU timing anomalies — unexpected gaps or bursts for a specific PDU ID compared to its observed baseline frequency.

#### SOME/IP-SD Detector

- **FR-021**: System MUST detect malformed SOME/IP-SD entries: truncated option runs, invalid entry types, entries that extend beyond declared SD length.
- **FR-022**: System MUST detect SubscribeEventgroup messages that arrive without a prior corresponding OfferService for the same service.
- **FR-023**: System MUST detect OfferService followed by StopOfferService with no intervening subscribers (potential misconfiguration).
- **FR-024**: System MUST detect Session ID gaps or resets for a given service/client pair.
- **FR-025**: System MUST detect SOME/IP-SD messages arriving on non-standard ports (not 30490).

#### DoIP Detector

- **FR-026**: System MUST detect DoIP protocol version / inverse version mismatch (byte 0 is not the bitwise complement of byte 1).
- **FR-027**: System MUST detect unknown or reserved DoIP payload types.
- **FR-028**: System MUST detect DoIP messages where the declared payload length exceeds the remaining TCP data (truncated message).
- **FR-029**: System MUST detect diagnostic message negative acknowledge (NACK) response codes.
- **FR-030**: System MUST detect routing activation requests that occur without a prior vehicle identification exchange on the same TCP stream.
- **FR-031**: System MUST detect alive check timeouts — an alive check request without a corresponding response within the expected time window.
- **FR-032**: System MUST detect diagnostic messages addressed to unknown target logical addresses not found in the ARXML ECU list (skipped if no DB).

#### UI Integration

- **FR-033**: System MUST add a "Run Detectors" action to the TopBarWidget toolbar.
- **FR-034**: System MUST display detection results in the existing DetectionsWidget table, replacing the current hardcoded mockup data.
- **FR-035**: System MUST update the DetectionsWidget statistics row (total, error count, warning count, info count, first/last timestamp) from real detection results.
- **FR-036**: System MUST support clicking a detection row to navigate to the corresponding trace message in MessageListWidget.
- **FR-037**: System MUST provide severity and detector name dropdown filters in the DetectionsWidget using a QSortFilterProxyModel.
- **FR-038**: System MUST show detection progress in the TopBarWidget status area with a cancel button, following the existing progress bar pattern.
- **FR-039**: System MUST disable the "Run Detectors" button when no trace is loaded, and during an active detection run.

### Key Entities

- **Detection**: A single anomaly finding — carries timestamp, source detector name, severity, human-readable message, index of the triggering trace message, related byte range, and structured context metadata.
- **Detector**: Abstract analysis unit that receives ProtocolMessage objects, maintains internal state (for stateful detectors), and emits zero or more Detection results. Implementations: PduDetector, SomeIpSdDetector, DoipDetector.
- **DetectionEngine**: Orchestrator that iterates trace chunks via the Analyzer, runs the ProtocolParser to produce ProtocolMessages, dispatches them to registered Detectors, and collects results.
- **ProtocolMessage**: A reassembled protocol-layer message (tagged union). Not 1:1 with TraceMessage — produced by the ProtocolParser after reassembly of fragmented Ethernet frames.
- **ProtocolParser**: Stateful parser that receives raw Ethernet payload bytes from TraceMessages and emits complete ProtocolMessage objects. Handles TCP stream reassembly for DoIP and PDU extraction for SOME/IP/UDP.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can run anomaly detection on a loaded trace and see protocol-level detections in under 10 seconds for a 1-million-message trace.
- **SC-002**: All 17 defined anomaly rules produce correct detections when tested against traces containing known anomalies.
- **SC-003**: Users can navigate from any detection to its source message in the message list with a single click.
- **SC-004**: Users can filter detections to view only a specific severity level or detector, reducing the visible detection count to the relevant subset.
- **SC-005**: Detection runs without crashing or blocking the UI, with progress visible and cancellation available at any time.
- **SC-006**: Detection produces meaningful results even without a loaded ARXML database, covering all structural anomaly checks.

## Assumptions

- Captured traces (BLF/pcapng) contain TCP segments in capture order. Full TCP reassembly with out-of-order reordering is deferred to a hardening phase; the initial implementation uses simple sequential concatenation with out-of-order detection flagging.
- The ARXML database, when loaded, contains accurate PDU ID definitions and ECU logical address mappings needed for database-dependent anomaly rules.
- SOME/IP-SD messages follow the AUTOSAR SOME/IP-SD specification (service discovery on port 30490 by default).
- DoIP messages follow ISO 13400-2 specification.
- The existing chunk-based iteration infrastructure in the Analyzer is stable and provides messages in timestamp order within each chunk.
- The future scripting extension (Python/JS detector adapters) is out of scope for this initial implementation but the Detector base class interface is designed to accommodate it.
