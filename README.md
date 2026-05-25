# QuickQtScript

**English** | [中文](README.zh-CN.md)

QtScript interface based on QuickJS

## Introduction

QuickQtScript implements a subset of the QtScript API on top of [QuickJS-NG](https://github.com/quickjs-ng/quickjs), covering the most commonly used functionality. More advanced features will be added gradually.

The primary goal of this project is to allow upper-layer code written against the QtScript API to migrate from Qt's built-in script engine to the QuickJS engine with **zero changes**.

## Usage

Switching engines is straightforward. There is a `ScriptEngine.pri` file under `src/QScriptEngine`. Simply include it in your project's `.pro` file.

For example, in `examples/QQScriptDemo/QQScriptDemo.pro`:

To use Qt's built-in script engine:
```qmake
QT += script
# include($$PWD/../../src/QScriptEngine/ScriptEngine.pri)
```

To use the QuickJS engine:
```qmake
# QT += script
include($$PWD/../../src/QScriptEngine/ScriptEngine.pri)
```

## Notes

QuickJS does not expose a public API for real-time script position information (file, line, column). This information is essential for implementing `QScriptEngineAgent`, so we have made targeted modifications to parts of the QuickJS source.

To see exactly what was changed, refer to the fork repository: [G-Yong/quickjs](https://github.com/G-Yong/quickjs)
1111
