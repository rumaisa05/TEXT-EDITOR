cmake_minimum_required(VERSION 3.16)
project(CS218TextEditor LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# Frontend-independent core: document, undo, edit operations, search, replace, files.
file(GLOB BACKEND "CS-218_SourceCode/*.cpp")
list(REMOVE_ITEM BACKEND
    "${CMAKE_CURRENT_SOURCE_DIR}/CS-218_SourceCode/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CS-218_SourceCode/QtMain.cpp")

# Qt GUI. Qt SVG is optional: it only provides the toolbar/sidebar icons.
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Svg)
add_executable(CS218TextEditor WIN32 CS-218_SourceCode/QtMain.cpp ${BACKEND})
target_link_libraries(CS218TextEditor PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
if(Qt${QT_VERSION_MAJOR}Svg_FOUND)
    target_link_libraries(CS218TextEditor PRIVATE Qt${QT_VERSION_MAJOR}::Svg)
    target_compile_definitions(CS218TextEditor PRIVATE CS218_HAS_SVG)
else()
    message(STATUS "Qt SVG not found: the GUI will be built without icons")
endif()

# Console frontend (no Qt code involved)
add_executable(CS218TextEditorConsole CS-218_SourceCode/main.cpp ${BACKEND})
