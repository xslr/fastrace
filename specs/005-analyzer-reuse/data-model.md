# Data Model: Analyzer Reuse

No new entities are introduced. The existing `fastrace::Analyzer` class is extended with a `reset()` method to clear its internal state.

## Entities

### `fastrace::Analyzer`

The core engine holding all loaded state for a trace file and signal database.

**Fields (Existing)**:
- `chunkIndex_` (std::vector<ChunkEntry>)
- `histogram_` (HistogramData)
- `totalMessages_` (size_t)
- `m_arDatabase_` (ArDatabase)
- `messages` (std::vector<TraceMessage>)
- Atomics: `cancelled`, `histogramCancelled`, `histogramChunksProcessed`, `bytesRead`, `totalBytes`, `messagesCollected`, `dbLoadProgress`, `dbLoadCancelled`

**New Method**:
- `void reset()`: Clears all standard containers, resets all atomic counters to 0, resets `m_arDatabase_`, and resets `cancelled` flags to `false`.

## Interface Contracts

No public external API changes. The internal C++ API for `Analyzer` is updated with the `reset()` method. `TopBarWidget` is updated with `setTraceComboEnabled(bool)`.
