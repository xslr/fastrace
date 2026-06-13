# UI Contracts: TopBarWidget Signals

**Feature**: 001-topbar-db-ui-refactor  
**Date**: 2026-06-13  
**Scope**: Qt signal contracts exposed by `TopBarWidget` to `MainWindow` and downstream consumers

---

## Signal: traceFileChanged (existing, unchanged)

```cpp
void traceFileChanged(const QString& path);
```

| Property | Detail |
|----------|--------|
| Emitted when | User selects a recent trace entry OR confirms Browse… dialog |
| NOT emitted when | User cancels Browse… dialog |
| `path` | Absolute path to the selected trace file; never empty when signal fires |
| Consumer | `MainWindow` → `MessageListWidget::loadFile()` (Overview + Notebook) |

---

## Signal: databaseSelectionChanged (NEW)

```cpp
void databaseSelectionChanged(const QStringList& paths);
```

| Property | Detail |
|----------|--------|
| Emitted when | Any DB entry is checked/unchecked, or new files added via Browse… |
| NOT emitted when | Browse… dialog is cancelled |
| `paths` | Ordered list of absolute paths of all currently-checked DB entries; may be empty |
| Consumer | `MainWindow` (or higher-level DB manager) connects to this to drive `SignalDatabases` / `ArxmlParser` |
| Guarantees | `TopBarWidget` never calls `SignalDatabases` or `ArxmlParser` itself |

---

## Signal: modeChanged (existing, unchanged)

```cpp
void modeChanged(TopBarWidget::ViewMode mode);
```

No changes. Documented here for completeness.

---

## Removed: btnSelectDb clicked / btnOpen clicked

Both buttons are removed. Their functionality is replaced by the signals above. Any existing connect() calls to these buttons in `MainWindow` must be removed.

| Removed Connection | Replacement |
|-------------------|-------------|
| `btnOpen::clicked` → file open logic | Browse… entry in `cmbTraceFile` → `traceFileChanged` |
| `btnSelectDb::clicked` → DB open logic | `cmbDatabase` Browse… + checkboxes → `databaseSelectionChanged` |

---

## OverviewView Constructor Contract (CHANGED)

**Before**:
```cpp
OverviewView(LeftPanelWidget* leftPanel,
             TimelineWidget* sharedTimeline,
             MessageDetailsWidget* messageDetails,
             DetectionsWidget* detectionsWidget,
             QWidget* parent = nullptr);
```

**After**:
```cpp
OverviewView(TimelineWidget* sharedTimeline,
             MessageDetailsWidget* messageDetails,
             DetectionsWidget* detectionsWidget,
             QWidget* parent = nullptr);
```

All callers (only `MainWindow::MainWindow()`) must be updated to remove the `leftPanel` argument.
