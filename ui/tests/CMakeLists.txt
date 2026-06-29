cmake_minimum_required(VERSION 3.20)
project(fastrace_ui_tests LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Qt discovery
# ---------------------------------------------------------------------------
find_package(QT NAMES Qt6 COMPONENTS Widgets Concurrent Test REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets Concurrent Test REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ---------------------------------------------------------------------------
# Shared include / source roots (relative to this CMakeLists.txt)
# ---------------------------------------------------------------------------
set(UI_INC  "${CMAKE_CURRENT_SOURCE_DIR}/../include")
set(UI_SRC  "${CMAKE_CURRENT_SOURCE_DIR}/../src")

# ---------------------------------------------------------------------------
# tst_timeline_widget  –  tests for TimelineWidget
# ---------------------------------------------------------------------------
add_executable(tst_timeline_widget
    tst_TimelineWidget.cpp
    # Widget source under test (compiled once for this target)
    ${UI_SRC}/TimelineWidget.cpp
    # Header included so AUTOMOC generates the required moc_*.cpp
    ${UI_INC}/TimelineWidget.h
)

# AUTOUIC must find TimelineWidget.ui which lives in UI_SRC
set_target_properties(tst_timeline_widget PROPERTIES
    AUTOUIC_SEARCH_PATHS "${UI_SRC}"
)

target_include_directories(tst_timeline_widget PRIVATE
    ${UI_INC}
    ${UI_SRC}   # so AUTOUIC-generated ui_TimelineWidget.h is found
)

target_link_libraries(tst_timeline_widget PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Test
    fastrace_analyzer
)

# ---------------------------------------------------------------------------
# tst_timeline_overview_widget  –  tests for TimelineOverviewWidget
# ---------------------------------------------------------------------------
add_executable(tst_timeline_overview_widget
    tst_TimelineOverviewWidget.cpp
    # Widget source under test
    ${UI_SRC}/TimelineOverviewWidget.cpp
    # Header included so AUTOMOC generates the required moc_*.cpp
    ${UI_INC}/TimelineOverviewWidget.h
)

target_include_directories(tst_timeline_overview_widget PRIVATE
    ${UI_INC}
    ${UI_SRC}
)

target_link_libraries(tst_timeline_overview_widget PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Test
    fastrace_analyzer
)

if(APPLE)
    # Strip from global link directories or flags if they appear there
    string(REPLACE "-framework AGL" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    string(REPLACE "-framework AGL" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
    string(REPLACE "-framework;AGL" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    string(REPLACE "-framework;AGL" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")

    # Strip from targets directly
    get_target_property(tst_link_libs tst_timeline_widget LINK_LIBRARIES)
    if(tst_link_libs)
        string(REPLACE "-framework;AGL" "" tst_link_libs_new "${tst_link_libs}")
        string(REPLACE "-framework AGL" "" tst_link_libs_new "${tst_link_libs_new}")
        set_target_properties(tst_timeline_widget PROPERTIES LINK_LIBRARIES "${tst_link_libs_new}")
        set_target_properties(tst_timeline_overview_widget PROPERTIES LINK_LIBRARIES "${tst_link_libs_new}")
    endif()

    # macOS Qt 6.8.3 from aqtinstall sometimes implicitly links to AGL which is removed in newer SDKs
    # AGL is no longer present in macOS SDK 14/15. We can try to avoid linking it or link OpenGL.
    # Actually, a known issue with Qt 6 on macos is it references AGL in some CMake configs.
    # We can strip it out from the INTERFACE_LINK_LIBRARIES of Qt6::Gui.
    get_target_property(gui_libs Qt6::Gui INTERFACE_LINK_LIBRARIES)
    if(gui_libs)
        string(REPLACE "-framework AGL" "" gui_libs_new "${gui_libs}")
        set_target_properties(Qt6::Gui PROPERTIES INTERFACE_LINK_LIBRARIES "${gui_libs_new}")
    endif()

    # AGL is also in Qt6::OpenGL and Qt6::Widgets and others
    foreach(qt_comp Gui OpenGL Widgets Core Test Concurrent)
        if (TARGET Qt6::${qt_comp})
            get_target_property(comp_libs Qt6::${qt_comp} INTERFACE_LINK_LIBRARIES)
            if(comp_libs)
                string(REPLACE "-framework AGL" "" comp_libs_new "${comp_libs}")
                set_target_properties(Qt6::${qt_comp} PROPERTIES INTERFACE_LINK_LIBRARIES "${comp_libs_new}")
            endif()
        endif()
    endforeach()
endif()


# ---------------------------------------------------------------------------
# CTest registration
# ---------------------------------------------------------------------------
enable_testing()

add_test(
    NAME    TimelineWidgetTests
    COMMAND tst_timeline_widget
)

add_test(
    NAME    TimelineOverviewWidgetTests
    COMMAND tst_timeline_overview_widget
)

if(APPLE)
    # Strip from global link directories or flags if they appear there
    string(REPLACE "-framework AGL" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    string(REPLACE "-framework AGL" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
    string(REPLACE "-framework;AGL" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    string(REPLACE "-framework;AGL" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")

    # Strip from targets directly
    get_target_property(tst_link_libs tst_timeline_widget LINK_LIBRARIES)
    if(tst_link_libs)
        string(REPLACE "-framework;AGL" "" tst_link_libs_new "${tst_link_libs}")
        string(REPLACE "-framework AGL" "" tst_link_libs_new "${tst_link_libs_new}")
        set_target_properties(tst_timeline_widget PROPERTIES LINK_LIBRARIES "${tst_link_libs_new}")
        set_target_properties(tst_timeline_overview_widget PROPERTIES LINK_LIBRARIES "${tst_link_libs_new}")
    endif()

    # macOS Qt 6.8.3 from aqtinstall sometimes implicitly links to AGL which is removed in newer SDKs
    # AGL is no longer present in macOS SDK 14/15. We can try to avoid linking it or link OpenGL.
    # Actually, a known issue with Qt 6 on macos is it references AGL in some CMake configs.
    # We can strip it out from the INTERFACE_LINK_LIBRARIES of Qt6::Gui.
    get_target_property(gui_libs Qt6::Gui INTERFACE_LINK_LIBRARIES)
    if(gui_libs)
        string(REPLACE "-framework AGL" "" gui_libs_new "${gui_libs}")
        set_target_properties(Qt6::Gui PROPERTIES INTERFACE_LINK_LIBRARIES "${gui_libs_new}")
    endif()

    # AGL is also in Qt6::OpenGL and Qt6::Widgets and others
    foreach(qt_comp Gui OpenGL Widgets Core Test Concurrent)
        if (TARGET Qt6::${qt_comp})
            get_target_property(comp_libs Qt6::${qt_comp} INTERFACE_LINK_LIBRARIES)
            if(comp_libs)
                string(REPLACE "-framework;AGL" "" comp_libs_new "${comp_libs}")
                string(REPLACE "-framework AGL" "" comp_libs_new "${comp_libs_new}")
                set_target_properties(Qt6::${qt_comp} PROPERTIES INTERFACE_LINK_LIBRARIES "${comp_libs_new}")
            endif()
        endif()
    endforeach()
endif()
