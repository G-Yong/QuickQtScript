cmake_minimum_required(VERSION 3.16)

project(QQScriptDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Concurrent
)

# Check whether Qt Script is available.
# Qt6 normally doesn't provide QtScript, but this preserves the qmake logic.
find_package(Qt6 QUIET COMPONENTS Script)

if(Qt6Script_FOUND)
    set(SCRIPT_LIB Qt6::Script)
else()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../src/QScriptEngine
                     ${CMAKE_BINARY_DIR}/QScriptEngine)

    # Replace ScriptEngine with the actual target name exported
    # by ../../src/QScriptEngine/CMakeLists.txt.
    set(SCRIPT_LIB QuickQtScript)
endif()

qt_standard_project_setup()

qt_add_executable(${PROJECT_NAME}
    main.cpp
    mainwindow.cpp
    myqobject.cpp
    myscriptengineagent.cpp

    codeEditor/jscodeeditor.cpp
    codeEditor/jssyntaxhighlighter.cpp
    codeEditor/linenumberarea.cpp
    codeEditor/codefoldingarea.cpp

    mainwindow.h
    myqobject.h
    barprototype.h
    myscriptcode.h
    myscriptengineagent.h

    codeEditor/jscodeeditor.h
    codeEditor/jssyntaxhighlighter.h
    codeEditor/linenumberarea.h
    codeEditor/codefoldingarea.h

    mainwindow.ui
    js.qrc
)

if(NOT Qt6Script_FOUND)
    target_sources(${PROJECT_NAME}
        PRIVATE
            quickjsTest.h
    )
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Concurrent
        ${SCRIPT_LIB}
)

# Equivalent of:
# DESTDIR = $$OUT_PWD
set_target_properties(${PROJECT_NAME}
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
