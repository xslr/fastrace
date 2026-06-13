# Tasks: Top Bar UI Refactor — DB Combobox, Trace Browse, Left Panel Removal

**Input**: Design documents from `/specs/001-topbar-db-ui-refactor/`  
**Prerequisites**: plan.md ✅ | spec.md ✅ | research.md ✅ | data-model.md ✅ | contracts/ ✅

**Tests**: Not requested — no test tasks generated.

**Organization**: Grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to
- All paths relative to repo root

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Wire up build system and establish the new class skeleton before any story work begins.

- [x] T001 Remove LeftPanelWidget entries from `ui/CMakeLists.txt` (3 lines: .cpp, .h, .ui)
- [x] T002 [P] Add `CheckboxItemDelegate` private class skeleton (paint + editorEvent stubs) at top of `ui/src/TopBarWidget.cpp`
- [x] T003 [P] Add `m_recentDbs` (RecentFiles instance) and `m_activeDbPaths` (QSet<QString>) member fields to `ui/src/TopBarWidget.h`

**Checkpoint**: Project compiles without LeftPanelWidget; TopBarWidget has new member stubs.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core structural changes that every user story depends on. Must be complete before story work begins.

- [x] T004 Remove `m_leftPanel` member declaration from `ui/src/MainWindow.h` and remove `LeftPanelWidget` forward-declaration and include
- [x] T005 Remove `LeftPanelWidget` include from `ui/src/MainWindow.cpp`; remove `m_leftPanel = new LeftPanelWidget` instantiation; remove `m_leftPanel` argument from `OverviewView` constructor call
- [x] T006 Update `OverviewView` constructor signature in `ui/src/OverviewView.h` — remove `LeftPanelWidget* leftPanel` parameter; remove `m_leftPanel` member field; remove `LeftPanelWidget` include and forward-declaration
- [x] T007 Rewrite `OverviewView` constructor body in `ui/src/OverviewView.cpp` — remove left-panel include, remove `m_leftPanel` init, rebuild `m_mainSplitter` as 2-panel (Centre | Right) with `setSizes({750, 300})`; remove LeftPanelWidget include
- [x] T008 Add `databaseSelectionChanged(const QStringList& paths)` signal declaration to `ui/src/TopBarWidget.h`
- [x] T009 [P] Add `onDbComboActivated(int index)` and `populateDbCombo()` private slot/method declarations to `ui/src/TopBarWidget.h`

**⚠️ CRITICAL**: No user story work can begin until this phase is complete. Verify build succeeds before proceeding.

**Checkpoint**: App launches, shows 2-panel layout (Centre + Right), no left sidebar visible. TopBarWidget declares new signal.

---

## Phase 3: User Story 1 — Load a Trace File via Browse (Priority: P1) 🎯 MVP

**Goal**: Replace `btnOpen` with a "📂 Browse…" sentinel entry at the top of `cmbTraceFile`. Selecting it opens a file dialog; confirmed pick loads the trace and adds it to the recent list.

**Independent Test**: Launch app with empty recent-file history → open cmbTraceFile → "📂 Browse…" entry visible at top → select it → pick a .blf file → trace loads → file appears in recent list → Browse… still at top.

### Implementation for User Story 1

- [x] T010 [US1] Remove `btnOpen` widget block from `ui/src/TopBarWidget.ui`
- [x] T011 [US1] Remove `btnOpen` connect() call and `onBtnOpenClicked` slot declaration from `ui/src/TopBarWidget.h`
- [x] T012 [US1] Remove `onBtnOpenClicked()` slot implementation from `ui/src/TopBarWidget.cpp`
- [x] T013 [US1] Update `populateRecentCombo()` in `ui/src/TopBarWidget.cpp` — insert "📂 Browse…" entry at index 0 with `Qt::UserRole` data `"::browse::"` before populating recent files
- [x] T014 [US1] Update `onComboActivated(int index)` in `ui/src/TopBarWidget.cpp` — detect `"::browse::"` sentinel, call `QFileDialog::getOpenFileName()`, call `openTrace(path)` on confirm, reset combo to previous index on cancel

**Checkpoint**: btnOpen is gone. User can load a trace via Browse… entry in cmbTraceFile. Recent files still selectable. Cancelling dialog leaves combo unchanged.

---

## Phase 4: User Story 2 — Load a Recent Trace File (Priority: P1)

**Goal**: Ensure selecting a recent entry in `cmbTraceFile` still loads the trace immediately (regression-free after Browse… entry insertion at index 0).

**Independent Test**: Load a trace via Browse… (adds to recents) → reopen combo → click the recent entry → trace reloads immediately, no dialog shown.

### Implementation for User Story 2

- [x] T015 [US2] Verify `onComboActivated` in `ui/src/TopBarWidget.cpp` correctly skips Browse… sentinel (index 0) and routes all other indices to `openTrace(path)` — adjust index offset if needed since Browse… is now index 0 and recent files start at index 1
- [x] T016 [US2] Verify `populateRecentCombo()` in `ui/src/TopBarWidget.cpp` — confirm `setCurrentIndex(-1)` call still places combo in "no selection" state on startup, and that Browse… entry at index 0 does not interfere with recent-file display text

**Checkpoint**: Recent trace loading works identically to before. Browse… entry does not interfere with recent-file selection.

---

## Phase 5: User Story 4 — Use App Without Left Sidebar (Priority: P1)

**Goal**: Overview layout renders as 2-panel (Centre | Right) with no gap, no blank column, no reference to LeftPanelWidget.

**Independent Test**: Launch app in Overview mode → confirm only Centre and Right panels visible → resize window → both panels scale proportionally → switch to Notebook mode → layout unaffected.

### Implementation for User Story 4

- [x] T017 [US4] Verify `OverviewView::activate()` in `ui/src/OverviewView.cpp` — timeline re-parent check still works correctly with 2-panel splitter (index logic unchanged: timeline inserts at 0 in centreSplitter)
- [x] T018 [US4] Verify `OverviewView::deactivate()` in `ui/src/OverviewView.cpp` — timeline detach still works correctly; no leftover references to m_leftPanel
- [x] T019 [US4] Full build + launch smoke test — confirm no LeftPanelWidget symbols referenced, no linker errors, 2-panel layout visible

**Checkpoint**: App launches cleanly, 2-panel overview visible, Notebook mode unaffected.

---

## Phase 6: User Story 3 — Select One or More Signal Databases (Priority: P2)

**Goal**: `btnSelectDb` is replaced by `cmbDatabase` — a checkbox combobox with a "📂 Browse…" entry, a recent-DB list with per-entry checkboxes, and a `databaseSelectionChanged(QStringList)` signal emitted on every state change. Dropdown stays open while toggling.

**Independent Test**: Open cmbDatabase → "📂 Browse…" at top → click Browse…, pick 2 ARXML files → both appear checked → click outside → dropdown closes → signal emitted with 2 pa- [x] T020 [US3] Replace `btnSelectDb` widget block in `ui/src/TopBarWidget.ui` with a `QComboBox` named `cmbDatabase`, minimum width 200px
- [x] T021 [US3] Implement `CheckboxItemDelegate::paint()` in `ui/src/TopBarWidget.cpp` — draw item text + a `QStyleOptionButton` checkbox on the right using `Qt::CheckStateRole`; skip checkbox for index 0 (Browse… entry)
- [x] T022 [US3] Implement `CheckboxItemDelegate::editorEvent()` in `ui/src/TopBarWidget.cpp` — on `MouseButtonRelease` in checkbox rect, toggle `Qt::CheckStateRole` on the model, call `view()->update()`, return `true` (consumes event, popup stays open)
- [x] T023 [US3] Install `CheckboxItemDelegate` on `cmbDatabase->view()` in `TopBarWidget` constructor in `ui/src/TopBarWidget.cpp`; set `view()->setEditTriggers(QAbstractItemView::NoEditTriggers)`
- [x] T024 [US3] Implement `populateDbCombo()` in `ui/src/TopBarWidget.cpp` — QSignalBlocker, clear, add Browse… sentinel at index 0, then for each entry in `m_recentDbs.getRecent(10)`: addItem with path as UserRole data and CheckStateRole = Checked/Unchecked based on `m_activeDbPaths`
- [x] T025 [US3] Connect `cmbDatabase` `activated(int)` signal to `onDbComboActivated` in `TopBarWidget` constructor in `ui/src/TopBarWidget.cpp`; call `populateDbCombo()` and set placeholder text
- [x] T026 [US3] Implement `onDbComboActivated(int index)` in `ui/src/TopBarWidget.cpp`:
  - If sentinel: call `QFileDialog::getOpenFileNames()`, add each path to `m_recentDbs` + `m_activeDbPaths`, call `populateDbCombo()`, call `emitDbSelectionChanged()`
  - Else (keyboard Enter on a checked item): toggle CheckStateRole at index, update `m_activeDbPaths`, call `emitDbSelectionChanged()`
- [x] T027 [US3] Implement `emitDbSelectionChanged()` private method in `ui/src/TopBarWidget.cpp` — iterate cmbDatabase items from index 1, collect paths where CheckStateRole == Checked, emit `databaseSelectionChanged(paths)`
- [x] T028 [US3] Initialize `m_recentDbs` with a distinct storage path (e.g. alongside trace recent file with suffix `-db`) in `TopBarWidget` constructor in `ui/src/TopBarWidget.cpp``m_recentDbs` with a distinct storage path (e.g. alongside trace recent file with suffix `-db`) in `TopBarWidget` constructor in `ui/src/TopBarWidget.cpp`

**Checkpoint**: cmbDatabase shows checkboxes, stays open on toggle, Browse… adds files as checked, signal fires on every state change with correct path list.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Cleanup, placeholder text, edge-case hardening across all stories.

- [x] T029 [P] Set placeholder text on `cmbDatabase` in `ui/src/TopBarWidget.cpp` constructor (e.g. `tr("Signal databases…")`)
- [x] T030 [P] Set placeholder text on `cmbTraceFile` to confirm it still shows correctly after Browse… entry insertion (regression check in `ui/src/TopBarWidget.cpp`)
- [x] T031 Remove `onBtnSelectDbClicked` (or equivalent) dead code from `ui/src/TopBarWidget.cpp` and `ui/src/TopBarWidget.h` if any existed
- [x] T032 Remove `LeftPanelWidget` and `m_leftPanel`-related dead includes from all translation units — verify with `grep -r "LeftPanel" ui/src/` returning zero hits in compiled files
- [x] T033 [P] Update `ui/src/OverviewView.h` doc comment to reflect 2-panel layout (remove left-panel reference from ASCII art)
- [x] T034 Full rebuild from clean and manual smoke test of all 4 user stories end-to-end

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — start immediately
- **Phase 2 (Foundational)**: Depends on Phase 1 — **BLOCKS all user stories**
- **Phase 3 (US1 — Browse Trace)**: Depends on Phase 2
- **Phase 4 (US2 — Recent Trace)**: Depends on Phase 3 (verifies no regression after Browse… insertion)
- **Phase 5 (US4 — No Left Panel)**: Depends on Phase 2; can run in parallel with Phase 3/4
- **Phase 6 (US3 — DB Combo)**: Depends on Phase 2; can run in parallel with Phases 3–5
- **Phase 7 (Polish)**: Depends on Phases 3–6 all complete

### User Story Dependencies

| Story | Depends on | Can parallel with |
|-------|-----------|-------------------|
| US1 — Browse Trace (P1) | Phase 2 | US4 |
| US2 — Recent Trace (P1) | US1 (regression check) | — |
| US4 — No Left Panel (P1) | Phase 2 | US1, US3 |
| US3 — DB Combo (P2) | Phase 2 | US1, US4 |

### Within Each User Story

- UI file edits before C++ logic (avoids moc regeneration conflicts)
- Header declarations before .cpp implementations
- Constructor wiring last (after helpers exist)

### Parallel Opportunities

- T002 + T003 (Phase 1): parallel — different scopes in same file and header
- T004 + T008 + T009 (Phase 2): T004/T005 sequential; T008/T009 parallel with each other
- T017 + T018 (Phase 5): parallel — independent verification tasks
- T021 + T022 (Phase 6): parallel — different methods of same class
- T029 + T030 + T033 (Phase 7): all parallel — different files/lines

---

## Parallel Example: Phase 6 (US3)

```text
# Parallel batch 1 — UI + delegate foundation:
T020  Replace btnSelectDb with cmbDatabase in TopBarWidget.ui
T021  Implement CheckboxItemDelegate::paint()
T022  Implement CheckboxItemDelegate::editorEvent()

# Sequential — depends on delegate existing:
T023  Install delegate on cmbDatabase->view()
T024  Implement populateDbCombo()
T025  Wire activated() signal in constructor

# Sequential — depends on populateDbCombo + signal wired:
T026  Implement onDbComboActivated()
T027  Implement emitDbSelectionChanged()
T028  Initialize m_recentDbs in constructor
```

---

## Implementation Strategy

### MVP First (US1 + US4 — the two P1 layout changes)

1. Complete Phase 1: Setup (T001–T003)
2. Complete Phase 2: Foundational (T004–T009) — verify build
3. Complete Phase 3: US1 Browse Trace (T010–T014)
4. Complete Phase 4: US2 Recent Trace regression (T015–T016)
5. Complete Phase 5: US4 No Left Panel (T017–T019)
6. **STOP and VALIDATE**: Launch app, confirm 2-panel layout + Browse… in trace combo works
7. Proceed to Phase 6 (US3 DB Combo) as next increment

### Incremental Delivery

1. Phases 1–2 → clean build, no left panel in layout
2. Phases 3–4 → Browse… in trace combo, recent files still work
3. Phase 5 → layout confirmed clean
4. Phase 6 → DB combobox with checkboxes functional
5. Phase 7 → polish, final smoke test

---

## Notes

- No new source files needed — all changes in existing files
- `CheckboxItemDelegate` lives entirely in `TopBarWidget.cpp` (private implementation detail)
- `LeftPanelWidget` source files are intentionally left on disk — do not delete
- Verify `grep -r "LeftPanel" ui/src/*.cpp ui/src/*.h` returns zero hits (except in LeftPanelWidget files themselves) after T032
- `databaseSelectionChanged` signal has no consumer wired yet in MainWindow — that is out of scope for this feature; the signal is emitted correctly but nothing connects to it until a future DB-loading feature is implemented
