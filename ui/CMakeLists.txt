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

    # ── Detection Models ──────────────────────────────────────────────────────
    src/DetectionTableModel.cpp
    include/DetectionTableModel.h
    src/DetectionFilterProxyModel.cpp
    include/DetectionFilterProxyModel.h

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

if(COMMAND qt_add_ui)
    qt_add_ui( fastrace_ui SOURCES
        src/TopBarWidget.ui
        src/TimelineWidget.ui
        src/MessageListWidget.ui
        src/MessageDetailsWidget.ui
        src/DetectionsWidget.ui
        src/ScriptEditorWidget.ui
    )
else()
    target_sources(fastrace_ui PRIVATE
        src/TopBarWidget.ui
        src/TimelineWidget.ui
        src/MessageListWidget.ui
        src/MessageDetailsWidget.ui
        src/DetectionsWidget.ui
        src/ScriptEditorWidget.ui
    )
endif()


target_link_libraries(fastrace_ui PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Concurrent fastrace_analyzer)

# ui/src/ must be on the include path so that the AUTOUIC-generated
# headers can find their necessary files.
target_include_directories(fastrace_ui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(APPLE)
    # macOS Qt 6.8.3 from aqtinstall sometimes implicitly links to AGL which is removed in newer SDKs
    # AGL is no longer present in macOS SDK 14/15. We can try to avoid linking it or link OpenGL.
    # Actually, a known issue with Qt 6 on macos is it references AGL in some CMake configs.
    # We can strip it out from the INTERFACE_LINK_LIBRARIES of Qt6::Gui.
    get_target_property(gui_libs Qt6::Gui INTERFACE_LINK_LIBRARIES)
    if(gui_libs)
        string(REGEX REPLACE "-framework;AGL(;)?" "" gui_libs_new "${gui_libs}")
        string(REGEX REPLACE "-framework AGL(;)?" "" gui_libs_new "${gui_libs_new}")
        set_target_properties(Qt6::Gui PROPERTIES INTERFACE_LINK_LIBRARIES "${gui_libs_new}")
    endif()

    # Also strip from global link directories or flags if they appear there
    string(REGEX REPLACE "-framework;AGL(;)?" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    string(REGEX REPLACE "-framework AGL(;)?" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

    string(REGEX REPLACE "-framework;AGL(;)?" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
    string(REGEX REPLACE "-framework AGL(;)?" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")

    # AGL is also in Qt6::OpenGL and Qt6::Widgets and others
    foreach(qt_comp Gui OpenGL Widgets Core Test Concurrent)
        if (TARGET Qt6::${qt_comp})
            get_target_property(comp_libs Qt6::${qt_comp} INTERFACE_LINK_LIBRARIES)
            if(comp_libs)
                string(REGEX REPLACE "-framework;AGL(;)?" "" comp_libs_new "${comp_libs}")
                string(REGEX REPLACE "-framework AGL(;)?" "" comp_libs_new "${comp_libs_new}")
                set_target_properties(Qt6::${qt_comp} PROPERTIES INTERFACE_LINK_LIBRARIES "${comp_libs_new}")
            endif()
        endif()
    endforeach()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
add_subdirectory(tests)

