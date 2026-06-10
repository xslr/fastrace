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
)

target_link_libraries(fastrace_ui PRIVATE Qt${QT_VERSION_MAJOR}::Widgets fastrace_analyzer)
