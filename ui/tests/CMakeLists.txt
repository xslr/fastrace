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
