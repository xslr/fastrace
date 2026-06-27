# Data Model: Single Database Load

**Feature**: 004-single-db-load  
**Date**: 2026-06-27

---

## Entities

### Active Database (was: Set of Active Databases)

Represents the single signal database file currently loaded into the analyzer.

| Attribute      | Type    | Source                          | Notes                               |
|----------------|---------|---------------------------------|-------------------------------------|
| `path`         | QString | User selection / recent list    | Absolute file path; empty = none    |
| `filename`     | QString | Derived from `path`             | Display name in combo entry         |
| `sizeBytes`    | int64_t | `RecentFiles` entry             | Used for `filename · size · date`   |
| `modTimeUnix`  | int64_t | `RecentFiles` entry             | Used for `filename · size · date`   |

**State transitions**:
```
[None] ──(user selects / browse)──► [Loading] ──(load complete)──► [Loaded]
[Loaded] ──(user selects new)──► [Cancelling] ──► [Loading] ──► [Loaded]
[Loaded / Loading] ──(empty path emitted)──► [None]
```

---

### Recent Database List

An ordered sequence of up to 10 recently used database entries, persisted between sessions via `RecentFiles`.

| Attribute   | Type    | Notes                                      |
|-------------|---------|--------------------------------------------|
| `path`      | string  | Absolute file path (std::string internally)|
| `filename`  | string  | Basename of path                           |
| `sizeBytes` | int64_t | File size at time of last access           |
| `modTimeUnix`| int64_t| Modification timestamp at last access      |

**Invariants**:
- Maximum 10 entries.
- Entries are ordered most-recently-used first.
- Selecting a file already in the list promotes it to position 0.

---

## State held in `TopBarWidget`

| Member (before)            | Member (after)      | Type    | Notes                                      |
|----------------------------|---------------------|---------|--------------------------------------------|
| `m_recentDbs`              | `m_recentDbs`       | `RecentFiles` | Unchanged — provides persistence     |
| `m_activeDbPaths`          | *(removed)*         | `QSet<QString>` | Deleted; replaced by current index   |
| *(none)*                   | Current item index is tracked by `QComboBox` itself | — | Standard combo behaviour |

The "currently selected" database path is now read back from the combo via `ui->cmbDatabase->currentData()` when needed, or passed directly in `openDatabase()`.

---

## State held in `MainWindow`

| Member (before)       | Member (after)       | Type    | Notes                                      |
|-----------------------|----------------------|---------|--------------------------------------------|
| `m_currentDbPaths`    | `m_currentDbPath`    | `QString` | Replaces `QStringList`; empty = none     |

Usage in `onLoadFinished`: if `!m_currentDbPath.isEmpty()` → call `onDatabaseSelectionChanged(m_currentDbPath)`.

---

## Signal Contract Change

| | Before | After |
|--|--------|-------|
| Signal | `databaseSelectionChanged(const QStringList& paths)` | `databaseSelectionChanged(const QString& path)` |
| Meaning | List of 0..N active paths | Single path, or empty string if none |
| Slot in MainWindow | `onDatabaseSelectionChanged(const QStringList& paths)` | `onDatabaseSelectionChanged(const QString& path)` |

