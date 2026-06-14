# Tasks: Lazy Loading Messages

**Input**: Design documents from `/specs/002-message-lazy-load/`
**Branch**: `002-message-lazy-load`
**Spec**: [spec.md](./spec.md) | **Plan**: [plan.md](./plan.md)

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel with other [P] tasks in the same phase
- **[Story]**: User story this task belongs to (US1/US2)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Wire new files into the build system; no logic yet.

- [ ] T001 Add `MessageTableModel.h` and `MessageTableModel.cpp` stubs to `ui/CMakeLists.txt` sources list
- [ ] T002 Add `ChunkEntry` struct and new `Analyzer` field declarations (`chunkIndex_`, `totalMessages_`, `mf_`) to `cpp/include/Analyzer.h` — no implementations yet

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core C++ infrastructure that both user stories depend on. Must be complete before any UI work.

**⚠️ CRITICAL**: US1 and US2 both require this phase to be complete before starting.

- [ ] T003 Implement `Analyzer::buildIndex(const std::string& filename)` in `cpp/src/Analyzer.cpp` — reuse producer-consumer pipeline from `processFile`; consumers run lightweight LOBJ-counting pass (read only `BlfObjectHeaderBase`, skip payloads, count message-type objects); collect `{containerIndex, countInContainer}` pairs into shared vector
- [ ] T004 After `buildIndex` pipeline finishes: sort container counts by `containerIndex`, prefix-sum, emit one `ChunkEntry` per 10,000-message boundary into `chunkIndex_`; set `totalMessages_`; keep `mf_` open — implement in `cpp/src/Analyzer.cpp`
- [ ] T005 Implement `Analyzer::decodeChunk(size_t chunkIndex) const` in `cpp/src/Analyzer.cpp` — seek to `chunkIndex_[chunkIndex].fileOffset`, decompress that container (and next container if needed for split-LOBJ stitching), skip `skipMessages` messages, collect exactly `CHUNK_SIZE` (or fewer for last chunk) full `TraceMessage` objects using existing `processInnerObjects` logic; return `std::vector<TraceMessage>`
- [ ] T006 [P] Update `ui/src/MessageListWidget.ui` — replace `QTableWidget` (`msgTable`) with `QTableView` (same `name="msgTable"`, same position); remove hardcoded `<column>` entries (headers now come from model)
- [ ] T007 [P] Implement `MessageTableModel` skeleton in `ui/src/MessageTableModel.h` and `ui/src/MessageTableModel.cpp` — subclass `QAbstractTableModel`; declare `setAnalyzer()`, `clear()`, `rowCount()`, `columnCount()`, `data()`, `headerData()`, `chunkDecodeRequested` signal, `onChunkDecoded` slot; add `cache_` (`std::map<size_t, std::vector<TraceMessage>>`), `pending_` (`std::set<size_t>`), `analyzer_` member

**Checkpoint**: `Analyzer::buildIndex` + `decodeChunk` compile and link. `MessageTableModel` compiles. Foundation ready.

---

## Phase 3: User Story 1 — Opening a Large Trace File (Priority: P1) 🎯 MVP

**Goal**: User opens a large file; app scans quickly, shows correct row count without loading all messages into memory.

**Independent Test**: Load any BLF file via the UI. The message list scrollbar should reflect the true total row count immediately after the progress bar finishes. Memory usage (via `htop`) should not grow proportionally to file size.

### Implementation for User Story 1

- [ ] T008 [US1] Implement `MessageTableModel::rowCount()` — return `analyzer_->totalMessages()` cast to `int`; return 0 if no analyzer attached — in `ui/src/MessageTableModel.cpp`
- [ ] T009 [US1] Implement `MessageTableModel::columnCount()` — return 8 — in `ui/src/MessageTableModel.cpp`
- [ ] T010 [US1] Implement `MessageTableModel::headerData()` — return column names (Time, Bus, ID/Src, Name, DLC, Data, Length, ECU) for `Qt::Horizontal` / `Qt::DisplayRole` — in `ui/src/MessageTableModel.cpp`
- [ ] T011 [US1] Implement `MessageTableModel::setAnalyzer()` — store shared_ptr, call `beginResetModel()`/`endResetModel()` — in `ui/src/MessageTableModel.cpp`
- [ ] T012 [US1] Add `attachAnalyzer(std::shared_ptr<fastrace::Analyzer>)` slot to `MessageListWidget` in `ui/src/MessageListWidget.h` and `ui/src/MessageListWidget.cpp` — create `MessageTableModel`, call `setAnalyzer()`, set model on `QTableView`; restore column resize modes
- [ ] T013 [US1] Update `MainWindow::startLoad()` in `ui/src/MainWindow.cpp` — change `QtConcurrent` lambda to call `analyzer->buildIndex(path)` instead of `analyzer->processFile(path)`
- [ ] T014 [US1] Update `MainWindow::onLoadFinished()` in `ui/src/MainWindow.cpp` — call `m_messageList->attachAnalyzer(m_analyzer)` instead of `populateFrom(m_analyzer->messages)`; update status label to use `m_analyzer->totalMessages()`

**Checkpoint**: Open any BLF file → progress bar fills → message list shows correct row count with empty/zero rows visible → memory stays low. User Story 1 independently testable.

---

## Phase 4: User Story 2 — Scrolling Through Messages (Priority: P1)

**Goal**: Scrolling shows real message data; unloaded rows show placeholder; cache evicts old chunks to bound memory.

**Independent Test**: After loading a file (Phase 3 complete), scroll through the list. Rows display real data (Time, Bus, ID, Data columns populated). Rapid scroll to the end then back to top should not crash or hang the UI. Memory stays bounded (confirm with `htop` that heap does not grow unboundedly during scroll).

### Implementation for User Story 2

- [ ] T015 [US2] Implement `MessageTableModel::data()` cache-hit path in `ui/src/MessageTableModel.cpp` — compute `chunkIndex = row / CHUNK_SIZE` and `offset = row % CHUNK_SIZE`; if chunk in `cache_`, format and return `QVariant` for the requested column using `TraceMessage` field mapping from data-model.md; return `QVariant()` for non-`DisplayRole` requests
- [ ] T016 [US2] Implement `MessageTableModel::data()` cache-miss path in `ui/src/MessageTableModel.cpp` — if `chunkIndex` not in `cache_` and not in `pending_`, insert into `pending_`, emit `chunkDecodeRequested(chunkIndex)`; return `QVariant("…")` placeholder for all columns
- [ ] T017 [US2] Connect `chunkDecodeRequested` signal to an async decode worker in `MessageTableModel` — use `QtConcurrent::run` to call `analyzer_->decodeChunk(chunkIndex)` on a background thread; connect result delivery back to `onChunkDecoded` slot via `Qt::QueuedConnection` — implement in `ui/src/MessageTableModel.cpp`
- [ ] T018 [US2] Implement `MessageTableModel::onChunkDecoded(size_t chunkIndex, QVector<fastrace::TraceMessage> messages)` in `ui/src/MessageTableModel.cpp` — remove from `pending_`, insert into `cache_`; apply eviction: if `cache_.size() > MAX_CACHED_CHUNKS (3)`, erase the chunk furthest from `chunkIndex`; call `dataChanged` for the row range `[chunkIndex*CHUNK_SIZE, (chunkIndex+1)*CHUNK_SIZE - 1]`
- [ ] T019 [US2] Implement `TraceMessage`→column formatting helpers (static functions) in `ui/src/MessageTableModel.cpp` — timestamp µs → `HH:MM:SS.µµµµµµ`; objectType → bus string; arbId/channel → ID/Src string; data bytes → hex string — extracted from existing `MessageListWidget::populateFrom` logic

**Checkpoint**: Scroll the table; real message data appears in all columns. Rapid scrolling shows "…" briefly then fills in. Memory stays bounded. User Story 2 independently testable.

---

## Phase 5: Polish & Cross-Cutting Concerns

**Purpose**: Cleanup, deprecation removal, and robustness improvements.

- [ ] T020 [P] Mark `MessageListWidget::populateFrom()` deprecated with a `// DEPRECATED` comment in `ui/src/MessageListWidget.h` and `ui/src/MessageListWidget.cpp`; leave body intact for transition
- [ ] T021 [P] Remove `collectMessages`, `maxMessages`, and `messages` vector usage from the UI loading path in `ui/src/MainWindow.cpp` — set `analyzer->collectMessages = false` before `buildIndex` call to avoid redundant allocation
- [ ] T022 Handle `buildIndex` error path in `ui/src/MainWindow.cpp` — if `buildIndex` returns 0 (empty or corrupt file), show an appropriate status message and do not call `attachAnalyzer`
- [ ] T023 [P] Update `ui/src/MessageListWidget.cpp` constructor — remove `QTableWidget`-specific setup calls (`setSectionResizeMode` on `QTableWidget`, `resizeColumnsToContents`) that no longer apply; replace with `QTableView` column resize setup compatible with `MessageTableModel`
- [ ] T024 [P] Add spdlog debug logging in `Analyzer::buildIndex` — log total containers scanned, total messages counted, index size, and elapsed time — in `cpp/src/Analyzer.cpp`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — start immediately
- **Phase 2 (Foundational)**: Depends on Phase 1 — **BLOCKS all user stories**
- **Phase 3 (US1 — Open File)**: Depends on Phase 2 complete
- **Phase 4 (US2 — Scrolling)**: Depends on Phase 3 complete (US2 builds on the attached analyzer)
- **Phase 5 (Polish)**: Depends on Phase 4 complete

### Within Phase 2

- T003 → T004 (prefix-sum depends on counting pass output)
- T005 independent of T003/T004 (different function, can be written in parallel once T002 done)
- T006 and T007 independent of each other and of T003–T005

### Within Phase 3

- T008–T011 parallelizable (different methods in same file — coordinate to avoid merge conflicts)
- T012 depends on T007 (model must exist before wiring into widget)
- T013 depends on T003/T004 (`buildIndex` must exist)
- T014 depends on T012 + T013

### Within Phase 4

- T019 can be written first (pure formatting logic, no Qt deps)
- T015 depends on T019
- T016 depends on T015
- T017 depends on T016
- T018 depends on T017

---

## Parallel Example: Phase 2

```
T003 (counting pipeline) ─┐
T004 (index assembly)  ←──┘ sequential within Analyzer.cpp
T005 (decodeChunk)        ─── independent, write in parallel with T003/T004
T006 (swap .ui widget)    ─── independent of all above
T007 (model skeleton)     ─── independent of all above
```

---

## Implementation Strategy

### MVP First (User Story 1 — Fast Open)

1. Complete Phase 1: Setup (T001–T002)
2. Complete Phase 2 foundational tasks: T003, T004, T006, T007 (T005 can be deferred)
3. Complete Phase 3: T008–T014
4. **STOP and VALIDATE**: Open a BLF file, confirm row count correct, memory low
5. Then proceed to Phase 4 (scrolling / data display)

### Incremental Delivery

1. Phase 1 + Phase 2 → compile-clean foundation
2. Phase 3 → file opens with correct row count (US1 demo-able)
3. Phase 4 → scrolling shows real data (US2 demo-able, full feature complete)
4. Phase 5 → cleanup, robustness

---

## Notes

- [P] within same phase = safe to parallelize (different files or non-conflicting methods)
- `CHUNK_SIZE = 10'000` and `MAX_CACHED_CHUNKS = 3` are constants; define in `Analyzer.h` and `MessageTableModel.h` respectively
- `decodeChunk` (T005) must handle the last chunk returning fewer than `CHUNK_SIZE` messages
- The split-LOBJ case in `decodeChunk` (T005): if `skipMessages > 0`, the chunk starts mid-container; the preceding partial LOBJ must be detected and skipped before counting begins — reuse `findNextLobj` pattern
- `onChunkDecoded` slot must always run on the GUI thread — ensure `QtConcurrent::run` result is delivered via queued connection, not direct
