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
    src/MainWindow.cpp
    src/MainWindow.h
    src/TopBarWidget.cpp
    src/TopBarWidget.h
    src/TopBarWidget.ui
    src/LeftPanelWidget.cpp
    src/LeftPanelWidget.h
    src/LeftPanelWidget.ui
    src/TimelineWidget.cpp
    src/TimelineWidget.h
    src/TimelineWidget.ui
    src/MessageListWidget.cpp
    src/MessageListWidget.h
    src/MessageListWidget.ui
    src/MessageDetailsWidget.cpp
    src/MessageDetailsWidget.h
    src/MessageDetailsWidget.ui
    src/ScriptEditorWidget.cpp
    src/ScriptEditorWidget.h
    src/ScriptEditorWidget.ui
    src/AnalyzerPreviewWidget.cpp
    src/AnalyzerPreviewWidget.h
    src/AnalyzerPreviewWidget.ui
)

target_link_libraries(fastrace_ui PRIVATE Qt${QT_VERSION_MAJOR}::Widgets fastrace_analyzer)
