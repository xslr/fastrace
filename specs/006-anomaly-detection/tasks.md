# Tasks: Anomaly Detection Framework

**Input**: Design documents from `/specs/006-anomaly-detection/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md

**Tests**: Not explicitly requested in the feature specification. Test tasks are omitted.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create the new directory structure and update build system for the detection framework.

- [ ] T001 Create directory structure: `cpp/include/detectors/` and `cpp/detectors/`
- [ ] T002 Update `cpp/CMakeLists.txt` to add new source files (`src/DetectionEngine.cpp`, `src/ProtocolParser.cpp`, `detectors/PduDetector.cpp`, `detectors/SomeIpSdDetector.cpp`, `detectors/DoipDetector.cpp`) to the `fastrace_analyzer` library target and add `cpp/include/detectors/` to the include path
- [ ] T003 Update `ui/CMakeLists.txt` to add new UI source files (`src/DetectionTableModel.cpp`, `include/DetectionTableModel.h`, `src/DetectionFilterProxyModel.cpp`, `include/DetectionFilterProxyModel.h`) to the `fastrace_ui` target

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core types and interfaces that ALL user stories depend on. No user story work can begin until this phase is complete.

### Core Types

- [ ] T004 [P] Define `Severity` enum (Info, Warning, Error) and `ByteRange` struct (offset, length) and `Detection` struct (timestampUs, detectorName, severity, message, messageIndex, relatedBytes, context) in `cpp/include/Detection.h` — per data-model.md (FR-003)
- [ ] T005 [P] Define `ProtocolType` enum (SomeIp, SomeIpSd, DoIp, Pdu) and protocol-specific structs (`SomeIpHeader`, `SomeIpSdMessage`, `DoipHeader`, `PduInfo`) and the `ProtocolMessage` tagged union in `cpp/include/ProtocolMessage.h` — per data-model.md (FR-004)
- [ ] T006 [P] Add packed wire-format header structs `SomeIpWireHeader` (16 bytes) and `DoipWireHeader` (8 bytes) and `DoipDiagSubHeader` (4 bytes: source + target address) to `cpp/include/NetTypes.h` — per FR-011, FR-012, FR-013
- [ ] T007 [P] Define `Detector` abstract base class with virtual methods `name()`, `isStateful()`, `inspect(const ProtocolMessage&)`, `finalize()`, `reset()`, `clone()`, `merge(const Detector&)` and a protected `emitDetection()` helper that appends to an internal `std::vector<Detection>`, with `takeDetections()` to move results out, in `cpp/include/Detector.h` — per data-model.md (FR-002)

### Protocol Parser

- [ ] T008 Define `ProtocolParser` class in `cpp/include/ProtocolParser.h` with: constructor taking optional `ArDatabase*`, method `parse(const TraceMessage& tm, size_t globalMsgIndex) -> std::vector<ProtocolMessage>`, internal TCP stream reassembly buffers (keyed by src:port+dst:port pair), and `reset()` method — per data-model.md, FR-010
- [ ] T009 Implement `ProtocolParser::parse()` in `cpp/src/ProtocolParser.cpp`: dispatch on TraceMessage fields — for UDP payloads extract SOME/IP PDUs by parsing 16-byte SOME/IP headers (FR-011), identify SOME/IP-SD (serviceId=0xFFFF, methodId=0x8100) and parse SD header+entries (FR-021), extract raw PDU info; for TCP payloads perform sequential stream reassembly and parse DoIP headers (FR-012, FR-014). Emit Info-level detection for out-of-order TCP segments (FR-015).

### Detection Engine

- [ ] T010 Define `DetectionEngine` class in `cpp/include/DetectionEngine.h` with: `addDetector(std::unique_ptr<Detector>)`, `run(Analyzer*, const ArDatabase*, std::atomic<bool>& cancelled, std::atomic<size_t>& chunksProcessed)`, `getResults() -> const std::vector<Detection>&`, `clearResults()`, `chunkCount() -> size_t` — per data-model.md (FR-001)
- [ ] T011 Implement `DetectionEngine::run()` in `cpp/src/DetectionEngine.cpp`: Phase 1 — iterate chunks in parallel using `std::thread` pool, for each chunk call `Analyzer::decodeChunk()`, run `ProtocolParser::parse()` on each message, dispatch each `ProtocolMessage` to all registered stateless detectors (where `isStateful()` returns false), collect detections with mutex protection (FR-005). Phase 2 — iterate chunks sequentially in order, dispatch `ProtocolMessage` objects to all stateful detectors, call `finalize()` on each, collect results (FR-005). Sort final results by timestamp (FR-008). Support cancellation via atomic flag (FR-006) and progress via atomic counter (FR-007). Skip DB-dependent rules if `ArDatabase*` is nullptr (FR-009).

**Checkpoint**: Foundation ready — all core types, parser, and engine are defined. User story implementation can begin.

---

## Phase 3: User Story 1 — Detect Protocol Anomalies in a Trace (Priority: P1) MVP

**Goal**: User loads a trace, clicks "Run Detectors", sees protocol anomalies in the DetectionsWidget with correct severity, detector name, timestamp, and description.

**Independent Test**: Load a trace with known anomalies, click "Run Detectors", verify detections appear in the table.

### Detectors

- [ ] T012 [P] [US1] Implement `PduDetector` in `cpp/include/detectors/PduDetector.h` and `cpp/detectors/PduDetector.cpp`: stateful detector tracking per-PDU-ID timing baselines; `inspect()` checks PDU length > remaining payload (FR-016), zero/small length (FR-017), unknown PDU ID against ArDatabase (FR-018, skipped if no DB), high PDU count per frame (FR-019); `finalize()` emits timing anomalies by comparing per-PDU-ID intervals against baseline mean+stddev (FR-020)
- [ ] T013 [P] [US1] Implement `SomeIpSdDetector` in `cpp/include/detectors/SomeIpSdDetector.h` and `cpp/detectors/SomeIpSdDetector.cpp`: stateful detector tracking service lifecycle (offered services, subscribers, session IDs per service/client); `inspect()` checks malformed SD entries (FR-021), subscribe without prior offer (FR-022), offer+stop with no subscribers (FR-023), session ID gaps/resets (FR-024), non-standard port != 30490 (FR-025)
- [ ] T014 [P] [US1] Implement `DoipDetector` in `cpp/include/detectors/DoipDetector.h` and `cpp/detectors/DoipDetector.cpp`: stateful detector tracking per-TCP-stream connection state (vehicle identification done, routing activation done, pending alive checks); `inspect()` checks version/inverse mismatch (FR-026), unknown payload type (FR-027), truncated payload (FR-028), NACK codes (FR-029), routing without identification (FR-030), alive check timeout by tracking request timestamps (FR-031), unknown target address against ArDatabase ECU list (FR-032, skipped if no DB)

### UI Integration (Run + Display)

- [ ] T015 [US1] Create `DetectionTableModel` (QAbstractTableModel subclass) in `ui/include/DetectionTableModel.h` and `ui/src/DetectionTableModel.cpp`: columns Time, Detector, Severity, Message; receives `const std::vector<Detection>&` from DetectionEngine; provides data for display in DetectionsWidget table — per FR-034
- [ ] T016 [US1] Add "Run Detectors" action and a cancel button to `ui/include/TopBarWidget.h` and `ui/src/TopBarWidget.cpp`: new QPushButton or QToolButton in the toolbar, disabled when no trace is loaded (FR-039), triggers `runDetectorsRequested()` signal; add progress label and cancel button for detection progress display (FR-038)
- [ ] T017 [US1] Modify `ui/include/DetectionsWidget.h` and `ui/src/DetectionsWidget.cpp`: remove all hardcoded mockup data from `populateDetections()` and `populateStatistics()`, replace with methods `setDetections(const std::vector<Detection>&)` that populates DetectionTableModel and updates statistics row (total, error, warning, info counts, first/last timestamps) — per FR-034, FR-035
- [ ] T018 [US1] Wire detection flow in `ui/src/MainWindow.cpp`: connect TopBarWidget `runDetectorsRequested()` signal to a slot that creates `DetectionEngine`, registers all three detectors, calls `QtConcurrent::run()` to execute `DetectionEngine::run()` on background thread, polls progress via QTimer to update TopBarWidget progress label, on completion calls `DetectionsWidget::setDetections()` with results. Connect cancel button to set atomic cancelled flag. Clear detections when new trace is loaded (FR-033, FR-038, FR-039).

**Checkpoint**: User Story 1 is fully functional — user can load a trace, run detection, see real anomalies, observe progress, cancel detection, and see statistics. Detection results are cleared on new trace load.

---

## Phase 4: User Story 2 — Navigate from Detection to Source Message (Priority: P2)

**Goal**: Clicking a detection row navigates to the source trace message in the MessageListWidget.

**Independent Test**: Run detection, click a row, verify MessageListWidget scrolls to and selects the correct message.

- [ ] T019 [US2] Add `detectionSelected(size_t messageIndex)` signal to `DetectionsWidget` in `ui/include/DetectionsWidget.h` and `ui/src/DetectionsWidget.cpp`, emitted when a row is clicked in the detection table — extract `messageIndex` from the clicked Detection via DetectionTableModel — per FR-036
- [ ] T020 [US2] Wire navigation in `ui/src/MainWindow.cpp`: connect `DetectionsWidget::detectionSelected(size_t)` signal to `MessageListWidget::scrollToMessage(size_t)` slot (add slot if not existing) to scroll to and select the trace message at the given index. Verify `MessageDetailsWidget` updates automatically via existing selection-changed signal — per FR-036

**Checkpoint**: User Story 2 complete — click-to-navigate works end-to-end from detection table to message list and details view.

---

## Phase 5: User Story 3 — Filter Detections by Severity and Detector (Priority: P3)

**Goal**: User can filter the detection table by severity level and detector name using dropdown controls.

**Independent Test**: Run detection producing multiple types/severities, use severity dropdown to show only Errors, use detector dropdown to show only DoIP results.

- [ ] T021 [P] [US3] Create `DetectionFilterProxyModel` (QSortFilterProxyModel subclass) in `ui/include/DetectionFilterProxyModel.h` and `ui/src/DetectionFilterProxyModel.cpp`: filter by severity (All/Error/Warning/Info) and by detector name (All/PduDetector/SomeIpSdDetector/DoipDetector); `filterAcceptsRow()` checks both criteria — per FR-037
- [ ] T022 [US3] Add severity and detector name QComboBox filter controls to `ui/src/DetectionsWidget.ui` and wire them in `ui/src/DetectionsWidget.cpp`: insert `DetectionFilterProxyModel` between `DetectionTableModel` and the QTableView; connect combobox `currentIndexChanged` signals to update filter criteria on proxy model; populate detector combobox dynamically from unique detector names in results; disable dropdowns when no detections exist — per FR-037

**Checkpoint**: User Story 3 complete — filtering works, dropdowns populated dynamically, disabled when empty.

---

## Phase 6: User Story 4 — Run Detection Without a Database (Priority: P3)

**Goal**: Detection runs and produces structural anomaly results even without an ARXML database loaded.

**Independent Test**: Load trace without database, run detection, verify structural anomalies are found and DB-dependent rules are silently skipped.

- [ ] T023 [US4] Verify and document DB-skip behavior across all three detectors: ensure `PduDetector` skips FR-018 (unknown PDU ID) when `ArDatabase*` is null, ensure `DoipDetector` skips FR-032 (unknown target address) when ECU list is empty, and ensure all other rules function correctly without a database — per FR-009. Add guard checks in each detector's `inspect()` method if not already present from T012-T014.

**Checkpoint**: User Story 4 complete — detection works with and without database, DB-dependent rules gracefully skipped.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Final cleanup, formatting, and documentation.

- [ ] T024 [P] Run `clang-format` on all new and modified C++ files in `cpp/include/`, `cpp/src/`, `cpp/include/detectors/`, `cpp/detectors/`, `ui/include/`, `ui/src/`
- [ ] T025 [P] Verify build compiles cleanly on all platforms: run `cmake --build build` and fix any warnings or errors
- [ ] T026 [P] Update `AGENTS.md` to document new files and directories (`cpp/include/detectors/`, `cpp/detectors/`, Detection.h, Detector.h, DetectionEngine.h, ProtocolMessage.h, ProtocolParser.h, DetectionTableModel, DetectionFilterProxyModel`)
- [ ] T027 Run end-to-end validation per `quickstart.md`: load a trace, run detectors, verify detections appear, click to navigate, filter by severity and detector, verify statistics, test cancel, test without database

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion — BLOCKS all user stories
- **User Stories (Phase 3-6)**: All depend on Foundational phase completion
  - US1 (Phase 3): Can start after Phase 2. **All other stories depend on US1 for UI wiring.**
  - US2 (Phase 4): Depends on US1 (needs DetectionsWidget with real data)
  - US3 (Phase 5): Depends on US1 (needs DetectionTableModel to exist)
  - US4 (Phase 6): Depends on US1 (validates detector behavior from T012-T014)
- **Polish (Phase 7)**: Depends on all user stories being complete

### Within Each User Story

- Backend code before UI code
- Models/types before services/engines
- Engine before widget wiring

### Parallel Opportunities

**Phase 2 parallel group**: T004, T005, T006, T007 can all run in parallel (different header files)
**Phase 3 parallel group**: T012, T013, T014 can all run in parallel (different detector files, all depend on T007+T008+T009)
**Phase 5 parallel group**: T021 can run in parallel with other Phase 5 work (new file)
**Phase 7 parallel group**: T024, T025, T026 can all run in parallel

---

## Parallel Example: User Story 1

```text
# After Phase 2 is complete, launch all three detectors in parallel:
Task T012: "Implement PduDetector in cpp/include/detectors/PduDetector.h and cpp/detectors/PduDetector.cpp"
Task T013: "Implement SomeIpSdDetector in cpp/include/detectors/SomeIpSdDetector.h and cpp/detectors/SomeIpSdDetector.cpp"
Task T014: "Implement DoipDetector in cpp/include/detectors/DoipDetector.h and cpp/detectors/DoipDetector.cpp"

# Then wire UI sequentially:
Task T015: "Create DetectionTableModel"
Task T016: "Add Run Detectors button"
Task T017: "Wire DetectionsWidget to real data"
Task T018: "Wire MainWindow detection flow"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001-T003)
2. Complete Phase 2: Foundational (T004-T011)
3. Complete Phase 3: User Story 1 (T012-T018)
4. **STOP and VALIDATE**: Load trace, run detectors, verify anomalies appear with correct severity/timestamp/message
5. Build and demo

### Incremental Delivery

1. Setup + Foundational -> Framework ready
2. Add User Story 1 -> Core detection works (MVP)
3. Add User Story 2 -> Click-to-navigate
4. Add User Story 3 -> Filtering
5. Add User Story 4 -> DB-independence validation
6. Polish -> Production-ready

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- Each user story should be independently completable and testable
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently
- The `detectors/` directory under `cpp/` is new and separate from `cpp/src/` per user preference
