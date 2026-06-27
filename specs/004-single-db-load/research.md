# Research: Single Database Load for TopBarWidget

**Feature**: 004-single-db-load  
**Date**: 2026-06-27

---

## Decision 1: How `cmbTraceFile` single-select flow works (reference pattern)

**Decision**: Mirror the exact flow of `onComboActivated` / `openTrace` for the database combo.

**Rationale**: The trace file combo is the stated reference. It uses:
1. `populateTraceCombo()` — fills `QComboBox` with "📂 Browse…" at index 0, then up to 10 recent entries formatted as `filename · size · date`.
2. `onComboActivated(int index)` — if index 0 (Browse), opens `QFileDialog::getOpenFileName`; otherwise calls `openTrace(path)`.
3. `openTrace(path)` — adds to `m_recentFiles`, re-populates combo, sets `currentIndex` via `QSignalBlocker`, emits `traceFileChanged(path)`.

The database flow will be identical in structure, replacing `m_recentFiles`/`traceFileChanged` with `m_recentDbs`/`databaseSelectionChanged`.

**Alternatives considered**:
- Keep the existing multi-select + checkbox approach: rejected per user requirement.
- Use a `QFileDialog` button (no combo): rejected — user wants a consistent dropdown UI matching `cmbTraceFile`.

---

## Decision 2: Signal signature change `QStringList` → `QString`

**Decision**: Change `databaseSelectionChanged(const QStringList& paths)` to `databaseSelectionChanged(const QString& path)` (empty string = no database).

**Rationale**: With single-select there is exactly 0 or 1 path. `MainWindow::onDatabaseSelectionChanged` already only uses `paths[0]`; the rest of the list is ignored. Simplifying to `QString` removes the dead list overhead and makes the intent unambiguous.

**Downstream impact** (exhaustive scan of callsites):
- `TopBarWidget.h` signal declaration
- `TopBarWidget.cpp` — `emitDbSelectionChanged` → removed; new `openDatabase(path)` emits directly
- `MainWindow.h` — slot signature updated: `onDatabaseSelectionChanged(const QString& path)`
- `MainWindow.cpp` — slot body: `if (path.isEmpty()) clearDatabase() else load(path)`; `m_currentDbPaths` → `m_currentDbPath` (QString); usage in `onLoadFinished` updated

**Alternatives considered**:
- Keep `QStringList` and always send a list of 0 or 1 items: simpler diff, but leaves misleading API surface. Rejected.

---

## Decision 3: Persistence — restoring the last selected database on launch

**Decision**: Use `m_recentDbs.getRecent(1)` to obtain the most recently used database path, then call `openDatabase()` on it at startup (via `QMetaObject::invokeMethod` queued, matching the existing trace file restore pattern).

**Rationale**: `RecentFiles` already stores entries in most-recently-used order. The first entry is always the last selected file. Using `getRecent(1)` avoids the need for a separate "current" persistence key.

**Missing file handling**: Check `QFileInfo(path).exists()` before restoring; skip silently if the file is gone.

**Alternatives considered**:
- Store the last path separately in `QSettings`: adds a second persistence mechanism for the same data. Rejected.
- Restore all databases in recent list as active: contradicts the single-select requirement. Rejected.

---

## Decision 4: `DatabaseComboBox` removal strategy

**Decision**: 
1. Delete `ui/src/DatabaseComboBox.cpp` and `ui/include/DatabaseComboBox.h`.
2. In `TopBarWidget.ui`, change `<widget class="DatabaseComboBox" name="cmbDatabase">` to `<widget class="QComboBox" name="cmbDatabase">`, remove the `<customwidgets>` block.
3. Remove the `src/DatabaseComboBox.cpp` and `include/DatabaseComboBox.h` lines from `ui/CMakeLists.txt`.
4. Remove the comment block `# ── Custom DB combobox` that referenced those files.
5. Remove the `target_include_directories` comment referencing `DatabaseComboBox.h` (or update it — the include of `ui/src/` may still be needed for other auto-generated headers; keep the directive, just update the comment).

**Rationale**: The class served only to paint a "N databases" summary. With single-select, standard `QComboBox` display text is sufficient (it shows the currently selected item's text). No custom painting needed.

**Alternatives considered**:
- Keep class but stub it out: no benefit, dead code. Rejected.

---

## Decision 5: `CheckboxItemDelegate` and `m_activeDbPaths` removal

**Decision**: Remove entirely.
- `CheckboxItemDelegate` (defined file-local in `TopBarWidget.cpp`): delete the entire class definition.
- `m_activeDbPaths` (`QSet<QString>` in `TopBarWidget.h`): remove member.
- `ui->cmbDatabase->setItemDelegate(...)` and `setEditTriggers(...)` calls in constructor: remove.
- `emitDbSelectionChanged()` function: remove; replaced by direct emit in `openDatabase()`.
- `updateDbComboDisplay()` function: remove.
- `populateDbCombo()`: simplify — remove all `setItemData(..., Qt::CheckStateRole)` calls.
- `onDbComboActivated()`: simplify — remove checkbox toggle branch; replace with `openDatabase(path)` call (mirrors `onComboActivated`).

---

## Decision 6: `populateDbCombo` entry format

**Decision**: Format entries identically to `populateTraceCombo`:
```
filename  ·  size  ·  date
```
using the existing `formatSize()` and `formatDate()` helper functions already present in `TopBarWidget.cpp`.

**Rationale**: FR-003 requires visual consistency with the trace file combo. The helpers are already available in the same translation unit.

---

## Decision 7: Build system — `AnalyzerPreviewWidget` check

**Decision**: Verify that `AnalyzerPreviewWidget` does not include `DatabaseComboBox.h`. After deletion, only `TopBarWidget.cpp` and the `.ui` file reference it — both are being updated. No other consumer.

**Rationale**: A scan of all `#include "DatabaseComboBox.h"` across the `ui/` directory confirms a single occurrence in `TopBarWidget.cpp`.

