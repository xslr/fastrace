# Research: Lazy Loading Messages (002-message-lazy-load)

**Phase**: 0 — Unknowns resolved  
**Date**: 2026-06-13

---

## 1. BLF Container Structure & Index Feasibility

**Decision**: The chunk lookup table stores `(containerFileOffset, containerIndex, intraContainerMessageSkip)` — one entry per 10,000th message.

**Rationale**: The producer in `runProducer` already walks containers sequentially and has access to `cursor.tell()` (byte offset). Each container pushes compressed data to the work queue tagged with `containerIndex`. A lightweight scanning pass can count messages per container without decoding payload bytes.

**Implementation note**: A fast-scan pass must use a single thread (no pipeline) to have a deterministic container→message ordering without synchronisation. It walks the mmap like the existing `runProducer` but only decompresses enough to count LOBJ signatures within each container — far cheaper than full `processInnerObjects`.

**Alternatives considered**:
- DuckDB: heavyweight external dep, zero benefit for this simple append-only table.
- SQLite (already in build): disk-backed, overkill for an in-memory sorted vector.
- Full decode during scan: works, 10–20× slower than counting.

---

## 2. Lookup Table Format

**Decision**: In-memory `std::vector<ChunkEntry>` in `Analyzer`.

```cpp
struct ChunkEntry {
    size_t   fileOffset;        // byte offset of the container in the mmap
    uint64_t containerIndex;    // 0-based sequential container number
    uint32_t skipMessages;      // number of messages before chunk start inside that container
};
```

**Rationale**: For 1 billion messages with chunk size 10,000, table has 100,000 entries × ~20 bytes = ~2 MB. Fits in L3 cache. Binary search O(log N) for random access. No external deps needed.

**Alternatives considered**: hash map (O(1) lookup but non-ordered, no binary search on partial chunk offset).

---

## 3. `Analyzer` API for On-Demand Chunk Decoding

**Decision**: Add `decodeChunk(size_t chunkIndex) -> std::vector<TraceMessage>` and `buildIndex(const std::string& filename) -> size_t` (returns total message count).

**Rationale**: `buildIndex` replaces the old eager `processFile` for the UI path. It populates `chunkIndex_` and sets `totalMessages_`. `decodeChunk` seeks to `chunkIndex_[i].fileOffset`, decompresses that single container (plus potentially the next one for stitching), skips `skipMessages` messages, and collects exactly 10,000 (or fewer for the last chunk).

**Thread safety**: `buildIndex` runs once synchronously (or async). `decodeChunk` is called from a background worker thread; the mmap is read-only so no mutex on the mmap itself. Multiple concurrent `decodeChunk` calls on different chunks are safe. The chunk index vector is read-only after `buildIndex` completes.

---

## 4. Qt Model Threading Contract

**Decision**: `MessageTableModel` owns a `QThreadPool` (or `QtConcurrent::run`) for chunk decode calls. It uses `QMetaObject::invokeMethod` or `emit` over a queued connection to update cache and call `dataChanged` on the UI thread.

**Rationale**: Qt model `data()` is called on the GUI thread. Blocking there causes jank. Async dispatch + queued signal satisfies the 16ms frame budget. QThreadPool is already available in Qt; no extra threading library needed.

**Cache eviction**: Simple `std::map<size_t, std::vector<TraceMessage>> cache_` keyed by chunkIndex. When cache exceeds 3 chunks (30,000 messages), evict the chunk furthest from current viewport.

**In-flight requests**: A `std::set<size_t> pending_` tracks chunks being decoded to avoid double dispatch. Cleared when result arrives.

---

## 5. UI/UX: Placeholder Display

**Decision**: Return `QVariant("…")` for un-cached rows. After `dataChanged` fires, Qt repaints only the affected rows.

**Rationale**: Minimal effort, correct Qt model pattern. No custom delegate required.

---

## 6. `MessageListWidget.ui` Change

**Decision**: Replace `QTableWidget` (`msgTable`) with `QTableView` in the `.ui` file. Remove hardcoded column definitions (model provides headers). Keep filter bar widgets (`cmbChannel`, `cmbView`, `btnColumns`) as-is; `cmbChannel` will be disabled/locked to "All" for v1.

**Rationale**: `QTableView` + `QAbstractTableModel` is the standard Qt pattern for large datasets. The existing `.ui` file structure only needs a widget class swap.

---

## 7. Split-Object Handling in Chunk Decode

**Decision**: When decoding a chunk that contains a split LOBJ at the container boundary, `decodeChunk` must stitch the tail fragment from container N with the head fragment of container N+1 — same logic as `runStitcher`, but synchronous and single-threaded since we're decoding exactly one chunk.

**Rationale**: Split objects span two consecutive containers. `runStitcher` already implements this. `decodeChunk` will replicate a synchronous variant (no queue needed since we own both containers).

---

## 8. `buildIndex` Fast-Scan Strategy

**Decision**: Reuse the existing producer-consumer pipeline (same as `processFile`) but replace full `processInnerObjects` with a **lightweight counting-only pass** inside each consumer worker.

**Why not single-threaded**: Decompression is the bottleneck, not I/O. A 10 GB compressed file can be 40–50 GB decompressed. Single-thread libdeflate at ~2 GB/s = 20–25 s, far over SC-002's 5 s budget. The existing pipeline parallelises decompression across all cores.

**Counting pass**: Each consumer decompresses its container payload, then walks LOBJ signatures and reads only `BlfObjectHeaderBase` (16 bytes) to check `objectType`. Message objects (CAN_MESSAGE, CAN_MESSAGE2, CAN_FD_MESSAGE, CAN_FD_MESSAGE_64, ETHERNET_FRAME, ETHERNET_FRAME_EX) are counted; payload bytes are skipped entirely. This is O(decompressed size / avg object size) with no allocation.

**Ordering problem**: Concurrent consumers process containers out of order. Each consumer reports `{containerIndex, countInContainer}` into a shared structure. After all consumers finish, the main thread:
1. Sorts by `containerIndex`.
2. Runs a prefix-sum to compute cumulative message numbers.
3. Records a `ChunkEntry` whenever the cumulative count crosses a multiple of `CHUNK_SIZE`.

The `skipMessages` field in `ChunkEntry` captures how many messages from the chunk-boundary container's start must be skipped to reach message index `chunkIndex * CHUNK_SIZE`.

**Progress reporting**: `buildIndex` updates `bytesRead` atomically (reuses existing UI progress bar mechanism).
