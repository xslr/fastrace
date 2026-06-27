# Implementation Plan: Single Database Load for TopBarWidget

**Branch**: `004-single-db-load` | **Date**: 2026-06-27 | **Spec**: [spec.md](spec.md)  
**Input**: Feature specification from `/specs/004-single-db-load/spec.md`

## Summary

Replace the multi-select, checkbox-based database combo (`cmbDatabase`) in `TopBarWidget` with a single-select combo modelled exactly on `cmbTraceFile`. One database file is active at a time; selecting a new one replaces the previous. The custom `DatabaseComboBox` widget and all multi-select scaffolding are removed. The `databaseSelectionChanged` signal is simplified from `QStringList` to `QString`.

## Technical Context

**Language/Version**: C++17 (existing project standard)  
**Primary Dependencies**: Qt 6 — `QComboBox`, `QFileDialog`, `QSignalBlocker`, `QtConcurrent` (unchanged)  
**Storage**: `RecentFiles` (existing custom persistence class) — retained as-is  
**Testing**: Manual build and run (no automated UI test framework in scope)  
**Target Platform**: Linux desktop (existing)  
**Project Type**: Desktop application (Qt 6 Widgets)  
**Performance Goals**: No additional latency introduced; startup restoration is queued/async  
**Constraints**: Must not break existing trace-file loading, mode switching, or DB progress-bar behaviour  
**Scale/Scope**: 6 files modified, 2 files deleted — contained refactor

## Constitution Check

*Constitution template is unpopulated — no project-specific gates to enforce.*  
*Standard quality gates applied:*

- ✅ No new dependencies introduced
- ✅ Change is self-contained within the UI module
- ✅ No breaking changes to the `fastrace_analyzer` library interface
- ✅ Existing behaviour (trace file, mode switching, progress bar) preserved

## Project Structure

### Documentation (this feature)

```text
specs/004-single-db-load/
├── plan.md              ← this file
├── research.md          ← Phase 0 (complete)
├── data-model.md        ← Phase 1 (complete)
└── tasks.md             ← Phase 2 (/speckit-tasks command)
```

### Source Code — files touched

```text
ui/
├── CMakeLists.txt                        ← remove DatabaseComboBox entries
├── include/
│   ├── DatabaseComboBox.h                ← DELETE
│   ├── MainWindow.h                      ← update slot + member type
│   └── TopBarWidget.h                    ← remove m_activeDbPaths; update signal + methods
└── src/
    ├── DatabaseComboBox.cpp              ← DELETE
    ├── MainWindow.cpp                    ← update slot body + m_currentDbPaths → m_currentDbPath
    ├── TopBarWidget.cpp                  ← major refactor (see implementation phases below)
    └── TopBarWidget.ui                   ← swap DatabaseComboBox → QComboBox; remove customwidgets block
```

## Implementation Phases

---

### Phase A — Delete `DatabaseComboBox`

**Goal**: Remove the now-unused custom widget class.

**Steps**:
1. Delete `ui/include/DatabaseComboBox.h`.
2. Delete `ui/src/DatabaseComboBox.cpp`.
3. In `ui/CMakeLists.txt`:
   - Remove lines:
     ```
     # ── Custom DB combobox ────────────────────────────────────────────────────
     src/DatabaseComboBox.cpp
     include/DatabaseComboBox.h
     ```
   - Update comment on `target_include_directories` (line 85) to remove reference to `DatabaseComboBox.h` — the directive itself stays (still needed for `ui_TopBarWidget.h`).

**Verification**: `cmake --build` must succeed without referencing `DatabaseComboBox`.

---

### Phase B — Update `TopBarWidget.ui`

**Goal**: Swap the promoted `DatabaseComboBox` widget for a standard `QComboBox`.

**Steps**:
1. Change line 67 of `TopBarWidget.ui`:
   ```xml
   <!-- Before -->
   <widget class="DatabaseComboBox" name="cmbDatabase">
   <!-- After -->
   <widget class="QComboBox" name="cmbDatabase">
   ```
2. Remove the entire `<customwidgets>` block at the bottom of the file (lines 182–188).

**Verification**: `AUTOUIC` generates `ui_TopBarWidget.h` without reference to `DatabaseComboBox`.

---

### Phase C — Refactor `TopBarWidget.h`

**Goal**: Update header to reflect single-select model.

**Changes**:
1. Remove `#include <QSet>` (if present — it may be transitively included; remove explicit include if exists).
2. Remove private member `QSet<QString> m_activeDbPaths;`.
3. Remove private method declaration `void updateDbComboDisplay();`.
4. Remove private method declaration `void emitDbSelectionChanged();`.
5. Add private method declaration `void openDatabase(const QString& path);`.
6. Change signal: `void databaseSelectionChanged(const QStringList& paths);` → `void databaseSelectionChanged(const QString& path);`.

---

### Phase D — Refactor `TopBarWidget.cpp`

**Goal**: Replace multi-select logic with single-select logic mirroring `cmbTraceFile`.

**Changes**:

#### D1. Remove dead code
- Delete the entire `CheckboxItemDelegate` class (lines 17–74).
- Delete `emitDbSelectionChanged()` function body.
- Delete `updateDbComboDisplay()` function body.

#### D2. Constructor cleanup
Remove from constructor:
```cpp
// Remove these two lines:
ui->cmbDatabase->setItemDelegate(new CheckboxItemDelegate(this));
ui->cmbDatabase->view()->setEditTriggers(QAbstractItemView::NoEditTriggers);

// Remove active-path restoration block:
for (const auto& p : m_recentDbs.getActivePaths()) {
    m_activeDbPaths.insert(QString::fromStdString(p));
}
// Re-populate so check states reflect restored active set
populateDbCombo();
updateDbComboDisplay();

// Remove queued emit:
QMetaObject::invokeMethod(this, [this] { emitDbSelectionChanged(); }, Qt::QueuedConnection);
```

Add startup restoration (after `populateDbCombo()`):
```cpp
// Restore last selected database (most recent entry)
const auto recent = m_recentDbs.getRecent(1);
if (!recent.empty()) {
    const QString lastPath = QString::fromStdString(recent[0].path);
    if (QFileInfo::exists(lastPath)) {
        QMetaObject::invokeMethod(this, [this, lastPath] { openDatabase(lastPath); },
                                  Qt::QueuedConnection);
    }
}
```

Remove unused `#include "DatabaseComboBox.h"`.

#### D3. Simplify `populateDbCombo()`
Replace current body with:
```cpp
void TopBarWidget::populateDbCombo()
{
    QSignalBlocker blocker(ui->cmbDatabase);
    ui->cmbDatabase->clear();

    // Add Browse entry at index 0
    ui->cmbDatabase->addItem(tr("📂 Browse…"), QString("::browse::"));

    for (const auto& entry : m_recentDbs.getRecent(10)) {
        const QString text = QString::fromStdString(entry.filename)
            + "  ·  " + formatSize(entry.sizeBytes)
            + "  ·  " + formatDate(entry.modTimeUnix);
        ui->cmbDatabase->addItem(text, QString::fromStdString(entry.path));
    }
    ui->cmbDatabase->setCurrentIndex(-1);
}
```

#### D4. Add `openDatabase()`
```cpp
void TopBarWidget::openDatabase(const QString& path)
{
    m_recentDbs.addFile(path.toStdString());
    populateDbCombo();

    // Select the entry without triggering activated
    {
        QSignalBlocker blocker(ui->cmbDatabase);
        const int idx = ui->cmbDatabase->findData(path);
        if (idx >= 0) {
            ui->cmbDatabase->setCurrentIndex(idx);
        }
    }

    emit databaseSelectionChanged(path);
}
```

#### D5. Simplify `onDbComboActivated()`
Replace entire body:
```cpp
void TopBarWidget::onDbComboActivated(int index)
{
    if (index < 0) {
        return;
    }
    const QString path = ui->cmbDatabase->itemData(index).toString();

    if (path == "::browse::") {
        const QString picked = QFileDialog::getOpenFileName(
            this, tr("Open Signal Database"), QString(),
            tr("Database Files (*.arxml *.dbc);;All Files (*)"));

        if (picked.isEmpty()) {
            ui->cmbDatabase->setCurrentIndex(-1);
            return;
        }
        openDatabase(picked);
    } else if (!path.isEmpty()) {
        openDatabase(path);
    }
}
```

#### D6. Remove includes that are no longer needed
- `#include <QAbstractItemView>` — only used by `setEditTriggers`; remove if no other usage.
- `#include <QMouseEvent>` — only used by `CheckboxItemDelegate`; remove.
- `#include <QStyledItemDelegate>` — only used by `CheckboxItemDelegate`; remove.
- `#include "DatabaseComboBox.h"` — remove.
- Retain: `QFileDialog`, `QFileInfo`, `QSignalBlocker`, `QComboBox`, `QApplication` (check), `QDateTime`.

---

### Phase E — Update `MainWindow.h`

**Changes**:
1. Change slot: `void onDatabaseSelectionChanged(const QStringList& paths);` → `void onDatabaseSelectionChanged(const QString& path);`
2. Change member: `QStringList m_currentDbPaths;` → `QString m_currentDbPath;`

---

### Phase F — Update `MainWindow.cpp`

**Changes**:

#### F1. Update `connect()` call
The signal/slot types change, so the `connect()` auto-connects correctly via the new signature (no other change needed in line 141–142).

#### F2. Update `onDatabaseSelectionChanged()`
```cpp
void MainWindow::onDatabaseSelectionChanged(const QString& path)
{
    m_currentDbPath = path;

    if (path.isEmpty()) {
        m_analyzer->clearDatabase();
        m_messageDetails->refreshSignalDecode();
        return;
    }

    if (m_dbWatcher->isRunning()) {
        m_analyzer->dbLoadCancelled.store(true, std::memory_order_relaxed);
        m_dbWatcher->waitForFinished();
    }
    m_analyzer->dbLoadCancelled.store(false, std::memory_order_relaxed);

    m_topBar->setDatabaseComboEnabled(false);
    m_topBar->setDbLoadProgress(0.0f);
    m_dbPollTimer->start();

    const std::string stdPath = path.toStdString();
    auto future = QtConcurrent::run([analyzer = m_analyzer, stdPath]() {
        analyzer->loadDatabase(stdPath);
    });
    m_dbWatcher->setFuture(future);
}
```

#### F3. Update `onLoadFinished()`
Change:
```cpp
// Before:
if (!m_currentDbPaths.isEmpty()) {
    onDatabaseSelectionChanged(m_currentDbPaths);
}
// After:
if (!m_currentDbPath.isEmpty()) {
    onDatabaseSelectionChanged(m_currentDbPath);
}
```

#### F4. Update `startLoad()`
The only mention of `m_currentDbPaths` in `startLoad` is none — verify no other references remain and update any found.

---

## Complexity Tracking

No constitution violations. This is a simplification (net code reduction: ~100 lines removed, ~40 lines added).

