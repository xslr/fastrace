# CLAUDE.md

This file provides guidance to Claude when working with code in this repository.

## Project Overview
* **What it is:** Library and UI to analyze automotive network traces. Currently supports CAN and Ethernet messages.
* **Target Platforms:** Linux, Mac, and Windows.
* **Core Goals:**
  * Fast processing.
  * Non blocking UI.
  * Iterative trace analysis.

## Development Commands
Run these commands to start, test, and check your work:
* **Build:** `cmake --build build` (from project root)
* **Run CPP CLI:** `./build/cpp/cpp_parser`
* **Run UI:** `./build/ui/fastrace_ui`
* **Format code:** `clang-format -i [file]`

## Coding Rules & Preferences
* **Code Style:** C++23 standard. Use Qt6 for UI components (`fastrace_ui`) and standard C++ for core logic (`fastrace_analyzer`). Avoid blocking the main GUI thread; use `QtConcurrent` to execute long running operations without blocking UI.
* **Naming:** 
  * Use `PascalCase` for Classes and Structs (e.g., `MainWindow`, `RecentFiles`).
  * Use `camelCase` for variables and methods (e.g., `openDatabase`, `populateDbCombo`).
  * Use `m_` prefix for private member variables (e.g., `m_currentDbPath`).
  * Use `k` prefix for constants (e.g., `kSpeedSamples`).
* **Includes:** Group headers logically (local headers, Qt headers, standard library).
* **Memory Management:** Use smart pointers (`std::make_shared`, `std::unique_ptr`) and RAII.
* **Organization:** Keep code modular with strong separation of responsibility. Ensure that logic is well split into separate functions and classes when appropriate to limit complexity.

## Do
* **Always:** Run `clang-format` on modified C++ files to ensure code follows syntax guidelines.
* **Always:** Handle missing file cases gracefully without crashing (e.g., silently clear UI or show friendly error).
* **Always:** Update `CMakeLists.txt` when adding or removing source files.

## Do Not Do
* **Never:** Introduce synchronous blocking calls in UI slots.
* **Never:** Bypass `RecentFiles` or custom project persistence for saving UI state.
* **Avoid:** Using emoticons in source code and plaintext documentation, except where already established (like UI action icons).

## Important files and directories
* `/cpp`: Core backend library (`fastrace_analyzer`) handling trace parsing and signal decoding logic. Independent of Qt UI.
  * `/cpp/include/`: Public headers
    * `Analyzer.h` – Top-level analyzer interface; entry point for trace loading and signal decoding.
    * `ArxmlParser.h` / `ArxmlTypes.h` – ARXML database parsing and type definitions.
    * `BlfTypes.h` – BLF trace format type definitions.
    * `Cursor.h` – Cursor abstraction for iterating over trace messages.
    * `MappedFile.h` – Memory-mapped file I/O helper.
    * `NetTypes.h` – Ethernet / network message type definitions.
    * `Detection.h` – Anomaly detection structs (Severity, Detection, ByteRange).
    * `Detector.h` – Base class for protocol-specific anomaly detectors.
    * `DetectionEngine.h` – Orchestrator for running detectors in parallel.
    * `ProtocolMessage.h` – Unified representation of nested network messages for detectors.
    * `ProtocolParser.h` – TCP/IP reassembly and header extraction.
    * `RecentFiles.h` – Persistent recent-file list (shared with UI).
    * `SignalDecoder.h` – Signal decoding from raw CAN/Ethernet frames.
    * `TraceMessage.h` – Unified trace message type used throughout the backend.
    * `WorkQueue.h` – Thread-safe work queue for background processing.
  * `/cpp/src/`: Backend implementation files
    * `Analyzer.cpp`, `ArxmlParser.cpp`, `MappedFile.cpp`, `RecentFiles.cpp`, `SignalDecoder.cpp`, `WorkQueue.cpp`, `DetectionEngine.cpp`, `ProtocolParser.cpp`
    * `main.cpp` – CLI entry point (`cpp_parser` binary).
  * `/cpp/include/detectors/` & `/cpp/detectors/`: Protocol-specific anomaly detector implementations.
    * `PduDetector.h` / `PduDetector.cpp`, `SomeIpSdDetector.h` / `SomeIpSdDetector.cpp`, `DoipDetector.h` / `DoipDetector.cpp`
* `/cpp/doc`: Documentation specific to backend architecture and algorithms.
* `/ui`: Qt6 Widgets-based frontend application (`fastrace_ui`). Connects to the backend `Analyzer`.
  * `/ui/include/`: UI widget headers
    * `MainWindow.h` – Application main window.
    * `TopBarWidget.h` – Toolbar / top-bar controls.
    * `OverviewView.h` – High-level trace overview view.
    * `MessageListWidget.h` / `MessageTableModel.h` – Message list table and its data model.
    * `MessageDetailsWidget.h` – Detailed view of a single message.
    * `TimelineWidget.h` / `TimelineOverviewWidget.h` – Timeline and mini-map widgets.
    * `DetectionsWidget.h` – Widget for displaying detected anomalies.
    * `DetectionTableModel.h` / `DetectionFilterProxyModel.h` – Models for detection table.
    * `NotebookView.h` / `NotebookBlockWidget.h` – Notebook / script analysis view.
    * `ScriptEditorWidget.h` – Embedded script editor.
    * `AnalyzerPreviewWidget.h` – Preview widget for analyzer output.
  * `/ui/src/`: UI implementation and Qt Designer form files (`.cpp` + `.ui` pairs)
    * `MainWindow.cpp`
    * `TopBarWidget.cpp` / `TopBarWidget.ui`
    * `OverviewView.cpp`
    * `MessageListWidget.cpp` / `MessageListWidget.ui`
    * `MessageDetailsWidget.cpp` / `MessageDetailsWidget.ui`
    * `TimelineWidget.cpp` / `TimelineWidget.ui`
    * `TimelineOverviewWidget.cpp`
    * `DetectionsWidget.cpp` / `DetectionsWidget.ui`
    * `NotebookView.cpp` / `NotebookBlockWidget.cpp`
    * `ScriptEditorWidget.cpp` / `ScriptEditorWidget.ui`
    * `AnalyzerPreviewWidget.cpp` / `AnalyzerPreviewWidget.ui`
    * `main.cpp` – UI application entry point.
* `/ui/doc`: Documentation specific to UI layouts, widgets, and view components.
* `/testdata`: Directory for manual testing files.
  * `/testdata/trace`: Example trace files (`.blf`, `.pcapng`).
  * `/testdata/db`: Signal database files (`.arxml`, `.dbc`).
* `/specs`: Project feature specifications, tasks, and implementation plans using Speckit.
