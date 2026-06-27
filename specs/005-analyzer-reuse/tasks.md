---
description: "Task list for Analyzer Reuse & Trace Loading UI Disable"
---

# Tasks: Analyzer Reuse & Trace Loading UI Disable

**Input**: Design documents from `/specs/005-analyzer-reuse/`
**Prerequisites**: plan.md, spec.md, data-model.md

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2)
- Include exact file paths in descriptions

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization and basic structure

*(No setup tasks required as this feature modifies an existing project.)*

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before ANY user story can be implemented

*(No foundational tasks required. The stories are independent enhancements to existing infrastructure.)*

**Checkpoint**: Foundation ready - user story implementation can now begin in parallel

---

## Phase 3: User Story 1 - Prevent Invalid Interactions During Trace Loading (Priority: P1) 🎯 MVP

**Goal**: Disable the trace loading selection while a trace is actively loading to prevent invalid state.

**Independent Test**: Load a large trace and verify that the trace selection combo box is disabled and visually indicates it cannot be used until the trace load completes or is cancelled.

### Implementation for User Story 1

- [x] T001 [P] [US1] Add `setTraceComboEnabled` declaration to TopBarWidget in `ui/include/TopBarWidget.h`
- [x] T002 [US1] Implement `setTraceComboEnabled` logic in `ui/src/TopBarWidget.cpp`
- [x] T003 [US1] Disable trace combo on load start in `MainWindow::startLoad` in `ui/src/MainWindow.cpp`
- [x] T004 [US1] Re-enable trace combo on load finish/cancel in `MainWindow::onLoadFinished` in `ui/src/MainWindow.cpp`

**Checkpoint**: At this point, User Story 1 should be fully functional and testable independently

---

## Phase 4: User Story 2 - Consistent Analyzer State Across UI (Priority: P2)

**Goal**: Reuse the existing `Analyzer` object by clearing its state in-place to prevent dangling pointers in UI widgets.

**Independent Test**: Load a trace, then load a second trace, and verify that all widgets automatically update to reflect the new trace's data.

### Implementation for User Story 2

- [x] T005 [P] [US2] Add `reset()` declaration to Analyzer in `cpp/include/Analyzer.h`
- [x] T006 [US2] Implement `reset()` logic clearing all state and atomics in `cpp/src/Analyzer.cpp`
- [x] T007 [US2] Update `MainWindow::startLoad` in `ui/src/MainWindow.cpp` to call `m_analyzer->reset()` instead of allocating a new `Analyzer`

**Checkpoint**: At this point, User Stories 1 AND 2 should both work independently

---

## Phase 5: Polish & Cross-Cutting Concerns

**Purpose**: Improvements that affect multiple user stories

- [x] T008 Check that UI cleanly re-enables when loading fails or is manually aborted.
- [x] T009 Ensure DB is properly cleared during `m_analyzer->reset()`.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: N/A
- **Foundational (Phase 2)**: N/A
- **User Stories (Phase 3+)**: Can start immediately.
  - User Story 1 and User Story 2 can proceed sequentially or in parallel.
- **Polish (Final Phase)**: Depends on all desired user stories being complete.

### User Story Dependencies

- **User Story 1 (P1)**: No dependencies.
- **User Story 2 (P2)**: No dependencies. May modify the same `MainWindow::startLoad` function as US1, so careful merging or sequential implementation is advised.

### Within Each User Story

- Headers before source files.
- Core implementation before integration.
- Story complete before moving to next priority.

### Parallel Opportunities

- T001 (TopBar header) and T005 (Analyzer header) can be done in parallel.
- T002 (TopBar logic) and T006 (Analyzer logic) can be done in parallel.

---

## Parallel Example: User Story 1 & 2

```bash
# Launch header modifications in parallel:
Task: "Add setTraceComboEnabled declaration to TopBarWidget in ui/include/TopBarWidget.h"
Task: "Add reset() declaration to Analyzer in cpp/include/Analyzer.h"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 3: User Story 1
2. **STOP and VALIDATE**: Test User Story 1 independently
3. Deploy/demo if ready

### Incremental Delivery

1. Add User Story 1 → Test independently → Deploy/Demo (MVP!)
2. Add User Story 2 → Test independently → Deploy/Demo
3. Each story adds value without breaking previous stories
