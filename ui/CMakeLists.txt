cmake_minimum_required(VERSION 3.20)
project(fastrace_ui LANGUAGES CXX)

# Support Qt6
find_package(QT NAMES Qt6 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
add_executable(fastrace_ui
    src/main.cpp

    # ── Core window ────────────────────────────────────────────────────────────
    src/MainWindow.cpp
    src/MainWindow.h

    # ── Top bar (with Overview / Notebook toggle) ──────────────────────────────
    src/TopBarWidget.cpp
    src/TopBarWidget.h
    src/TopBarWidget.ui

    # ── Custom DB combobox ────────────────────────────────────────────────────
    src/DatabaseComboBox.cpp
    src/DatabaseComboBox.h

    # ── Shared left panel ─────────────────────────────────────────────────────
    # (LeftPanelWidget removed from build)


    # ── Shared timeline ───────────────────────────────────────────────────────
    src/TimelineWidget.cpp
    src/TimelineWidget.h

    # ── Shared message list ───────────────────────────────────────────────────
    src/MessageListWidget.cpp
    src/MessageListWidget.h

    # ── Overview-exclusive widgets ────────────────────────────────────────────
    src/MessageDetailsWidget.cpp
    src/MessageDetailsWidget.h

    src/DetectionsWidget.cpp
    src/DetectionsWidget.h

    # ── Notebook-exclusive widgets ────────────────────────────────────────────
    src/ScriptEditorWidget.cpp
    src/ScriptEditorWidget.h

    # ── Notebook block wrapper ────────────────────────────────────────────────
    src/NotebookBlockWidget.cpp
    src/NotebookBlockWidget.h

    # ── View layouts ──────────────────────────────────────────────────────────
    src/OverviewView.cpp
    src/OverviewView.h

    src/NotebookView.cpp
    src/NotebookView.h
)

qt_add_ui( fastrace_ui SOURCES
    src/TopBarWidget.ui
    src/TimelineWidget.ui
    src/MessageListWidget.ui
    src/MessageDetailsWidget.ui
    src/DetectionsWidget.ui
    src/ScriptEditorWidget.ui
)


target_link_libraries(fastrace_ui PRIVATE Qt${QT_VERSION_MAJOR}::Widgets fastrace_analyzer)

# ui/src/ must be on the include path so that the AUTOUIC-generated
# ui_TopBarWidget.h can find promoted-widget headers (e.g. DatabaseComboBox.h).
target_include_directories(fastrace_ui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
