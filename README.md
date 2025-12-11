# QuickQtScript
基于QuickJS实现的QtScript接口 / QtScript interface based on QuickJS

# 介绍
本项目是基于QuickJS（[准确说是QuickJS-NG](https://github.com/quickjs-ng/quickjs/tree/v0.11.0)）v0.11.0实现的QtScript的部分接口，基本满足普通使用。更高级的功能后面会慢慢补全。 

本项目的基本目的是，让使用了QtScript编写的上层代码**零改动**就能将底层引擎从自带的引擎迁移到QuickJS引擎。

# 使用
使用（切换）起来非常方便。在`src/QScriptEngine`下有个`ScriptEngine.pri`文件。在你项目的`.pro`文件中引入即可。

比如本项目的`examples/QQScriptDemo/QQScriptDemo.pro`文件中，

假如要使用Qt原版的引擎就这样写：
```bash
QT += script
# include($$PWD/../../src/QScriptEngine/ScriptEngine.pri)
```
假如要使用QuickJS引擎就这样写
```bash
# QT += script
include($$PWD/../../src/QScriptEngine/ScriptEngine.pri)
```

# 特别说明
由于QuickJS官方本身对外并没有提供脚本实时位置（file、line、col）的接口，而这个功能是实现`QScriptEngineAgent`必不可少的，因此，我们对QuickJS的源码的部分文件做了一些更改。

目前更改过的文件为：
```text
quickjs.c
```
若要查看具体做了哪些修改，可以把官方的上述源文件放到文件夹dir1，本项目的文件放到文件夹dir2，然后通过`git diff`命令来查看差异
```text
 git diff dir1 dir2 > changes.patch
```
测试过测试
