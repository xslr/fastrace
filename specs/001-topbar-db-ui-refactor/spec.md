# Feature Specification: Top Bar UI Refactor — DB Combobox, Trace Browse, Left Panel Removal

**Feature Branch**: `001-topbar-db-ui-refactor`  
**Created**: 2026-06-13  
**Status**: Draft  
**Input**: User description: "Remove left sidebar (m_leftPanel). Replace btnSelectDb with a combobox to load a database (similar to how cmbTraceFile works). Remove btnOpen — instead add an option to cmbTraceFile which will show open file dialog to select trace file for loading."

---

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Load a Trace File via Browse (Priority: P1)

A user opens the application and wants to load a trace file they haven't loaded before. They click the trace file combobox, choose the "📂 Browse…" entry at the top of the list, a file picker dialog opens, they select their file, and the trace loads immediately. The file is added to the combobox's recent list for future quick access.

**Why this priority**: This is the primary way new or infrequent files are loaded. The old `Open` button is removed; this path must work from day one.

**Independent Test**: Can be fully tested by launching the app with no recent trace history, selecting Browse…, picking a valid BLF file, and confirming the trace loads and the file appears in the recent list.

**Acceptance Scenarios**:

1. **Given** the app has no recent trace files, **When** the user opens the trace combobox, **Then** the "📂 Browse…" entry is visible at the top of the list.
2. **Given** the user selects "📂 Browse…", **When** they pick a file in the dialog, **Then** the trace loads immediately and the file is added to the recent-files list.
3. **Given** the user cancels the file dialog, **When** they dismiss it without selecting a file, **Then** the combobox resets to its previous state and no trace is loaded.
4. **Given** the app has recent trace files, **When** the user opens the combobox, **Then** "📂 Browse…" remains at the top above all recent entries.

---

### User Story 2 — Load a Recent Trace File (Priority: P1)

A user who has previously loaded trace files opens the combobox and clicks a recent entry. The trace loads immediately without any dialogs.

**Why this priority**: Equally critical as browsing — returning users rely on this for daily workflow efficiency.

**Independent Test**: Can be fully tested by verifying that selecting a recent entry in the combobox loads the corresponding trace without opening a file dialog.

**Acceptance Scenarios**:

1. **Given** the app has recent trace files, **When** the user selects one from the combobox, **Then** that trace loads immediately.
2. **Given** the combobox is populated with recent files, **When** it is opened, **Then** each entry shows the filename, file size, and last-modified date.

---

### User Story 3 — Select One or More Signal Databases (Priority: P2)

A user wants to load signal databases (ARXML, DBC, etc.) to decode trace messages. They open the database combobox, which shows a list of previously used databases each with a checkbox. They check/uncheck entries to control which databases are active. The dropdown stays open while they toggle multiple entries. When done, they click elsewhere to close it. A "📂 Browse…" entry at the top opens a file dialog to add new DB files.

**Why this priority**: DB selection is secondary to trace loading but essential for message decoding. The left panel that previously handled this is removed.

**Independent Test**: Can be fully tested by opening the DB combobox, checking two entries, clicking outside, and confirming both databases are active (i.e., a databaseSelectionChanged signal carrying both paths is emitted).

**Acceptance Scenarios**:

1. **Given** the DB combobox is open, **When** the user checks a database entry, **Then** a checkmark appears on that entry and the dropdown remains open.
2. **Given** the DB combobox is open with multiple entries checked, **When** the user clicks outside the dropdown, **Then** the dropdown closes and the active database set reflects all checked entries.
3. **Given** the user selects "📂 Browse…" in the DB combobox, **When** they pick one or more files, **Then** those files are added to the combobox list as checked entries and are immediately active.
4. **Given** the DB combobox has entries, **When** the user unchecks all entries, **Then** no databases are active and the system notifies downstream components with an empty selection.
5. **Given** the user cancels the Browse dialog, **When** they dismiss without selecting, **Then** the DB list and active set remain unchanged.

---

### User Story 4 — Use App Without Left Sidebar (Priority: P1)

A user opens the app and the left sidebar (formerly showing trace summary, messages, signals, ECUs, SOME/IP tabs and database list) is no longer present. The layout uses the full width for the Centre panel (Timeline + Message List) and the Right panel (Detections + Message Details).

**Why this priority**: The left panel removal affects the fundamental layout visible to every user on every launch.

**Independent Test**: Can be fully tested by launching the app and confirming only two visible panels exist — Centre and Right — with no left-panel column.

**Acceptance Scenarios**:

1. **Given** the app launches, **When** the overview is shown, **Then** no left sidebar is visible.
2. **Given** the app is in overview mode, **When** the window is resized, **Then** the Centre and Right panels resize proportionally without any gap or empty column where the left panel was.
3. **Given** the app is in notebook mode, **When** the view is active, **Then** the layout is unaffected (no leftover left-panel references).

---

### Edge Cases

- What happens when a trace file in the recent list has been deleted from disk? The entry should remain in the combobox but loading it should surface an informative error.
- What happens if a DB file is checked in the combobox but its path no longer exists on disk? The system should report the missing file rather than silently failing to decode.
- What happens if the user selects Browse… in cmbDatabase and picks files already present in the recent list? Duplicates are not added; the existing entry is checked instead.
- What happens if the file dialog for Browse… in cmbTraceFile is opened while a trace is already loading? The UI should remain responsive.

---

## Requirements *(mandatory)*

### Functional Requirements

**Left Panel Removal**

- **FR-001**: The application MUST NOT display the left sidebar panel (formerly LeftPanelWidget) in any view.
- **FR-002**: The overview layout MUST consist of exactly two horizontal panels: Centre (Timeline above Message List) and Right (Detections above Message Details).
- **FR-003**: All functionality previously provided exclusively by the left panel (trace summary, messages/signals/ECUs/SOME-IP tabs) is considered out of scope for this feature and may be addressed separately.

**Trace File Combobox (cmbTraceFile) Changes**

- **FR-004**: The Open button (btnOpen) MUST be removed from the top bar.
- **FR-005**: The trace file combobox MUST include a "📂 Browse…" entry permanently positioned at the top of the dropdown list.
- **FR-006**: Selecting the "📂 Browse…" entry MUST open a file selection dialog filtered to supported trace formats.
- **FR-007**: After the user confirms a file selection, the trace MUST load immediately, the selected file MUST be added to the recent-files list, and the combobox MUST display that file as the current selection.
- **FR-008**: Cancelling the file dialog MUST return the combobox to its previous state without loading any file.
- **FR-009**: The trace file combobox MUST remain single-select — selecting any recent entry loads that trace immediately.

**Database Combobox (cmbDatabase) — Replaces btnSelectDb**

- **FR-010**: The Select DB button (btnSelectDb) MUST be replaced by a combobox (cmbDatabase).
- **FR-011**: Each database entry in cmbDatabase MUST display a checkbox, allowing individual activation/deactivation.
- **FR-012**: The cmbDatabase dropdown MUST remain open after the user checks or unchecks an entry, closing only when the user interacts outside the dropdown.
- **FR-013**: cmbDatabase MUST include a "📂 Browse…" entry at the top that opens a file dialog supporting one or more file selections.
- **FR-014**: Files selected through Browse… MUST be added to the combobox list in a checked (active) state and appended to the recent-DB list.
- **FR-015**: When the active database set changes (entries checked/unchecked or new files added), the top bar MUST emit a notification containing the full list of currently active database paths.
- **FR-016**: The top bar MUST NOT contain database-loading or parsing logic; it only reports which paths are selected.

### Key Entities

- **Recent Trace Entry**: A previously loaded trace file with path, filename, size (bytes), and last-modified date.
- **Recent DB Entry**: A previously selected database file with path, filename, and active/inactive state (checkbox state).
- **Active Database Set**: The ordered collection of database file paths currently checked in cmbDatabase.

---

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user with no prior trace history can load a trace file via the Browse… entry in under 30 seconds from app launch.
- **SC-002**: A returning user can load a recent trace file with a single click (no dialogs).
- **SC-003**: A user can activate or deactivate multiple databases in a single combobox interaction (open once, toggle several, close once).
- **SC-004**: The top bar occupies no more vertical space than before — all three changes fit within the existing top bar height.
- **SC-005**: The overview layout renders correctly at all supported window sizes without any blank panel or layout artifact where the left sidebar was.
- **SC-006**: 100% of database path changes are communicated to downstream components without the top bar performing any file I/O.

---

## Assumptions

- The existing RecentFiles mechanism (used for trace files) will be reused or adapted for the DB recent-file list; no new persistence infrastructure is required.
- Supported trace file formats remain BLF and PCAPNG (as currently configured in the file dialog filter).
- Supported database file formats are ARXML and DBC (as previously handled by LeftPanelWidget).
- The LeftPanelWidget source files are retained in the codebase but removed from compilation; no source deletion is required.
- SignalDatabases and ArxmlParser will remain as separate non-UI components; the top bar does not import or use them directly.
- The notebook view layout is not affected by the left panel removal (it did not include the left panel).
- The right sidebar (Detections + Message Details) is kept as-is; only the left panel is removed.
- Multi-file selection in the DB Browse… dialog (selecting multiple ARXML/DBC files at once) is supported.
