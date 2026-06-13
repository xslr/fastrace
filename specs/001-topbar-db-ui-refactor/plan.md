# Implementation Plan: Top Bar UI Refactor — DB Combobox, Trace Browse, Left Panel Removal

**Branch**: `001-topbar-db-ui-refactor` | **Date**: 2026-06-13 | **Spec**: [spec.md](./spec.md)  
**Input**: Feature specification from `/specs/001-topbar-db-ui-refactor/spec.md`

---

## Summary

Remove the left sidebar (`LeftPanelWidget`) from the overview layout, replace `btnSelectDb` with a multi-select checkbox combobox (`cmbDatabase`) that emits path-change signals, and replace `btnOpen` with a "📂 Browse…" sentinel entry inside `cmbTraceFile`. All three changes are confined to the UI layer; no core analyzer or backend logic is affected.

---

## Technical Context

**Language/Version**: C++17  
**Primary Dependencies**: Qt 6 (Widgets module) — QComboBox, QAbstractItemDelegate, QFileDialog, QSignalBlocker  
**Storage**: JSON flat-file via existing `RecentFiles` class; same mechanism reused for DB recent list  
**Testing**: Manual UI validation (no automated test harness present in codebase)  
**Target Platform**: Linux desktop (CMake + Qt6 build)  
**Project Type**: Desktop application  
**Performance Goals**: UI interactions must feel instant (< 16 ms per frame); no background I/O on UI thread  
**Constraints**: Top bar fixed height (46 px) must not grow; `LeftPanelWidget` source files kept but excluded from build  
**Scale/Scope**: Single-window application; changes touch 5 source files + 1 CMakeLists

---

## Constitution Check

Constitution template is unpopulated (no project-specific principles defined). Applying reasonable defaults for a Qt/C++ desktop app:

| Gate | Status | Notes |
|------|--------|-------|
| No new external dependencies | ✅ PASS | Only Qt6 Widgets already in use |
| UI thread safety | ✅ PASS | `QSignalBlocker` already used; pattern continues |
| No file I/O on UI thread | ✅ PASS | TopBarWidget emits signal; loading done downstream |
| Source file hygiene | ✅ PASS | LeftPanelWidget kept, just removed from CMake |
| No complexity regressions | ✅ PASS | Net removal of code; one new delegate class |

No violations. No complexity justification table needed.

---

## Project Structure

### Documentation (this feature)

```text
specs/001-topbar-db-ui-refactor/
├── plan.md              ← this file
├── research.md          ← Phase 0 output
├── data-model.md        ← Phase 1 output
├── contracts/
│   └── topbar-signals.md  ← Phase 1 output
└── tasks.md             ← Phase 2 output (/speckit-tasks)
```

### Source Code (affected files)

```text
ui/
├── CMakeLists.txt              ← remove LeftPanelWidget entries
└── src/
    ├── TopBarWidget.ui         ← remove btnOpen, btnSelectDb; add cmbDatabase
    ├── TopBarWidget.h          ← new signal, new slot, CheckboxDelegate forward-decl
    ├── TopBarWidget.cpp        ← Browse… sentinel, cmbDatabase logic, populateDbCombo()
    ├── MainWindow.cpp          ← remove m_leftPanel instantiation and includes
    ├── MainWindow.h            ← remove m_leftPanel member
    ├── OverviewView.cpp        ← remove leftPanel param from ctor, rebuild 2-panel splitter
    └── OverviewView.h          ← remove leftPanel ctor param and member

# Kept but excluded from build:
    ├── LeftPanelWidget.cpp     ← no changes, excluded from CMakeLists
    ├── LeftPanelWidget.h       ← no changes
    └── LeftPanelWidget.ui      ← no changes
```

**Structure Decision**: Single Qt project; no new subprojects. All changes within `ui/src/`.

---

## Phase 0: Research

> See [research.md](./research.md)

Key questions resolved during research:

1. **Checkbox combobox that stays open** — Qt doesn't provide this natively; custom `QStyledItemDelegate` overriding `paint()` and `editorEvent()` is the canonical approach. The delegate draws a `QStyleOptionButton` checkbox in each row; `editorEvent` handles mouse clicks, toggles `Qt::CheckStateRole`, and calls `view()->update()` without closing the popup.

2. **Keeping popup open on item click** — Default QComboBox closes popup on `activated`. Solution: subclass nothing; instead use `QListView::setEditTriggers(QAbstractItemView::NoEditTriggers)` on the combo's view and handle all interaction in the delegate's `editorEvent`. Popup stays open because no item "selection" is committed.

3. **Sentinel entry pattern** — Store `"::browse::"` as `Qt::UserRole` data on the Browse… item. `onComboActivated(int)` checks `itemData(index).toString() == "::browse::"` and routes accordingly. After file pick, reset `cmbTraceFile` current index to the newly added entry.

4. **Recent DB list persistence** — `fastrace::RecentFiles` is templated on a storage path via its constructor. A second instance pointing to a `db-recent.json` sibling file reuses all existing logic with zero new code.

5. **Multi-file dialog** — `QFileDialog::getOpenFileNames()` (plural) replaces `getOpenFileName()` for the DB Browse… path only. Trace Browse… continues using single-file `getOpenFileName()`.

---

## Phase 1: Design & Contracts

> See [data-model.md](./data-model.md) and [contracts/topbar-signals.md](./contracts/topbar-signals.md)

### Design Decisions

#### A. CheckboxItemDelegate (new class, in TopBarWidget.cpp)

A private `CheckboxItemDelegate : public QStyledItemDelegate` defined within `TopBarWidget.cpp` (not a separate file — it's ~40 lines). Responsibilities:

- `paint()`: draw standard item text + a checkbox on the right, using `Qt::CheckStateRole`
- `editorEvent()`: on `MouseButtonRelease` over the checkbox rect, toggle `Qt::CheckStateRole`, emit `dataChanged`, return `true` (event consumed, popup stays open)
- No editor widget needed (returns `nullptr` from `createEditor`)

#### B. cmbDatabase population

```
populateDbCombo():
  QSignalBlocker on cmbDatabase
  clear()
  add "📂 Browse…" at index 0, data = "::browse::", CheckState = Unchecked (hidden/N/A)
  for each entry in m_recentDbs.getRecent(10):
    add filename text, data = full path
    set CheckStateRole = Checked if path in m_activeDbPaths, else Unchecked
```

#### C. cmbDatabase activated logic

```
onDbComboActivated(int index):
  if itemData == "::browse::":
    paths = QFileDialog::getOpenFileNames(...)
    for each path:
      m_recentDbs.addFile(path)
      m_activeDbPaths.insert(path)
    populateDbCombo()
    emitDbSelectionChanged()
  else:
    // checkbox toggle happened via delegate; this signal fires on keyboard Enter
    // toggle the check state and emit
    toggle CheckStateRole at index
    update m_activeDbPaths
    emitDbSelectionChanged()

emitDbSelectionChanged():
  QStringList paths
  for i in 1..count:  // skip index 0 (Browse…)
    if item.checkState == Checked: paths << itemData
  emit databaseSelectionChanged(paths)
```

#### D. cmbTraceFile Browse… entry

```
populateRecentCombo():
  add "📂 Browse…" at index 0, data = "::browse::"
  then recent files at indices 1..N (unchanged)

onComboActivated(int index):
  path = itemData(index)
  if path == "::browse::":
    picked = QFileDialog::getOpenFileName(...)
    if picked.isEmpty(): reset to previous index; return
    openTrace(picked)   // existing function, handles recents + signal
  else:
    openTrace(path)     // existing path, unchanged
```

#### E. OverviewView constructor change

Remove `LeftPanelWidget* leftPanel` parameter. The `m_mainSplitter` becomes 2-panel:

```cpp
m_mainSplitter->addWidget(m_centreSplitter);   // was index 1, now index 0
m_mainSplitter->addWidget(m_rightSplitter);    // was index 2, now index 1
m_mainSplitter->setSizes({750, 300});           // was {250, 750, 300}
```

---

## Complexity Tracking

No constitution violations — table not required.
