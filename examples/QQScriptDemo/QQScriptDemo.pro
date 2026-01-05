QT       += core gui concurrent

# QT += script
contains(QT, script){
}else{
#当不使用Qt自带的script时，使用自行定义的脚本
include($$PWD/../../src/QScriptEngine/ScriptEngine.pri)
}

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    codeEditor/jscodeeditor.cpp \
    codeEditor/jssyntaxhighlighter.cpp \
    codeEditor/linenumberarea.cpp \
    codeEditor/codefoldingarea.cpp \
    main.cpp \
    mainwindow.cpp \
    myqobject.cpp \
    myscriptengineagent.cpp

HEADERS += \
    codeEditor/jscodeeditor.h \
    codeEditor/jssyntaxhighlighter.h \
    codeEditor/linenumberarea.h \
    codeEditor/codefoldingarea.h \
    mainwindow.h \
    myqobject.h \
    barprototype.h \
    myscriptengineagent.h

FORMS += \
    mainwindow.ui

contains(QT, script){
} else {
    HEADERS += \
        quickjsTest.h
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
