# Data Model: Top Bar UI Refactor

**Feature**: 001-topbar-db-ui-refactor  
**Date**: 2026-06-13

---

## Entities

### RecentTraceEntry (existing, unchanged)
- `path` — absolute file path (string)
- `filename` — basename (string, derived)
- `sizeBytes` — file size in bytes (int64)
- `modTimeUnix` — last-modified Unix timestamp (int64)

**Used by**: `cmbTraceFile` (existing `populateRecentCombo()`)

---

### RecentDbEntry (new, mirrors RecentTraceEntry)
- `path` — absolute file path (string)
- `filename` — basename (string, derived)
- `sizeBytes` — file size in bytes (int64)
- `modTimeUnix` — last-modified Unix timestamp (int64)

**Persistence**: `fastrace::RecentFiles` instance pointing to `db-recent.json`  
**Used by**: `cmbDatabase` (`populateDbCombo()`)

---

### ActiveDatabaseSet (runtime state, not persisted)
- `paths` — ordered list of currently-checked DB file paths (QStringList)
- **Owner**: `TopBarWidget::m_activeDbPaths` (QSet<QString>)
- **Lifetime**: session only; reconstructed from checked combo items
- **State transitions**:
  - Entry added (Browse…): path added to set → signal emitted
  - Entry checked: path added to set → signal emitted
  - Entry unchecked: path removed from set → signal emitted
  - All unchecked: empty set → signal emitted

---

### ComboSentinelItem (logical, not persisted)
- `displayText` — "📂 Browse…"
- `userData` — `"::browse::"` (sentinel string stored as Qt::UserRole)
- **Position**: always index 0 in both `cmbTraceFile` and `cmbDatabase`
- **Check state**: N/A (not checkable; delegate skips checkbox rendering for index 0)

---

## State Machine: cmbDatabase

```
[Empty / no recents]
  → user selects Browse…
  → QFileDialog::getOpenFileNames()
  → files added as checked entries
  → databaseSelectionChanged([paths]) emitted

[Has entries, all unchecked]
  → user checks entry N
  → entry N CheckStateRole → Checked
  → popup stays open
  → databaseSelectionChanged([pathN]) emitted

[Has entries, some checked]
  → user unchecks entry N
  → entry N CheckStateRole → Unchecked
  → popup stays open
  → databaseSelectionChanged([remaining paths]) emitted

[User clicks outside popup]
  → popup closes
  → no additional signal (state already emitted on each toggle)
```

---

## State Machine: cmbTraceFile (modified)

```
[No recents, only Browse… entry]
  → user selects Browse…
  → QFileDialog::getOpenFileName()
  → cancelled: reset to index -1, no signal
  → confirmed: openTrace(path) → traceFileChanged(path) emitted
              → file added to recent list
              → combo shows new file as current

[Has recents]
  → user selects recent entry
  → openTrace(path) → traceFileChanged(path) emitted (unchanged behaviour)
```
