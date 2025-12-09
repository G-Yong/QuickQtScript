QT += core

INCLUDEPATH += $$PWD
INCLUDEPATH += $$PWD/scriptEngine

# 注意，此处使用的是 quickjs-ng https://github.com/quickjs-ng/quickjs
# 而不是原版的，原版的对windows支持不好 https://github.com/bellard/quickjs
SOURCES += \
    $$PWD/quickjs/cutils.c \
    $$PWD/quickjs/dtoa.c \
    $$PWD/quickjs/libregexp.c \
    $$PWD/quickjs/libunicode.c \
    $$PWD/quickjs/quickjs.c \
    $$PWD/scriptEngine/QScriptContext.cpp \
    $$PWD/scriptEngine/QScriptEngine.cpp \
    $$PWD/scriptEngine/QScriptValue.cpp \
    $$PWD/scriptEngine/QScriptValueIterator.cpp \
    $$PWD/scriptEngine/QScriptEngineAgent.cpp \
    $$PWD/scriptEngine/QScriptContextInfo.cpp \
    $$PWD/scriptEngine/QScriptSyntaxCheckResult.cpp


HEADERS += \
    $$PWD/quickjs/cutils.h \
    $$PWD/quickjs/dtoa.h \
    $$PWD/quickjs/libregexp-opcode.h \
    $$PWD/quickjs/libregexp.h \
    $$PWD/quickjs/libunicode-table.h \
    $$PWD/quickjs/libunicode.h \
    $$PWD/quickjs/quickjs.h \
    $$PWD/scriptEngine/QScriptContext.h \
    $$PWD/scriptEngine/QScriptEngine.h \
    $$PWD/scriptEngine/QScriptValue.h \
    $$PWD/scriptEngine/QScriptValueIterator.h \
    $$PWD/scriptEngine/QScriptEngineAgent.h \
    $$PWD/scriptEngine/QScriptContextInfo.h \
    $$PWD/scriptEngine/QScriptSyntaxCheckResult.h


    DEFINES += __TINYC__
win32: {
    # DEFINES += __TINYC__

    LIBS += -lws2_32 -liphlpapi
    DEFINES += WIN32_LEAN_AND_MEAN
}
