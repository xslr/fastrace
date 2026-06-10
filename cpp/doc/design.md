# Fastrace C++ Parser Design

## Requirements

The core goal of `fastrace_cpp` is to parse Vector BLF (Binary Log File) files at extremely high throughput. The parser must meet the following requirements:
- **Parallel Decompression**: Decompress `CONTAINER` (type 10) payloads concurrently using multiple worker threads.
- **High Performance**: Minimize memory allocations and data copying. Operate directly on decompressed memory buffers that fit well within the CPU's L3 cache.
- **Accurate Log Object Decoding**: Correctly identify and decode embedded inner log objects (LOBJs) such as CAN messages (`CAN_MESSAGE`, `CAN_MESSAGE2`) and Ethernet frames (`ETHERNET_FRAME`).
- **Cross-Container Split Assembly**: Handle cases where a single LOBJ is split across two contiguous compressed container boundaries (e.g. the header and part of the payload are at the tail of container $N$, and the rest of the payload is at the head of container $N+1$).
- **Lock-Free Hot Path**: Cross-container assembly must avoid serializing the decompression pipeline or using expensive inter-thread synchronization during normal object processing.

## Architecture & Design

### Pipeline Overview

The system uses a single-producer, multi-consumer pipeline designed to overlap file I/O with decompression compute:

1. **Producer Thread** (Main Thread):
   - Memory-maps the file (`MappedFile`) and uses OS-level pre-fetching (`MADV_POPULATE_READ` / `PrefetchVirtualMemory`).
   - Scans the file sequentially to find `CONTAINER` LOBJs.
   - Assigns a monotonically increasing `containerIndex` to each container.
   - Pushes `{compData, compSize, containerIndex}` structs into a bounded, thread-safe `WorkQueue`.

2. **Consumer Threads**:
   - Pop compressed containers from the `WorkQueue`.
   - Decompress the payload into a persistent, per-thread scratch buffer (minimizing allocations).
   - Parse all fully-contained inner LOBJs directly from the scratch buffer.
   - Accumulate statistics locally and merge them once at thread exit to avoid lock contention.

### Cross-Container Split Handling: The Stitcher Thread

To handle split LOBJs without stalling the parallel consumer threads, the parser uses an asynchronous **Stitcher Thread** approach.

When a consumer processes a decompressed container (with `containerIndex = i`), it performs the following checks:
- **Head Fragment Detection**: If the beginning of the decompressed buffer does not start with an `"LOBJ"` signature, it means the buffer begins with the remainder of an object from the previous container. The consumer extracts these initial bytes and pushes them as a `HeadFragment{i, bytes}` to a lock-free or low-contention `FragmentQueue`.
- **Tail Fragment Detection**: If the consumer reaches the end of the buffer and detects an incomplete object (i.e. `objectEnd > dataLen`), it extracts those trailing bytes and pushes them as a `TailFragment{i, bytes}` to the `FragmentQueue`.

**Stitcher Thread Execution**:
- A dedicated background Stitcher Thread runs concurrently with the Consumers.
- It consumes `HeadFragment` and `TailFragment` messages from the `FragmentQueue`.
- It maintains an internal reassembly state. When it receives both a `TailFragment` for container `i` and a `HeadFragment` for container `i+1`, it concatenates the two byte arrays.
- The Stitcher Thread then runs the standard `processInnerObjects` logic on the reassembled, contiguous buffer to decode the stitched log object.
- It maintains its own local counters and merges them into the global statistics upon completion.

### Advantages of this Design

- **Zero Wait**: Consumer threads never have to wait for each other. Container $i+1$ can be fully processed before container $i$ finishes decompression.
- **Isolated Complexity**: The complex logic of matching boundaries and reassembling bytes is isolated to a single Stitcher Thread, keeping the Consumer hot-path extremely simple and fast.
- **Low Overhead**: Splits occur relatively rarely (typically a few dozen times per GB). The overhead of copying fragment bytes and pushing to the `FragmentQueue` is negligible compared to the total throughput.

---

# Fastrace UI Architecture

## Goals

Provide automotive engineers with a browser-based tool for analyzing vehicle network traces (BLF files). Core requirements:

- Signal-level analysis, discrepancy detection, and custom user-defined analyzers
- Analysis executes server-side; trace files never leave the host machine
- Custom analyzers written in Python, Lua, or JavaScript
- UI host is decoupled from the analysis library so a future Rust reimplementation can reuse the same UI without changes

## Component Overview

```
┌──────────────────────────────┐    C API    ┌─────────────────────────────────┐
│  Analysis Lib (C++ / Rust)   │ ◄─────────► │  UI Host (C++)                  │
│                              │             │                                 │
│  - BLF parsing               │             │  - Embedded interpreters        │
│  - Signal extraction         │             │    · Python  (CPython C API)    │
│  - Pure C API surface        │             │    · Lua     (sol2)             │
│  - No interpreter deps       │             │    · JS      (QuickJS)          │
└──────────────────────────────┘             │  - ViewModel (server-owned)     │
                                             │  - WebSocket server (cpp-httplib)│
                                             │  - SPA static file serving      │
                                             └────────────────┬────────────────┘
                                                              │ WebSocket
                                                      ┌───────▼───────┐
                                                      │  Browser SPA  │
                                                      │  (React)      │
                                                      └───────────────┘
```

## Design Decisions

### Analysis lib exposes a pure C API

The C API is the stable contract between the analysis backend and the UI host. Keeping it pure C (no C++ types in the surface) ensures:

- The same UI host binary works against the C++ and future Rust implementations without recompilation
- The analysis lib has no dependency on any scripting runtime

Bulk signal access is exposed via buffer pointers into the lib's own memory (no copy):

```c
trace_handle_t  trace_open(const char* path);
void            trace_close(trace_handle_t h);
int             trace_signal_count(trace_handle_t h);
signal_meta_t   trace_signal_meta(trace_handle_t h, int idx);
size_t          trace_signal_samples(trace_handle_t h, int signal_id,
                                     uint64_t t_start, uint64_t t_end,
                                     const sample_t** out_ptr);
```

`out_ptr` points directly into the lib's internal buffer — no allocation or copy on the call path.

### Embedded interpreters live in the UI host

Analyzers need access to signal data but not to raw trace bytes. Since the UI host and analysis lib run in the same process, crossing the C API boundary is a function call, not IPC — there is no data transfer cost. Keeping interpreters out of the analysis lib means:

- The lib remains lightweight and embeddable in other contexts
- The Rust reimplementation only needs to satisfy the C API; it does not embed any scripting runtime

### Server-side MVVM

The UI host owns the ViewModel — a JSON structure representing the current view state (loaded file, visible signals, analyzer results, active selections). The browser SPA is a pure view:

- **Commands flow in** over WebSocket (e.g. `open_file`, `run_analyzer`, `set_time_range`)
- **ViewModel diffs flow out** over WebSocket whenever state changes
- The browser never holds analysis data; it only holds what the server sends it to display

This keeps the SPA thin and stateless, and means the UI works correctly even if the browser is refreshed mid-session.

### Scripting runtimes

| Language | Runtime  | Rationale                                      |
|----------|----------|------------------------------------------------|
| Python   | CPython (embedded C API) | Familiar to engineers; rich numerical libs available |
| Lua      | sol2 (header-only C++ wrapper over Lua) | Low overhead for hot inner loops |
| JS       | QuickJS  | Small (~200 KB), full ES2023, no V8 build complexity |

All analyzers run in the UI host process under the same C API surface. An analyzer receives a handle to the open trace and calls C API functions to query signal data; it returns a result value that the UI host serializes into the ViewModel.

### UI host is built separately from the analysis lib

The UI host links against the analysis lib as a shared or static library. It is a separate CMake target (and eventually a separate repository) so that swapping the analysis lib (C++ → Rust) requires only relinking, not modifying UI host source.
