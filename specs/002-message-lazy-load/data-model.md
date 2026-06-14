# Data Model: Lazy Loading Messages (002-message-lazy-load)

**Date**: 2026-06-13  
**Branch**: `002-message-lazy-load`

---

## Entities

### `ChunkEntry` (new, in `Analyzer`)

Represents one entry in the sparse chunk index. One entry per 10,000 messages.

| Field            | Type       | Description                                          |
|------------------|------------|------------------------------------------------------|
| `fileOffset`     | `size_t`   | Byte offset of the container in the mmap'd file      |
| `containerIndex` | `uint64_t` | Sequential container number (0-based)                |
| `skipMessages`   | `uint32_t` | Messages before chunk start within that container    |

**Cardinality**: ~100,000 entries for 1 billion messages (≈ 2 MB in memory)

---

### Modified `Analyzer`

New fields added alongside existing ones:

| Field              | Type                        | Description                             |
|--------------------|-----------------------------|-----------------------------------------|
| `chunkIndex_`      | `std::vector<ChunkEntry>`   | Sparse lookup table, built by `buildIndex` |
| `totalMessages_`   | `size_t`                    | Total message count determined during scan |
| `mf_`              | `MappedFile`                | Persistent mmap kept open for `decodeChunk` calls |

**Invariants**:
- `chunkIndex_` is read-only after `buildIndex` completes
- `mf_` lifetime matches `Analyzer` instance lifetime after `buildIndex`
- `chunkIndex_[i]` refers to the start of chunk `i*CHUNK_SIZE`

---

### `MessageTableModel` (new, in UI)

`QAbstractTableModel` subclass. Bridges the Qt view to `Analyzer`.

| Field        | Type                                     | Description                             |
|--------------|------------------------------------------|-----------------------------------------|
| `analyzer_`  | `std::shared_ptr<fastrace::Analyzer>`    | Shared with `MainWindow`                |
| `cache_`     | `std::map<size_t, std::vector<TraceMessage>>` | Decoded chunks keyed by chunk index |
| `pending_`   | `std::set<size_t>`                       | Chunk indices currently being decoded   |
| `rowCount_`  | `size_t`                                 | Total message count from `Analyzer`     |

**Cache policy**: max 3 chunks (30,000 messages). On overflow evict chunk furthest from last-requested chunk index.

---

### State Transitions

```
Analyzer state:
  [Created] → buildIndex() → [Indexed, mf_ open]
                           → decodeChunk(i) [any time after Indexed]

MessageTableModel state:
  [Empty] → setAnalyzer() → [Ready]
  data(row) hit cache → return TraceMessage fields
  data(row) miss cache → return placeholder, dispatch async decode
  async decode done → insert to cache_, emit dataChanged
```

---

## Column Mapping

`MessageTableModel::data()` maps `TraceMessage` fields to the 8 existing table columns:

| Col | Header     | Source field(s)                                                                 |
|-----|------------|---------------------------------------------------------------------------------|
| 0   | Time       | `timestampUs` → `HH:MM:SS.µµµµµµ` string                                       |
| 1   | Bus        | `objectType` → "CAN" / "CAN FD" / "Ethernet" / "Other"                        |
| 2   | ID / Src   | `arbId` (CAN) or `"CH%d"` (Ethernet) from `channel`                            |
| 3   | Name       | `""` (signal DB lookup deferred to future feature)                              |
| 4   | DLC        | `dlc`                                                                           |
| 5   | Data       | `data[0..dataLen-1]` → hex string                                               |
| 6   | Length     | `dataLen`                                                                       |
| 7   | ECU        | `"CH%d"` from `channel`                                                         |
