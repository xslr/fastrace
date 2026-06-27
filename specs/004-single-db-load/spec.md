# Feature Specification: Single Database Load for TopBarWidget

**Feature Branch**: `004-single-db-load`  
**Created**: 2026-06-27  
**Status**: Draft  
**Input**: User description: "Modify the database loading UI (cmbDatabase) in TopBarWidget to load only a single database at a time. This will be similar to how cmbTraceFile is used for loading trace files."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Select a Database from Recent List (Priority: P1)

A user opens the application, clicks the database combo box, and sees a list of recently used database files. Selecting one immediately loads it, replacing any previously loaded database. The combo box displays the selected file's name, size, and date — just like the trace file combo.

**Why this priority**: This is the primary interaction path. Users most often reload a recently used database, so it must work cleanly and be visually consistent with the trace file selector.

**Independent Test**: Can be fully tested by launching the app with existing recent databases, selecting one from the dropdown, and verifying the database loads and the UI shows the correct entry.

**Acceptance Scenarios**:

1. **Given** recent databases exist, **When** the user opens the database combo, **Then** entries are shown formatted as `filename · size · date` with a "📂 Browse…" entry at the top.
2. **Given** a recent database is selected, **When** the selection is made, **Then** the previously loaded database is replaced, the combo shows the selected entry, and the downstream signal carries exactly one path.
3. **Given** a database is already loaded, **When** the user selects a different database from the list, **Then** the new database replaces the old one without requiring a manual unload step.

---

### User Story 2 - Browse and Load a New Database File (Priority: P1)

A user who has not yet used a database, or who wants to load a new one not in the recent list, selects the "📂 Browse…" entry. A single-file open dialog appears filtered to `.arxml` and `.dbc` files. After picking a file, it loads as the active database, is added to the recent list, and the combo shows it.

**Why this priority**: Equally critical as recent selection — it is the entry point for first-time use and for loading files outside the history.

**Independent Test**: Can be fully tested by clearing recent databases, selecting Browse, picking a file via dialog, and confirming the database loads and appears in the recent list on next launch.

**Acceptance Scenarios**:

1. **Given** the user selects "📂 Browse…", **When** a file is chosen in the dialog, **Then** the file is loaded as the single active database, the combo displays its entry, and it appears in the recent list.
2. **Given** the user selects "📂 Browse…", **When** the dialog is cancelled, **Then** no database change occurs and the combo resets to its previous state.
3. **Given** a file is chosen via Browse, **When** the app is restarted, **Then** the file appears in the recent database list.

---

### User Story 3 - Persist and Restore Last Selected Database (Priority: P2)

When the application is closed and reopened, the previously selected database is automatically restored as the active database. The combo shows the restored entry on startup.

**Why this priority**: Reduces repetitive setup. Users typically work with the same database across sessions.

**Independent Test**: Can be fully tested by selecting a database, closing the app, relaunching, and confirming the database is restored and the combo reflects the correct selection.

**Acceptance Scenarios**:

1. **Given** a database was selected in a previous session, **When** the app launches, **Then** the previously selected database is loaded automatically and the combo shows its entry.
2. **Given** the previously selected database file has been deleted, **When** the app launches, **Then** the combo defaults to no selection (placeholder text shown) and no error is shown to the user.

---

### User Story 4 - Signal Integration (Priority: P2)

When the active database changes (or is cleared), downstream components (e.g., signal decode view) are notified with the single selected path. If no database is selected, they receive an empty/cleared notification.

**Why this priority**: Required for the feature to integrate correctly with the rest of the application; without it the database combo change has no effect on signal decoding.

**Independent Test**: Can be tested by verifying that selecting a database triggers signal decode to update, and that clearing the selection correctly clears decoded signals.

**Acceptance Scenarios**:

1. **Given** a database is selected, **When** the selection changes, **Then** exactly one path is communicated to the rest of the application.
2. **Given** no database is selected, **When** the app initialises, **Then** the application receives a clear/empty notification.

---

### Edge Cases

- What happens when the selected database file no longer exists on disk at launch time? → Combo defaults to no selection; no crash or error dialog.
- What happens if the same file is selected again (re-selected from the recent list)? → The database reloads (same behaviour as selecting a new one).
- What happens if a database load is in progress and the user selects a new database? → The in-flight load is cancelled and the new database load starts (consistent with existing cancellation logic).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The database combo MUST allow selection of exactly one database file at a time; selecting a new file replaces the previously active one.
- **FR-002**: The database combo MUST include a "📂 Browse…" entry that opens a single-file open dialog filtered to `.arxml` and `.dbc` files.
- **FR-003**: The database combo MUST display recent database entries formatted as `filename · size · date`, consistent with the trace file combo display.
- **FR-004**: The database combo MUST limit the recent list to the 10 most recently used files.
- **FR-005**: When the user selects a database, the file MUST be added to (or promoted in) the recent database list and persisted for future sessions.
- **FR-006**: The last selected database MUST be restored automatically when the application launches.
- **FR-007**: When the active database changes, the system MUST notify downstream components with a single file path (or an empty value if no database is selected).
- **FR-008**: The database combo's collapsed state MUST display the selected file's name (or a placeholder when nothing is selected), not a count of selected items.
- **FR-009**: All multi-select UI elements (checkboxes, checkbox delegate, active-count display) MUST be removed from the database combo.
- **FR-010**: The custom `DatabaseComboBox` widget class MUST be removed and replaced with a standard combo box widget.
- **FR-011**: The `databaseSelectionChanged` signal MUST carry a single file path string (empty string when no database is selected) instead of a list.
- **FR-012**: All code managing a set of active database paths (`m_activeDbPaths`) MUST be removed.

### Key Entities

- **Active Database**: The single database file currently loaded. Has a file path, filename, size, and modification date. Replaces the concept of a "set of active databases".
- **Recent Database List**: An ordered list of up to 10 previously used database file paths, persisted between sessions.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user can select and load a database file from the recent list in 2 interactions (open combo → click entry).
- **SC-002**: A user can browse and load a new database file in 3 interactions (open combo → click Browse → pick file).
- **SC-003**: The selected database is restored within the normal application startup time — no additional wait is introduced.
- **SC-004**: Selecting a database via the combo produces exactly one downstream notification carrying one path; no multi-path lists are emitted.
- **SC-005**: No multi-select UI artifacts (checkboxes, count labels) are visible in the database combo.
- **SC-006**: The database combo is visually and behaviourally consistent with the trace file combo (same entry format, same browse-and-load flow).

## Assumptions

- The `m_recentDbs` (`RecentFiles`) persistence mechanism is retained as-is; only the active-set tracking (`m_activeDbPaths`) is removed.
- "Restoring the last selected database" means restoring the most recently selected single path from the `RecentFiles` store (the first item in the recent list).
- If the most recent database file is missing at launch, the app silently skips restoration rather than showing an error dialog.
- Cancellation of an in-flight database load when a new selection is made follows the existing cancellation pattern already present in `MainWindow`.
- The progress bar (`dbLoadProgress`) and the `setDatabaseComboEnabled`/`setDbLoadProgress` API on `TopBarWidget` are retained unchanged — they still serve the single-database load progress display.
- `MainWindow::m_currentDbPaths` (a `QStringList`) is simplified to a single `QString` (`m_currentDbPath`).
- The `DatabaseComboBox.h` and its corresponding `.cpp` source file are deleted from the project and removed from the build system.
- Mobile and accessibility support are out of scope for this change.
