# Implementation Plan: Lazy Loading Messages

**Branch**: `002-message-lazy-load` | **Date**: 2026-06-13 | **Spec**: [spec.md](./spec.md)

---

## Summary

Replace `MessageListWidget`'s `QTableWidget` (eagerly loads all messages) with a `QTableView` backed by a new `MessageTableModel` (`QAbstractTableModel`). The model fetches messages on demand via a new `Analyzer::decodeChunk()` API. A preceding `Analyzer::buildIndex()` fast-scan produces a sparse `ChunkEntry` lookup table (one entry per 10,000 messages, stored in-memory as `std::vector<ChunkEntry>`) that enables O(log N) random access to any chunk without re-scanning the whole file.

---

## Technical Context

**Language/Version**: C++20, Qt 6  
**Primary Dependencies**: Qt6 (QAbstractTableModel, QTableView, QtConcurrent), libdeflate (already used), spdlog (already used)  
**Storage**: In-memory `std::vector<ChunkEntry>` for the index; persistent `MappedFile` mmap for on-demand decode  
**Testing**: Manual integration test with a large BLF file; no existing test framework in project  
**Target Platform**: Linux / Windows desktop (same as current app)  
**Project Type**: Desktop application  
**Performance Goals**: Initial scan ≤5 s on a 10 GB file; chunk decode ≤1 s; UI frame budget ≤16 ms (60 fps)  
**Constraints**: Peak memory ≤ 30,000 decoded `TraceMessage` structs in cache (~90 MB worst case for 64-byte data each); index ≤ 2 MB for 1 billion messages  
**Scale/Scope**: Up to ~1 billion messages per trace file

---

## Constitution Check

Constitution file is a blank template — no project-specific gates defined. No violations.

---

## Project Structure

### Documentation (this feature)

```text
specs/002-message-lazy-load/
├── plan.md              ← this file
├── research.md          ← Phase 0 output
├── data-model.md        ← Phase 1 output
├── contracts/
│   └── internal-api.md  ← Phase 1 output
└── tasks.md             ← Phase 2 output (/speckit-tasks)
```

### Source Code Changes

```text
cpp/include/
├── Analyzer.h           ← add ChunkEntry, buildIndex(), decodeChunk(), private fields
└── TraceMessage.h       ← no change

cpp/src/
└── Analyzer.cpp         ← add buildIndex(), decodeChunk() implementations

ui/src/
├── MessageTableModel.h  ← NEW: QAbstractTableModel subclass
├── MessageTableModel.cpp← NEW: model impl + async decode + cache
├── MessageListWidget.h  ← replace populateFrom with attachAnalyzer
├── MessageListWidget.cpp← wire QTableView + MessageTableModel
├── MessageListWidget.ui ← swap QTableWidget → QTableView
└── MainWindow.cpp       ← change onLoadFinished to call attachAnalyzer

ui/CMakeLists.txt        ← add MessageTableModel.cpp to sources
```

---

## Architecture Decisions

### Index scan reuses producer-consumer pipeline

`buildIndex` runs the same producer-consumer pipeline as `processFile`, but each consumer runs a **lightweight counting-only variant** instead of full `processInnerObjects`:
- Decompress container (same as today, fully parallel)
- Walk LOBJ signatures; read only `BlfObjectHeaderBase` per object
- Count message-type objects (CAN/CAN-FD/Ethernet); skip all payload bytes
- Report `{containerIndex, countInContainer}` to a shared accumulator

After all consumers finish, the main thread sorts by `containerIndex`, prefix-sums, and emits one `ChunkEntry` per `CHUNK_SIZE`-message boundary. This is why `skipMessages` exists — it captures the offset into the boundary container.

This preserves the multi-core decompression throughput. A single-threaded approach would take 20–25 s on a large file (far over SC-002), since decompression is the bottleneck.

### `decodeChunk` is thread-safe & concurrent

Chunk decode is stateless against the index (`chunkIndex_` is read-only post-build). Multiple chunks can decode in parallel from different `QThreadPool` workers since the mmap is read-only. Each worker allocates its own `libdeflate_decompressor`.

### Cache eviction policy

`MessageTableModel` keeps at most `MAX_CACHED_CHUNKS = 3` decoded chunks. On overflow, evict the chunk whose index is furthest from the most recently requested chunk index (distance-based LRU approximation, O(1) since max 3 entries).

### Split-object handling in `decodeChunk`

When seeking to a chunk that starts inside a container, `decodeChunk` must handle the case where the last LOBJ in the preceding container is split. `decodeChunk` checks whether `skipMessages > 0`; if so it decompresses the container, skips the leading partial LOBJ, then begins counting. For chunks that start at a container boundary, no special handling needed.

### `populateFrom` deprecation

`populateFrom(const std::vector<TraceMessage>&)` stays in `MessageListWidget` for now (compile guard with a comment). Removed when `buildIndex` path is confirmed working end-to-end.

---

## Phase 1 Design Review — Constitution Check

No constitution gates defined. No violations. Complexity in this plan:

| Area | Complexity | Justification |
|------|------------|---------------|
| Async decode | `QThreadPool` + queued signals | Required to keep UI at 60 fps; blocking `data()` is not acceptable |
| In-memory index | `std::vector<ChunkEntry>` | O(log N) lookup, < 2 MB, zero deps — minimal complexity |
| Split LOBJ in decode | synchronous stitch | Existing `runStitcher` logic ported to synchronous variant — reuses understood logic |
