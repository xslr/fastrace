# Research: Top Bar UI Refactor

**Feature**: 001-topbar-db-ui-refactor  
**Date**: 2026-06-13

---

## Decision 1: Checkbox QComboBox That Stays Open

**Decision**: Custom `QStyledItemDelegate` overriding `paint()` + `editorEvent()`; no QComboBox subclassing.

**Rationale**: Qt's popup closes on `activated` signal. The delegate intercepts mouse clicks at the item level, toggles `Qt::CheckStateRole` on the model, and returns `true` (event consumed) — preventing the default selection-commit path that would close the popup. This is the standard Qt-forum-approved pattern for multi-check combos.

**Alternatives considered**:
- Subclassing QComboBox + overriding `showPopup` / `hidePopup` → more invasive, harder to maintain
- Using a QMenu with QWidgetAction items → different visual style, harder to match native combobox look
- Installing an event filter on `comboBox->view()` → works but couples teardown logic across objects

---

## Decision 2: Sentinel Value for Browse… Entry

**Decision**: Store `"::browse::"` as `Qt::UserRole` data; detect in `onComboActivated`.

**Rationale**: Simple, zero-overhead, readable. No subclassing, no custom model. The sentinel string is an internal implementation detail not visible to the user.

**Alternatives considered**:
- Index 0 hardcoded check → breaks if list reorders
- Separate `QAction` or button next to combo → extra widget, wastes top bar space

---

## Decision 3: Recent DB Persistence

**Decision**: Second `fastrace::RecentFiles` instance with a distinct storage path (`db-recent.json` alongside the trace recent file).

**Rationale**: `RecentFiles` already handles all serialization, deduplication, and ordering. Zero new code needed beyond a second constructor call with a different path argument.

**Alternatives considered**:
- Extend `RecentFiles` with a "kind" field → unnecessary complexity
- Hard-code DB paths in a QSettings key → inconsistent with existing trace-file pattern

---

## Decision 4: Multi-file Dialog for DB Browse…

**Decision**: `QFileDialog::getOpenFileNames()` (returns `QStringList`).

**Rationale**: User explicitly requested ability to select one or more DB files in one dialog. Native Qt API, no custom dialog needed.

**Alternatives considered**:
- Open dialog in a loop (one file at a time) → poor UX
- Custom file browser widget → excessive scope

---

## Decision 5: CheckboxDelegate Placement

**Decision**: Define `CheckboxItemDelegate` as a private class at the top of `TopBarWidget.cpp` (not a separate header/source pair).

**Rationale**: The delegate is tightly coupled to `cmbDatabase` and has no value outside `TopBarWidget`. Keeping it in the same translation unit avoids header pollution and extra CMake entries.

**Alternatives considered**:
- Separate CheckboxItemDelegate.h/.cpp → unnecessary for a ~40-line private helper
- Lambda-based approach → Qt's delegate interface is virtual, not signal-based

---

## Decision 6: LeftPanelWidget Build Exclusion

**Decision**: Remove the three `LeftPanelWidget` entries from `ui/CMakeLists.txt`; keep source files on disk.

**Rationale**: User explicitly requested source files be kept. CMake exclusion is the cleanest way to stop compilation without deletion. Files remain available for future reference or partial re-integration.

**Alternatives considered**:
- `#ifdef` guard in source → messy, leaves dead code in active build
- Moving files to an `_archive/` subfolder → unnecessary file system churn
