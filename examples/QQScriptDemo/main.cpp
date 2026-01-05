#include "mainwindow.h"

#include <QApplication>

#ifdef __TINYC__
#include "quickjsTest.h"
#endif


int main(int argc, char *argv[])
{
    // qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication a(argc, argv);

    MainWindow w;
    w.show();

    // // quickjsTest();
    // scriptEngineTest();

    // 深拷贝测试代码
    QScriptEngine engine;
    QScriptValue obj = engine.newObject();
    obj.setProperty("x", 10);
    obj.setProperty("str", "字符串");
    obj.setProperty("plus", engine.newFunction([](QScriptContext *, QScriptEngine *)->QScriptValue{
        return QScriptValue();
    }));

    auto arr = engine.newArray(0);
    arr.setProperty(quint32(0), 42);
    arr.setProperty(quint32(1), "hello");
    arr.setProperty(quint32(2), true);

    // 最终数组为: [42, "hello", true]
    obj.setProperty("Array", arr);

    obj.setProperty("Undefined", engine.undefinedValue());
    obj.setProperty("Null", engine.nullValue());

    // 确保 QByteArray 生命周期
    QByteArray expBytes = QString("异常类型").toUtf8();
    auto value = JS_ThrowPlainError(engine.ctx(), "%s", expBytes.constData());
    auto qVal = QScriptValue(engine.ctx(), value, &engine);
    JS_FreeValue(engine.ctx(), value);
    obj.setProperty("Exception", qVal);

    // 添加 Symbol
    JSValue normalSymbol = JS_NewSymbol(engine.ctx(), "", false);
    QScriptValue qNormalSymbol(engine.ctx(), normalSymbol, &engine);
    JS_FreeValue(engine.ctx(), normalSymbol);
    obj.setProperty("normalSymbol", qNormalSymbol);

    qDebug() << obj.toString();

    // 深拷贝
    QScriptValue obj2 = engine.newObject();
    QScriptValueIterator it(obj);
    while (it.hasNext()) {
        it.next();
        obj2.setProperty(it.name(), it.value());
    }
    qDebug() << obj2.toString();

    auto Symbol1 = QScriptValue(engine.ctx(),JS_NewSymbol(engine.ctx(), "mySymbol", false), &engine);
    auto Symbol2 = QScriptValue(engine.ctx(),JS_NewSymbol(engine.ctx(), "mySymbol", false), &engine);
    qDebug() << "Symbol1 === Symbol1 is :" << Symbol1.strictlyEquals(Symbol2);

    auto sym1 = obj.property("normalSymbol");
    // 正常的结果来说，应该是true，不过现在的deepclone实现是创建新的symbol，导致结果为false
    qDebug() << "obj.normalSymbol === obj2.normalSymbol is :" << sym1.strictlyEquals(obj2.property("normalSymbol"));

    auto str1 = obj.property("str");
    qDebug() << "obj.str === obj2.str is :" << str1.strictlyEquals(obj2.property("str"));

    return a.exec();
}
