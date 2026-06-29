# Phase 0: Outline & Research

## Technical Questions & Clarifications

### 1. SOME/IP Wire Format
- **Decision:** Parse the full 16-byte header.
- **Rationale:** The exact header layout is: Service ID (16b), Method ID (16b), Length (32b), Client ID (16b), Session ID (16b), Protocol Version (8b), Interface Version (8b), Message Type (8b), Return Code (8b). All these fields are necessary for stateful detectors (tracking session resets, matching request/response).

### 2. SOME/IP-SD Format
- **Decision:** Parse SD Header + Entries + Options.
- **Rationale:** The SOME/IP-SD payload contains: Flags (8b), Reserved (24b), Entries Length (32b), Entries Array, Options Length (32b), Options Array. Understanding entry types (FindService=0x00, OfferService=0x01, Subscribe=0x06, StopSubscribe=0x07) is critical for lifecycle anomaly detection.

### 3. DoIP (ISO 13400-2) Format
- **Decision:** Parse the 8-byte base header and the 4-byte diagnostic sub-header.
- **Rationale:** Base header is Protocol Version (8b), Inverse Version (8b), Payload Type (16b), Payload Length (32b). Payload type 0x8001 (Diagnostic Message) is followed by Source Address (16b) and Target Address (16b). Both are needed to detect unknown target addresses and routing anomalies.

### 4. SOME/IP-SD Port
- **Decision:** Default to 30490.
- **Rationale:** AUTOSAR specifies 30490 (UDP) as the standard port for SOME/IP Service Discovery. The SomeIpSdDetector will check for messages arriving on other ports.

### 5. DoIP Alive Check Timeout
- **Decision:** Configurable or hardcoded to standard 500ms window.
- **Rationale:** ISO 13400 specifies alive check responses must arrive within a defined timeout (typically 500ms). The DoIP detector will track the timestamps of requests and flag if the response exceeds this window.

### 6. Existing Codebase Patterns
- **Decision:** Detection will follow `Analyzer::buildHistogram()` pattern.
- **Rationale:** `Analyzer::buildHistogram()` loops through `chunkIndex_`, calls `decodeChunk(i)`, checks `std::atomic<bool> cancelled`, and increments `std::atomic<size_t> histogramChunksProcessed`. The `DetectionEngine` will do exactly this.
- **Decision:** Protocol parsing will integrate with `Analyzer::processInnerObjects()`.
- **Rationale:** `processInnerObjects` currently extracts UDP payload and basic PDUs (`extractUdpInfo`). A new `ProtocolParser` will be injected or called from here (or `DetectionEngine`) to emit `ProtocolMessage` variants instead of simple byte buffers.

## Design Confirmations
All "NEEDS CLARIFICATION" points from the plan are now resolved.
