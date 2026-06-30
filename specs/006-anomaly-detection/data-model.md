# Data Model & Interfaces

## 1. Entities

### Detection
A single anomaly finding emitted by a detector.
- `timestampUs` (uint64_t): Time the anomaly occurred.
- `detectorName` (string): Name of the source detector (e.g., "DoipDetector").
- `severity` (enum Severity { Info, Warning, Error }): Severity of the anomaly.
- `message` (string): Human-readable description.
- `messageIndex` (size_t): Index of the TraceMessage in the Analyzer (for click-to-navigate).
- `relatedBytes` (struct { offset, length }): Affected payload byte range.
- `context` (map<string, string>): Structured metadata (e.g., {"serviceId": "0x1234"}).

### ProtocolMessage (Tagged Union)
Reassembled protocol-layer message.
- `timestampUs` (uint64_t): Time of the first fragment.
- `sourceIndices` (vector<size_t>): TraceMessage indices this was assembled from.
- `type` (enum ProtocolType { SomeIp, SomeIpSd, DoIp, Pdu }): Variant tag.
- `union data`:
  - `SomeIpHeader someIp`
  - `SomeIpSdMessage someIpSd`
  - `DoipHeader doip`
  - `PduInfo pdu`

### Detector (Abstract Base)
Base class for protocol-specific anomaly detectors.
- `name()` -> string
- `isStateful()` -> bool
- `inspect(const ProtocolMessage& msg)` -> void
- `finalize()` -> void
- `reset()` -> void
- `clone()` -> unique_ptr<Detector>
- `merge(const Detector& other)` -> void

### DetectionEngine
Orchestrator for the detection process.
- `run(Analyzer* analyzer, std::atomic<bool>& cancelFlag, std::atomic<size_t>& progressCounter)` -> void
- `getResults()` -> const vector<Detection>&
- `clearResults()` -> void

### ProtocolParser
Stateful parser for Ethernet payloads.
- `parse(const TraceMessage& tm)` -> vector<ProtocolMessage>

## 2. Validation Rules & State Transitions

- **TCP Reassembly**: `ProtocolParser` maintains a buffer per `(srcIp:srcPort, dstIp:dstPort)`. Data is concatenated until `payload length` is satisfied. Out-of-order TCP segments trigger an `Info` detection and are currently dropped/ignored for simplicity.
- **Stateful Detectors (Merge)**:
  - `SomeIpSdDetector`: Merges session ID max/min across chunk boundaries to detect cross-chunk resets.
  - `DoipDetector`: Merges connection states (TCP stream state machines).

## 3. UI Models

- **DetectionTableModel**: Qt model wrapping `std::vector<Detection>`.
- **DetectionFilterProxyModel**: `QSortFilterProxyModel` subclass filtering by `severity` and `detectorName`.
