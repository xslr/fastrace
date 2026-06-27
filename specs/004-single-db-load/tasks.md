---
description: "Task list for 004-single-db-load: Single Database Load for TopBarWidget"
---

# Tasks: Single Database Load for TopBarWidget

**Input**: Design documents from `/specs/004-single-db-load/`  
**Branch**: `004-single-db-load`  
**Prerequisites**: plan.md ✅ | spec.md ✅ | research.md ✅ | data-model.md ✅  
**Tests**: Not requested — no test tasks generated.

**Organization**: Tasks are grouped by user story. No automated test framework is in scope; verification is done by building and running the application.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no shared dependencies)
- **[Story]**: Which user story this task belongs to
- Exact file paths included in every task description

---

## Phase 1: Setup (Remove Dead Code & Files)

**Purpose**: Delete the `DatabaseComboBox` widget and strip it from the build system before touching any logic. This unblocks all subsequent phases and ensures the project still compiles after removal.

**⚠️ CRITICAL**: Must complete before any other phase — later tasks depend on `DatabaseComboBox` being gone.

- [x] T001 Delete `ui/src/DatabaseComboBox.cpp` from the repository
- [x] T002 Delete `ui/include/DatabaseComboBox.h` from the repository
- [x] T003 Remove the `DatabaseComboBox` source entries from `ui/CMakeLists.txt` (lines referencing `src/DatabaseComboBox.cpp` and `include/DatabaseComboBox.h` plus the `# ── Custom DB combobox` comment block)
- [x] T004 Update the `target_include_directories` comment in `ui/CMakeLists.txt` (line ~85) to remove the now-inaccurate reference to `DatabaseComboBox.h` — keep the directive itself

**Checkpoint**: `cmake --build ui/build` must succeed (or configure cleanly) with `DatabaseComboBox` gone.

---

## Phase 2: Foundational (UI Layout & Header Contracts)

**Purpose**: Update the `.ui` file and both headers before any `.cpp` logic changes. Establishing the correct type signatures here unblocks parallel implementation work in Phases 3+.

**⚠️ CRITICAL**: Signal/slot type mismatch will cause compile errors in Phase 3+; resolve here first.

- [x] T005 In `ui/src/TopBarWidget.ui`: change `<widget class="DatabaseComboBox" name="cmbDatabase">` to `<widget class="QComboBox" name="cmbDatabase">` and remove the entire `<customwidgets>` block (the `DatabaseComboBox` registration at the bottom of the file)
- [x] T006 In `ui/include/TopBarWidget.h`: remove the `QSet<QString> m_activeDbPaths` private member
- [x] T007 In `ui/include/TopBarWidget.h`: remove the `void updateDbComboDisplay()` private method declaration
- [x] T008 In `ui/include/TopBarWidget.h`: remove the `void emitDbSelectionChanged()` private method declaration
- [x] T009 In `ui/include/TopBarWidget.h`: add the `void openDatabase(const QString& path)` private method declaration
- [x] T010 In `ui/include/TopBarWidget.h`: change the signal `void databaseSelectionChanged(const QStringList& paths)` to `void databaseSelectionChanged(const QString& path)`
- [x] T011 [P] In `ui/include/MainWindow.h`: change the slot `void onDatabaseSelectionChanged(const QStringList& paths)` to `void onDatabaseSelectionChanged(const QString& path)`
- [x] T012 [P] In `ui/include/MainWindow.h`: change the member `QStringList m_currentDbPaths` to `QString m_currentDbPath`

**Checkpoint**: Headers compile cleanly (no link step needed yet). `TopBarWidget.ui` generates a valid `ui_TopBarWidget.h` that `#include`s `QComboBox`, not `DatabaseComboBox`.

---

## Phase 3: User Story 1 — Select a Database from Recent List (Priority: P1) 🎯 MVP

**Goal**: The database combo shows recent entries formatted as `filename · size · date`, selecting one loads it and replaces any previous database, visually consistent with the trace file combo.

**Independent Test**: Launch the app with at least one entry in the recent-databases list. Open the database combo — entries appear formatted as `filename · size · date`. Click an entry — the database loads (progress bar appears and completes, signal decoding updates). The combo shows the selected entry as current. Re-selecting a different entry replaces the previous load.

### Implementation for User Story 1

- [x] T013 [US1] In `ui/src/TopBarWidget.cpp`: remove the entire `CheckboxItemDelegate` class definition (lines 17–74 in current file)
- [x] T014 [US1] In `ui/src/TopBarWidget.cpp`: remove the `emitDbSelectionChanged()` function body entirely
- [x] T015 [US1] In `ui/src/TopBarWidget.cpp`: remove the `updateDbComboDisplay()` function body entirely
- [x] T016 [US1] In `ui/src/TopBarWidget.cpp`: remove the `#include "DatabaseComboBox.h"` include line
- [x] T017 [US1] In `ui/src/TopBarWidget.cpp`: remove the now-unused includes `<QAbstractItemView>`, `<QMouseEvent>`, `<QStyledItemDelegate>` (verify each has no other usage before removing)
- [x] T018 [US1] In `ui/src/TopBarWidget.cpp` constructor: remove the two `cmbDatabase` delegate/trigger setup lines (`setItemDelegate` and `setEditTriggers`)
- [x] T019 [US1] In `ui/src/TopBarWidget.cpp` constructor: remove the `m_activeDbPaths` restoration loop, the second `populateDbCombo()` call, and the `updateDbComboDisplay()` call
- [x] T020 [US1] In `ui/src/TopBarWidget.cpp` constructor: remove the `QMetaObject::invokeMethod` that queued `emitDbSelectionChanged()`
- [x] T021 [US1] In `ui/src/TopBarWidget.cpp`: rewrite `populateDbCombo()` to format entries as `filename  ·  size  ·  date` using the existing `formatSize()` and `formatDate()` helpers, with no `Qt::CheckStateRole` data, mirroring `populateTraceCombo()`
- [x] T022 [US1] In `ui/src/TopBarWidget.cpp`: add the new `openDatabase(const QString& path)` method — adds to `m_recentDbs`, calls `populateDbCombo()`, sets `currentIndex` via `QSignalBlocker`, emits `databaseSelectionChanged(path)`
- [x] T023 [US1] In `ui/src/TopBarWidget.cpp`: rewrite `onDbComboActivated()` to mirror `onComboActivated()` — index 0 (Browse) opens `QFileDialog::getOpenFileName` with filter `Database Files (*.arxml *.dbc);;All Files (*)`; cancel resets index to -1; otherwise calls `openDatabase(path)`

**Checkpoint**: Build succeeds. Launch app — database combo shows recent entries with size/date. Selecting one triggers a DB load (progress bar visible, signal decoding updates on completion).

---

## Phase 4: User Story 2 — Browse and Load a New Database File (Priority: P1)

**Goal**: Selecting "📂 Browse…" opens a single-file dialog filtered to `.arxml`/`.dbc`. Picking a file loads it, adds it to the recent list, and shows it in the combo. Cancelling the dialog leaves the combo unchanged.

**Independent Test**: Clear recent DBs, open the database combo, click "📂 Browse…", pick a `.arxml` or `.dbc` file. The dialog title is "Open Signal Database". After picking: the database loads, the combo shows the new entry (formatted as `filename · size · date`), and the entry appears in the recent list on the next app launch. Repeat with cancel: no change to current selection.

### Implementation for User Story 2

*Note: T023 (from Phase 3) already implements the Browse branch. This phase validates that the browse behaviour is correct and complete — no additional code changes are needed if T023 is implemented correctly.*

- [x] T024 [US2] Verify in `ui/src/TopBarWidget.cpp` `onDbComboActivated()` that the Browse branch uses `QFileDialog::getOpenFileName` (single-file, not `getOpenFileNames`), confirms the dialog title is `"Open Signal Database"`, and uses the filter `"Database Files (*.arxml *.dbc);;All Files (*)"` — update if needed
- [x] T025 [US2] Verify in `ui/src/TopBarWidget.cpp` that on cancel (`picked.isEmpty()`), `ui->cmbDatabase->setCurrentIndex(-1)` is called and no emit occurs — update if needed
- [x] T026 [US2] Verify in `ui/src/TopBarWidget.cpp` `openDatabase()` that `m_recentDbs.addFile(path.toStdString())` is called before `populateDbCombo()` so the newly browsed file immediately appears in the list — update if needed

**Checkpoint**: Browse flow works end-to-end. Cancel leaves combo unchanged. Picked file loads and appears in the recent list.

---

## Phase 5: User Story 3 — Persist and Restore Last Selected Database (Priority: P2)

**Goal**: On application launch, the most recently selected database is automatically restored. If the file no longer exists, the combo defaults to no selection with no error shown.

**Independent Test**: Select a database, close the app, relaunch — the database is loaded automatically on startup and the combo shows the correct entry. Delete the database file, relaunch — the combo shows the placeholder text, no crash or dialog.

### Implementation for User Story 3

- [x] T027 [US3] In `ui/src/TopBarWidget.cpp` constructor: after the first `populateDbCombo()` call, add startup restoration logic — call `m_recentDbs.getRecent(1)`, check `QFileInfo::exists(path)`, and if valid, queue `openDatabase(lastPath)` via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`
- [x] T028 [US3] Verify that `QFileInfo` is already included in `ui/src/TopBarWidget.cpp`; if not, add `#include <QFileInfo>` (it is already present per current file — confirm no regression)

**Checkpoint**: App launch restores the last-used database. Missing-file case is silently handled.

---

## Phase 6: User Story 4 — Signal Integration (Priority: P2)

**Goal**: Downstream components (`MainWindow`, `MessageDetailsWidget`) receive exactly one `QString` path when the database changes, and an empty string when nothing is selected.

**Independent Test**: Select a database → signal decode updates (if a trace is loaded). Select a second database → previous decode clears and new one loads. Cancel browse with no prior selection → no spurious signal emitted.

### Implementation for User Story 4

- [x] T029 [US4] In `ui/src/MainWindow.cpp`: update `onDatabaseSelectionChanged` parameter from `const QStringList& paths` to `const QString& path`
- [x] T030 [US4] In `ui/src/MainWindow.cpp` `onDatabaseSelectionChanged()`: replace `m_currentDbPaths = paths` with `m_currentDbPath = path`; replace `if (paths.isEmpty())` with `if (path.isEmpty())`; replace `const std::string path = paths[0].toStdString()` with `const std::string stdPath = path.toStdString()` and update the lambda capture accordingly
- [x] T031 [US4] In `ui/src/MainWindow.cpp` `onLoadFinished()`: replace `if (!m_currentDbPaths.isEmpty()) { onDatabaseSelectionChanged(m_currentDbPaths); }` with `if (!m_currentDbPath.isEmpty()) { onDatabaseSelectionChanged(m_currentDbPath); }`
- [x] T032 [US4] In `ui/src/MainWindow.cpp`: search for any remaining references to `m_currentDbPaths` and replace with `m_currentDbPath` (e.g., in `startLoad()` if applicable)

**Checkpoint**: Full integration works — selecting a database while a trace is loaded triggers signal decode update. Re-selecting with a different DB cancels in-flight load and starts a new one. Empty-path case clears the database.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Final verification, comment cleanup, and consistency pass.

- [x] T033 [P] In `ui/src/TopBarWidget.cpp`: update the placeholder text for `cmbDatabase` if still set to `"Signal databases..."` — change to `"Select database (ARXML, DBC…)"` to reflect single-select intent
- [x] T034 [P] In `ui/CMakeLists.txt`: verify that `target_include_directories(fastrace_ui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)` comment accurately describes its purpose (needed for `ui_TopBarWidget.h` and other auto-generated headers) — update comment text if needed
- [x] T035 Full build verification: configure and build the entire `fastrace_ui` target with no warnings related to the changed files
- [x] T036 Manual smoke test: launch app, exercise all four user stories in sequence (recent select → browse → restart/restore → verify signal decode)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — start immediately
- **Phase 2 (Foundational)**: Depends on Phase 1 (files must be deleted before headers updated to remove references)
- **Phase 3 (US1 — Recent Select)**: Depends on Phase 2 — signal type must match before implementation
- **Phase 4 (US2 — Browse)**: Depends on Phase 3 (T023 provides the Browse branch; Phase 4 verifies/adjusts it)
- **Phase 5 (US3 — Persistence)**: Depends on Phase 3 (`openDatabase()` must exist before startup call)
- **Phase 6 (US4 — Signal)**: Depends on Phase 3 (new signal type must be emitted before `MainWindow` updated)
- **Phase 7 (Polish)**: Depends on all phases complete

### User Story Dependencies

- **US1 (P1)**: After Phase 2 — no story dependencies
- **US2 (P1)**: After US1 (reuses `onDbComboActivated` Browse branch from T023)
- **US3 (P2)**: After US1 (requires `openDatabase()` from T022)
- **US4 (P2)**: After US1 (requires new signal signature from T010/T022)

### Within Each Phase

- Phase 3: T013–T017 (dead code removal) → T018–T020 (constructor cleanup) → T021 (populate) → T022 (openDatabase) → T023 (onDbComboActivated)
- Phase 6: T029–T030 (slot body) → T031 (onLoadFinished) → T032 (scan remaining references)

### Parallel Opportunities

- T011 and T012 (Phase 2) — different parts of `MainWindow.h`, can be done together
- T006–T010 (TopBarWidget.h changes) — sequential edits to one file, but trivially fast
- T013–T017 (Phase 3 dead code removal) — all deletions, no ordering constraints between them
- T024–T026 (Phase 4 verification) — independent checks, can be reviewed in parallel
- T033 and T034 (Phase 7) — different files, fully parallel

---

## Parallel Example: Phase 3 Dead-Code Removal (T013–T017)

```text
# All of these remove code from TopBarWidget.cpp — no ordering dependency:
Task T013: Remove CheckboxItemDelegate class
Task T014: Remove emitDbSelectionChanged() body
Task T015: Remove updateDbComboDisplay() body
Task T016: Remove #include "DatabaseComboBox.h"
Task T017: Remove unused includes (<QAbstractItemView>, <QMouseEvent>, <QStyledItemDelegate>)
```

---

## Implementation Strategy

### MVP First (User Stories 1 + 2, same phase)

1. Complete **Phase 1**: Delete DatabaseComboBox files
2. Complete **Phase 2**: Update UI file + headers
3. Complete **Phase 3**: Implement `openDatabase()`, rewrite populate + activated handler → **recent-select works**
4. Complete **Phase 4**: Verify Browse branch → **browse-and-load works**
5. **STOP and VALIDATE**: Build + smoke test US1 & US2

### Incremental Delivery

1. Phase 1 + 2 → Foundation clean
2. Phase 3 + 4 → Core combo behaviour (US1 + US2) ✅ MVP
3. Phase 5 → Startup restoration (US3)
4. Phase 6 → MainWindow signal integration (US4)
5. Phase 7 → Polish + full smoke test

### Single Developer (Sequential)

Work top-to-bottom: T001 → T036. Each phase checkpoint provides a compilable and testable state.

---

## Notes

- `[P]` tasks touch different files and have no unresolved dependencies — safe to do in any order within their phase
- `[Story]` labels map tasks to spec.md user stories for traceability
- No test framework is in scope; verification is manual build + run
- Commit after each phase checkpoint to preserve clean compile state
- The net change is a simplification: ~100 lines removed, ~40 lines added across 6 files

