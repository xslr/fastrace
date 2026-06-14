# Internal Contracts: Lazy Loading Messages (002-message-lazy-load)

This feature is a desktop application (no HTTP API). Contracts here are C++ interfaces and Qt signal/slot boundaries.

---

## Contract 1: `Analyzer` Indexing & Chunk Decode API

**File**: `cpp/include/Analyzer.h`

```cpp
namespace fastrace {

// Sparse lookup entry: one per CHUNK_SIZE messages
struct ChunkEntry {
    size_t   fileOffset;      // byte offset of container in mmap
    uint64_t containerIndex;  // 0-based container sequence number
    uint32_t skipMessages;    // messages before chunk start inside the container
};

class Analyzer {
public:
    static constexpr size_t CHUNK_SIZE = 10'000;

    // --- New API (lazy-load path) ---

    /// Perform a fast single-threaded scan: count messages, build chunkIndex_.
    /// Updates bytesRead / totalBytes for UI progress.
    /// Returns total message count (also stored in totalMessages_).
    /// After this call, mf_ stays open for decodeChunk().
    size_t buildIndex(const std::string& filename);

    /// Decode messages [chunkIndex * CHUNK_SIZE, (chunkIndex+1) * CHUNK_SIZE).
    /// Thread-safe: may be called concurrently from different threads for different chunks.
    /// Returns fewer than CHUNK_SIZE items only for the last chunk.
    std::vector<TraceMessage> decodeChunk(size_t chunkIndex) const;

    /// Total message count; valid after buildIndex() completes.
    size_t totalMessages() const noexcept { return totalMessages_; }

    // --- Existing API (kept for CLI/benchmark path) ---
    void processFile(const std::string& filename);

    // ... existing fields ...
private:
    std::vector<ChunkEntry> chunkIndex_;
    size_t                  totalMessages_ = 0;
    MappedFile              mf_;   // kept open after buildIndex
};

} // namespace fastrace
```

**Pre-conditions**:
- `buildIndex` must complete before any `decodeChunk` call
- `chunkIndex` must be < `(totalMessages_ + CHUNK_SIZE - 1) / CHUNK_SIZE`

**Post-conditions**:
- `decodeChunk` returns messages in timestamp-ascending order within the chunk
- Concurrent calls for different chunk indices are safe

---

## Contract 2: `MessageTableModel` Qt Model API

**File**: `ui/src/MessageTableModel.h`

```cpp
#include <QAbstractTableModel>
#include <memory>
#include "Analyzer.h"

class MessageTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit MessageTableModel(QObject* parent = nullptr);

    /// Attach analyzer (post-buildIndex). Resets model and emits layoutChanged.
    void setAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer);

    /// Detach analyzer and clear cache. Emits layoutChanged.
    void clear();

    // QAbstractTableModel overrides
    int      rowCount(const QModelIndex& parent = {}) const override;
    int      columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

signals:
    void chunkDecodeRequested(size_t chunkIndex);  // emitted to trigger async decode

private slots:
    void onChunkDecoded(size_t chunkIndex, QVector<fastrace::TraceMessage> messages);
};
```

**Column count**: always 8 (Time, Bus, ID/Src, Name, DLC, Data, Length, ECU).

**Threading invariant**: `data()` and `rowCount()` called only on the GUI thread. `onChunkDecoded` slot connected via `Qt::QueuedConnection` — always runs on GUI thread.

---

## Contract 3: `MessageListWidget` Integration

**File**: `ui/src/MessageListWidget.h`

```cpp
class MessageListWidget : public QWidget {
    Q_OBJECT
public:
    explicit MessageListWidget(QWidget* parent = nullptr);
    ~MessageListWidget() override;

public slots:
    /// Called after Analyzer::buildIndex() completes.
    /// Replaces populateFrom(). Attaches model to view.
    void attachAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer);

    /// Detach and reset to empty state.
    void clearTable();

    // DEPRECATED — kept for transition period, removed when buildIndex path is complete:
    void populateFrom(const std::vector<fastrace::TraceMessage>& messages);
};
```

---

## Contract 4: `MainWindow` Loading Flow Change

Current flow:
```
onTraceFileChanged → startLoad → QtConcurrent(Analyzer::processFile) 
                   → onLoadFinished → MessageListWidget::populateFrom
```

New flow:
```
onTraceFileChanged → startLoad → QtConcurrent(Analyzer::buildIndex)
                   → onLoadFinished → MessageListWidget::attachAnalyzer
                   → [scroll] → MessageTableModel::data miss
                              → async QtConcurrent(Analyzer::decodeChunk)
                              → onChunkDecoded → dataChanged signal → repaint
```
