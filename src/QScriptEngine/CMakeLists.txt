cmake_minimum_required(VERSION 3.16)
project(QuickQtScript LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)

find_package(Qt6 REQUIRED COMPONENTS Core)

# -------------------------------------------------
# Includes
# -------------------------------------------------
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/include
)


# -------------------------------------------------
# Option: system or bundled QuickJS
# -------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/quickjs/CMakeLists.txt")
    set(USE_SYSTEM_QUICKJS_DEFAULT OFF)
else()
    set(USE_SYSTEM_QUICKJS_DEFAULT ON)
endif()

option(USE_SYSTEM_QUICKJS "Use system-installed QuickJS instead of bundled" ${USE_SYSTEM_QUICKJS_DEFAULT})

# -------------------------------------------------
# QuickJS selection
# -------------------------------------------------
if(USE_SYSTEM_QUICKJS)
    message(STATUS "Using SYSTEM QuickJS")
    find_package(qjs REQUIRED CONFIG)
else()
    message(STATUS "Using BUNDLED QuickJS")

    add_subdirectory(quickjs)
endif()

# -------------------------------------------------
# Library target
# -------------------------------------------------
add_library(${PROJECT_NAME} STATIC SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/QScriptContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/QScriptEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/QScriptValue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/QScriptValueIterator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/QScriptEngineAgent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/QScriptContextInfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/QScriptSyntaxCheckResult.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/include/QScriptContext.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/include/QScriptEngine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/include/QScriptValue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/include/QScriptValueIterator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/include/QScriptEngineAgent.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/include/QScriptContextInfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/include/QScriptSyntaxCheckResult.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
)

target_compile_options(${PROJECT_NAME} PRIVATE -fpermissive -Wno-narrowing)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/include
)

# -------------------------------------------------
# Compile definitions
# -------------------------------------------------

target_compile_definitions(${PROJECT_NAME} PRIVATE
    JS_HAVE_THREADS
    _GNU_SOURCE
)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        __TINYC__
        WIN32_LEAN_AND_MEAN
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        ws2_32
        iphlpapi
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE qjs)

# -------------------------------------------------
# Optional feature (USE_LIBC)
# -------------------------------------------------
option(USE_LIBC "Enable libc support" OFF)

if (USE_LIBC)
    target_sources(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/QScriptLibrary.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/scriptEngine/include/QScriptLibrary.h
    )
endif()
