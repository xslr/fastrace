# Research: Analyzer Reuse

## Technical Context
We need to reuse the `Analyzer` instance when loading new traces to ensure UI widgets retaining `shared_ptr<Analyzer>` don't lose connection to the actively loaded trace data. Additionally, we need to disable the UI trace loading selection while an asynchronous load is ongoing to prevent invalid state.

## Decisions

### Decision 1: Analyzer Reset Method
- **Decision**: Add a `reset()` method to the existing `fastrace::Analyzer` class.
- **Rationale**: Reusing the existing object avoids having to update every widget's `shared_ptr` every time a new trace is loaded. The `reset()` method will clear internal data (`chunkIndex_`, `histogram_`, `messages`, `totalMessages_`, `m_arDatabase_`) and reset atomics.
- **Alternatives considered**: Swapping the `shared_ptr` reference. This is what was currently done and caused issues because widgets copy the `shared_ptr` to retain access to the trace. Updating all widgets dynamically would be fragile.

### Decision 2: UI Disabled State during Load
- **Decision**: Implement `setTraceComboEnabled(bool)` in `TopBarWidget` and use it to disable the trace dropdown during active loads.
- **Rationale**: Prevents users from initiating a second async load while one is ongoing. The disabled state provides native visual feedback.
- **Alternatives considered**: Queuing loads or silently dropping them. Disabling the UI is more intuitive and immediately conveys that the application is busy.
