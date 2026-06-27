cmake_minimum_required(VERSION 3.20)
project(fastrace_ui LANGUAGES CXX)

# Support Qt6
find_package(QT NAMES Qt6 COMPONENTS Widgets Concurrent REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets Concurrent REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Include directories
include_directories(include)

add_executable(fastrace_ui
    src/main.cpp

    # ── Core window ────────────────────────────────────────────────────────────
    src/MainWindow.cpp
    include/MainWindow.h

    # ── Top bar (with Overview / Notebook toggle) ──────────────────────────────
    src/TopBarWidget.cpp
    include/TopBarWidget.h
    src/TopBarWidget.ui



    # ── Shared left panel ─────────────────────────────────────────────────────
    # (LeftPanelWidget removed from build)


    # ── Shared timeline ───────────────────────────────────────────────────────
    src/TimelineWidget.cpp
    include/TimelineWidget.h

    # ── Shared message list ───────────────────────────────────────────────────
    src/MessageListWidget.cpp
    include/MessageListWidget.h
    src/MessageTableModel.cpp
    include/MessageTableModel.h

    # ── Overview-exclusive widgets ────────────────────────────────────────────
    src/MessageDetailsWidget.cpp
    include/MessageDetailsWidget.h

    src/DetectionsWidget.cpp
    include/DetectionsWidget.h

    # ── Notebook-exclusive widgets ────────────────────────────────────────────
    src/ScriptEditorWidget.cpp
    include/ScriptEditorWidget.h

    # ── Notebook block wrapper ────────────────────────────────────────────────
    src/NotebookBlockWidget.cpp
    include/NotebookBlockWidget.h

    # ── Overview heatmap widget ───────────────────────────────────────────────
    src/TimelineOverviewWidget.cpp
    include/TimelineOverviewWidget.h

    # ── View layouts ──────────────────────────────────────────────────────────
    src/OverviewView.cpp
    include/OverviewView.h

    src/NotebookView.cpp
    include/NotebookView.h
)

qt_add_ui( fastrace_ui SOURCES
    src/TopBarWidget.ui
    src/TimelineWidget.ui
    src/MessageListWidget.ui
    src/MessageDetailsWidget.ui
    src/DetectionsWidget.ui
    src/ScriptEditorWidget.ui
)


target_link_libraries(fastrace_ui PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Concurrent fastrace_analyzer)

# ui/src/ must be on the include path so that the AUTOUIC-generated
# headers can find their necessary files.
target_include_directories(fastrace_ui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
