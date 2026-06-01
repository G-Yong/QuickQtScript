QT += core
QT -= gui

CONFIG += console c++17
CONFIG -= app_bundle

msvc:QMAKE_CXXFLAGS += /utf-8

TARGET = qscriptengine_memory_safety_test
TEMPLATE = app

include($$PWD/../../src/QScriptEngine/ScriptEngine.pri)

SOURCES += \
    main.cpp
