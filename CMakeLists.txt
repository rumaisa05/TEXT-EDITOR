
cmake_minimum_required(VERSION 3.16)

project(CS218TextEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Frontend-independent core
file(GLOB BACKEND
    "${CMAKE_CURRENT_SOURCE_DIR}/CS-218_SourceCode/*.cpp"
)

list(REMOVE_ITEM BACKEND
    "${CMAKE_CURRENT_SOURCE_DIR}/CS-218_SourceCode/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/CS-218_SourceCode/QtMain.cpp"
)

# Console frontend (does not require Qt)
add_executable(CS218TextEditorConsole
    CS-218_SourceCode/main.cpp
    ${BACKEND}
)

# Qt GUI is optional
find_package(QT NAMES Qt6 Qt5 QUIET COMPONENTS Widgets)

if(QT_FOUND)

    find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Widgets)

    if(TARGET Qt${QT_VERSION_MAJOR}::Widgets)

        # Qt SVG is optional
        find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Svg)

        add_executable(CS218TextEditor WIN32
            CS-218_SourceCode/QtMain.cpp
            ${BACKEND}
        )

        target_link_libraries(CS218TextEditor
            PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
        )

        if(TARGET Qt${QT_VERSION_MAJOR}::Svg)

            target_link_libraries(CS218TextEditor
                PRIVATE Qt${QT_VERSION_MAJOR}::Svg
            )

            target_compile_definitions(CS218TextEditor
                PRIVATE CS218_HAS_SVG
            )

        else()

            message(STATUS
                "Qt SVG not found: GUI will be built without icons"
            )

        endif()

    else()

        message(STATUS
            "Qt Widgets not found: building console version only"
        )

    endif()

else()

    message(STATUS
        "Qt not found: building console version only"
    )

endif()
