# Spec: CAN Signal Visualization

Grilled: 2026-06-21. Two phases.

---

## Phase 1 — Point-in-time decode in `MessageDetailsWidget`

### 1.1 `ArxmlParser`: fix signal name to use ISignal SHORT-NAME

**File:** `cpp/src/ArxmlParser.cpp`

`ArSignal::name` currently stores the I-SIGNAL-TO-I-PDU-MAPPING `SHORT-NAME`, not the ISignal name. Change to extract the last path segment from `I-SIGNAL-REF`:

```cpp
// cpp/src/ArxmlParser.cpp — inside the I-SIGNAL-TO-PDU-MAPPINGS loop
std::string ref = m.child("I-SIGNAL-REF").text().get();
auto pos = ref.rfind('/');
sm.name = (pos != std::string::npos) ? ref.substr(pos + 1) : ref;
```

No change to `ArSignal` struct — `name` field semantics only.

---

### 1.2 New: `SignalDecoder` — free-function CAN signal extraction

**New files:** `cpp/include/SignalDecoder.h`, `cpp/src/SignalDecoder.cpp`

```cpp
// SignalDecoder.h
#pragma once
#include <cstdint>
#include <vector>
#include "ArxmlTypes.h"
#include "TraceMessage.h"

namespace fastrace {

struct DecodedSignal {
    std::string name;
    uint64_t rawValue = 0;
    uint32_t bitLength = 0;
};

// Extract raw unsigned value from CAN data bytes.
//
// Intel (isBigEndian=false):
//   startBit = LSB bit position.
//   Bit numbering: byte 0 bits 0..7 (0=LSB), byte 1 bits 8..15, ...
//   Extract bitLength bits starting at startBit, LSB-first.
//
// Motorola (isBigEndian=true):
//   startBit = MSB bit position, same numbering as Intel.
//   Bits packed MSB-first. Walk from MSB downward within each byte,
//   then continue at the MSB of the next byte (byte index + 1, bit 7 within byte).
//   Handles signals that cross byte boundaries.
//
// Returns 0 if startBit/bitLength exceed dataLen.
uint64_t extractSignalRaw(
    const uint8_t* data, uint8_t dataLen,
    uint32_t startBit, uint32_t bitLength, bool isBigEndian);

// Look up ArMessage for msg.arbId in db, decode all its signals.
// Returns empty vector if no matching message or no signal definitions.
std::vector<DecodedSignal> decodeAllSignals(
    const ArDatabase& db, const TraceMessage& msg);

} // namespace fastrace
```

#### Intel extract algorithm

```
rawValue = 0
for i in 0..bitLength-1:
    bitPos = startBit + i
    byteIdx = bitPos / 8
    bitInByte = bitPos % 8
    if byteIdx < dataLen:
        rawValue |= uint64_t((data[byteIdx] >> bitInByte) & 1) << i
```

#### Motorola extract algorithm

```
rawValue = 0
// Track current bit position, starting at MSB (startBit)
byteIdx = startBit / 8
bitInByte = startBit % 8   // 0=LSB, 7=MSB within byte
for i in 0..bitLength-1:
    // i=0 is MSB, placed at bit (bitLength-1-i) of rawValue
    if byteIdx < dataLen:
        rawValue |= uint64_t((data[byteIdx] >> bitInByte) & 1) << (bitLength - 1 - i)
    // Advance: within byte go toward LSB; at byte LSB (bitInByte==0) wrap to MSB of next byte
    if bitInByte == 0:
        byteIdx += 1
        bitInByte = 7
    else:
        bitInByte -= 1
```

---

### 1.3 `Analyzer`: add ArDatabase + `loadDatabase`

**File:** `cpp/include/Analyzer.h` — add to `class Analyzer`:

```cpp
// Database async-load progress (0.0–1.0). Polled by UI every 100ms.
std::atomic<float> dbLoadProgress { 0.0f };
// Set true by UI to cancel in-flight loadDatabase.
std::atomic<bool> dbLoadCancelled { false };

// Load and parse ARXML database from path. Blocking; call via QtConcurrent.
// Updates dbLoadProgress as parse progresses.
void loadDatabase(const std::string& path);

const ArDatabase& arDatabase() const { return m_arDatabase_; }
```

**Private member:**
```cpp
ArDatabase m_arDatabase_;
```

**File:** `cpp/src/Analyzer.cpp` — implement `loadDatabase`:

```cpp
void Analyzer::loadDatabase(const std::string& path)
{
    dbLoadProgress.store(0.0f, std::memory_order_relaxed);
    // ArxmlParser::parseFiles returns ArDatabase; progress reporting is
    // coarse (start=0, done=1) until parser supports incremental progress.
    m_arDatabase_ = ArxmlParser::parseFiles({ path });
    m_arDatabase_.buildIndex();
    dbLoadProgress.store(1.0f, std::memory_order_relaxed);
}
```

Note: `ArxmlParser::parseFiles` signature already takes `vector<string>`. If it needs updating to accept a single path, overload or wrap.

---

### 1.4 `MessageDetailsWidget`: signal decode tab

**File:** `ui/include/MessageDetailsWidget.h` — add:

```cpp
#include <memory>
#include "Analyzer.h"
#include "TraceMessage.h"

// Call once after construction; required for signal decode tab to work.
void attachAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer);
// Re-decode last selected message with updated ArDatabase. No-op if no message seen yet.
void refreshSignalDecode();

private:
    std::shared_ptr<fastrace::Analyzer> m_analyzer;
    fastrace::TraceMessage m_lastMsg;
    bool m_hasMsg = false;
    void populateSignalTab();
```

**File:** `ui/src/MessageDetailsWidget.cpp`

- `updateFromMessage`: cache `m_lastMsg`, set `m_hasMsg = true`, call `populateSignalTab()`.
- `refreshSignalDecode()`: if `m_hasMsg && m_analyzer`, call `populateSignalTab()`.
- `populateSignalTab()`: calls `fastrace::decodeAllSignals(m_analyzer->arDatabase(), m_lastMsg)`, populates `tabSignalDecoding` with a `QTreeWidget` (3 columns: **Signal**, **Raw Value**, **Bits**).

`tabSignalDecoding` already exists in `MessageDetailsWidget.ui`. Add a `QTreeWidget` inside it programmatically in `MessageDetailsWidget.cpp` constructor (or via `.ui` edit).

**Column layout:**

| Signal | Raw Value | Bits |
|--------|-----------|------|
| EngineSpeed | 0x00A4 (164) | [7..0] |

Raw value: display as `0x%X (%llu)`. Bits: display as `[startBit+bitLen-1..startBit]` for Intel; `[startBit..startBit-bitLen+1]` for Motorola.

If `ArDatabase` is empty or no match for arbId: show "No signal definitions loaded" placeholder label.

---

### 1.5 `MainWindow`: wire database loading

**File:** `ui/include/MainWindow.h` — add:

```cpp
QFutureWatcher<void>* m_dbWatcher = nullptr;
QTimer* m_dbPollTimer = nullptr;

private slots:
    void onDatabaseSelectionChanged(const QStringList& paths);
    void onDbLoadFinished();
    void onPollDbProgress();
```

**File:** `ui/src/MainWindow.cpp`

Connect in constructor:
```cpp
connect(m_topBar, &TopBarWidget::databaseSelectionChanged,
        this, &MainWindow::onDatabaseSelectionChanged);
```

`onDatabaseSelectionChanged(paths)`:
- If paths empty: clear `m_analyzer->m_arDatabase_`; `m_messageDetails->refreshSignalDecode()`; return.
- If `m_dbWatcher` running: cancel (set `m_analyzer->dbLoadCancelled = true`, wait — or just block new loads; disable `cmbDatabase` until done).
- Take `paths[0]` (single DB for now; multi-DB is future work).
- Disable `cmbDatabase` in TopBarWidget (add `setDatabaseComboEnabled(bool)` slot or access via method).
- `m_dbPollTimer->start(100)`.
- `m_dbWatcher->setFuture(QtConcurrent::run([analyzer = m_analyzer, path = paths[0].toStdString()] { analyzer->loadDatabase(path); }))`.

`onDbLoadFinished()`:
- `m_dbPollTimer->stop()`.
- Re-enable `cmbDatabase`.
- `m_messageDetails->refreshSignalDecode()`.

`onPollDbProgress()`:
- Read `m_analyzer->dbLoadProgress`.
- Update the narrow progress bar below `cmbDatabase` (see §1.6).

---

### 1.6 Progress bar below `cmbDatabase`

**File:** `ui/src/TopBarWidget.ui` — add a `QProgressBar` named `dbLoadProgress` directly below `cmbDatabase`, height 3px, no text label, hidden by default.

**File:** `ui/include/TopBarWidget.h` — add:
```cpp
void setDatabaseComboEnabled(bool enabled);
void setDbLoadProgress(float fraction); // 0.0–1.0; hides bar at 1.0
```

`setDbLoadProgress`: `ui->dbLoadProgress->setValue(int(fraction * 100))`. Show bar when `fraction < 1.0`, hide when `>= 1.0`.

---

### 1.7 Remove `LeftPanelWidget`

Delete files:
- `ui/include/LeftPanelWidget.h`
- `ui/src/LeftPanelWidget.cpp`
- `ui/src/LeftPanelWidget.ui`

`LeftPanelWidget` is not instantiated in `MainWindow` or `OverviewView` — no other changes needed. Remove from `CMakeLists.txt` source list.

Also remove: `cpp/include/SignalDatabases.h`, `cpp/src/SignalDatabases.cpp` — superseded by `Analyzer::loadDatabase`. Remove from `CMakeLists.txt`. Remove `#include "SignalDatabases.h"` from any remaining headers.

---

## Phase 2 — Time-series plot in `TimelineWidget`

### 2.1 `SignalBin` struct + `Analyzer::buildSignalTimeSeries`

**File:** `cpp/include/Analyzer.h` — add:

```cpp
struct SignalBin {
    uint64_t timestampUs = 0; // centre of bin
    uint64_t minRaw = UINT64_MAX;
    uint64_t maxRaw = 0;
    bool hasData = false;
};

// Async: scan all chunks for messages matching the CAN ID of the signal
// identified by iSignalName in arDatabase(). For each matching message,
// decode the signal and accumulate into numBins bins covering full trace duration.
// Caller owns 'out'; resized to numBins on entry.
// Uses histogramChunksProcessed / histogramCancelled atomics for progress/cancel
// (reuse pattern from buildHistogram).
void buildSignalTimeSeries(
    const std::string& iSignalName,
    int numBins,
    std::vector<SignalBin>& out);
```

Implementation notes:
- Look up signal by iterating `arDatabase().messages` and their `signalDefs` to find matching `name`. Record `arbId` + `ArSignal`.
- Reuse `getChunkIndex()` + `decodeChunk()` loop pattern from `buildHistogram`.
- For each chunk message: if `msg.arbId == arbId`, call `extractSignalRaw`, accumulate into bin at `(msg.timestampUs - traceStart) / binWidth`.
- Bin `timestampUs` = `traceStart + binIdx * binWidth + binWidth/2`.
- Uses `histogramCancelled` to allow UI to abort.
- Increments `histogramChunksProcessed` per chunk processed.

---

### 2.2 `TimelineWidget`: signal lanes + "Add signal" button

**File:** `ui/src/TimelineWidget.ui` — in `headerLayout`, add before `headerSpacer`:
```xml
<widget class="QPushButton" name="btnAddSignal">
  <property name="text"><string>+ Signal</string></property>
</widget>
```

Replace `graphPlaceholder` QLabel with a `QScrollArea` named `signalScrollArea` containing a `QWidget` named `signalLanesWidget`.

**File:** `ui/include/TimelineWidget.h`

```cpp
#include <memory>
#include <vector>
#include <string>
#include <QFutureWatcher>
#include "Analyzer.h"

void attachAnalyzer(std::shared_ptr<fastrace::Analyzer> analyzer);

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onBtnAddSignalClicked();
    void onSignalJobFinished(int laneIndex);

private:
    struct SignalLane {
        std::string iSignalName;
        std::vector<fastrace::SignalBin> bins;
        QFutureWatcher<void>* watcher = nullptr;
        bool loading = false;
    };

    void startSignalJob(int laneIdx);
    void paintLane(QPainter& p, const SignalLane& lane, QRect rect);

    std::shared_ptr<fastrace::Analyzer> m_analyzer;
    std::vector<SignalLane> m_lanes;
    QTimer* m_repaintTimer = nullptr;
```

**Signal search dropdown (on btnAddSignal click):**

Create a `QDialog` (or `QMenu`-style popup) containing:
- `QLineEdit` for search input
- `QListWidget` populated with all signal names from `m_analyzer->arDatabase()`
- As user types, filter list using `QSortFilterProxyModel` or manual filter
- On signal selected: add to `m_lanes`, call `startSignalJob(laneIdx)`, close popup

Signal names come from:
```cpp
for (auto& msg : m_analyzer->arDatabase().messages)
    for (auto& sig : msg.signalDefs)
        // sig.name is ISignal SHORT-NAME
```

**`startSignalJob(laneIdx)`:**

Pattern mirrors `restartHistogramJob` in `TimelineOverviewWidget`:
```cpp
m_analyzer->histogramCancelled.store(false, std::memory_order_relaxed);
m_analyzer->histogramChunksProcessed.store(0, std::memory_order_relaxed);
m_lanes[laneIdx].loading = true;
m_lanes[laneIdx].bins.resize(numBins);
auto future = QtConcurrent::run([analyzer = m_analyzer, name = lane.iSignalName, numBins, &out = lane.bins] {
    analyzer->buildSignalTimeSeries(name, numBins, out);
});
m_lanes[laneIdx].watcher->setFuture(future);
m_repaintTimer->start(100);
```

`numBins` = `signalLanesWidget->width() / 4` (4px per bin, full trace).

---

### 2.3 `TimelineWidget::paintEvent`

Paint `signalLanesWidget` (custom widget or override `paintEvent` on it). For each `SignalLane`:
- Lane height: 40px. Label area: 120px left. Plot area: remainder.
- Draw lane name in label area (left-aligned, vertically centred).
- Draw horizontal separator line.
- If loading: draw thin blue progress bar at top of lane (read `histogramChunksProcessed / chunkIndex.size()`).
- Once loaded: for each bin with `hasData == true`, map `maxRaw`/`minRaw` to y within lane (0 = bottom = value 0, top = value `(1 << bitLength) - 1`). Draw dots at `(x, yMax)` and `(x, yMin)`. If `minRaw == maxRaw`, one dot.
- Dot color: cycle through palette per lane index (blue, green, orange, red, purple, …).

Signal lanes widget lives inside `QScrollArea` — vertical scroll for many lanes.

---

## Ordering / dependencies

```
1. ArxmlParser name fix           (standalone, no deps)
2. SignalDecoder.h/.cpp           (depends on ArxmlTypes, TraceMessage)
3. Analyzer: loadDatabase         (depends on ArxmlParser, ArDatabase)
4. SignalDatabases removal        (cleanup, parallel with above)
5. LeftPanelWidget removal        (cleanup, parallel with above)
6. MessageDetailsWidget decode tab (depends on SignalDecoder + Analyzer accessor)
7. MainWindow: DB wiring + progress (depends on Analyzer::loadDatabase, TopBarWidget progress bar)
8. TopBarWidget: progress bar     (small UI addition)
9. Analyzer: buildSignalTimeSeries (depends on SignalDecoder + ChunkIndex)
10. TimelineWidget: signal lanes  (depends on buildSignalTimeSeries)
```

Steps 1–5 can be done in parallel. Step 6 needs 1–3. Step 7 needs 3, 6, 8. Steps 9–10 need 2, 3.
