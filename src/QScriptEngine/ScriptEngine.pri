QT += core

# 之前把xxx xxx.h xxx.cpp（比如QScriptEngine QScriptEngine.h QScriptEngine.cpp）放在一起时，会出现下面的问题
# 包含此项目后，在linux下编译时，可能会出现 XXXX: No such file or directory的问题
# 查看编译输出可以看到，主要是因为在编译scriptEngine文件夹中的某些cpp文件时，编译器直接试图将其编译成可执行文件
# 其试图编译成的可执行文件，恰好是和scriptEngine文件夹下的无后缀的文件一致，比如QScriptEngine。
# 经过查找资料，发现可能是由于这些没有后缀的文件触发了Makefile的一些隐藏规则
# 因此假如想要顺利编译，得在Makefile中禁用隐藏规则（或者还有其他更好的办法，因为Qt也有这些无后缀文件）
# 在 Makefile 中加上以下语句，禁用隐藏规则
# .SUFFIXES:  # 清空后缀列表

# 现在分开了，就没问题了

INCLUDEPATH += $$PWD
INCLUDEPATH += $$PWD/quickjs
INCLUDEPATH += $$PWD/scriptEngine/include

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
    $$PWD/scriptEngine/include/QScriptContext.h \
    $$PWD/scriptEngine/include/QScriptEngine.h \
    $$PWD/scriptEngine/include/QScriptValue.h \
    $$PWD/scriptEngine/include/QScriptValueIterator.h \
    $$PWD/scriptEngine/include/QScriptEngineAgent.h \
    $$PWD/scriptEngine/include/QScriptContextInfo.h \
    $$PWD/scriptEngine/include/QScriptSyntaxCheckResult.h


win32: {
    DEFINES += __TINYC__
    LIBS += -lws2_32 -liphlpapi
    DEFINES += WIN32_LEAN_AND_MEAN
} else {
    # 在quick.js 第52行左右有这么一段代码
       #if defined(EMSCRIPTEN) || defined(_MSC_VER)
       #define DIRECT_DISPATCH  0
       #else
       #define DIRECT_DISPATCH  1
       #endif
    # 也就意味着在Linux下，DIRECT_DISPATCH会被定义为1
    # 经过测试，这样会导致JS_CallInternal()只被调用一次，从而使我们的机制失效
    # 因此需要强行让其使用  #define DIRECT_DISPATCH  0

    # 添加linux系统可能用到的宏定义
    DEFINES += _GNU_SOURCE
    # DEFINES += JS_HAVE_THREADS
}

# 还是直接固定使用吧。否则在window下使用mingw时，又被钻了空子
    DEFINES += EMSCRIPTEN
