#include <QCoreApplication>
#include <QDebug>
#include <QScriptContext>
#include <QScriptEngine>
#include <QScriptEngineAgent>
#include <QScriptValue>

#ifdef Q_OS_WIN
#include <crtdbg.h>
#endif

#ifdef Q_OS_WIN
#pragma execution_character_set("utf-8")
#endif

/*
 * 复现思路说明：
 *
 * 1. 崩溃并不是发生在脚本读取/写入属性的瞬间，而是发生在 QScriptEngine
 *    析构时。析构函数最后会释放 QuickJS 的 JSRuntime；如果此前注册属性时
 *    JSValue 引用所有权处理错误，JS_FreeRuntime() 会在清理阶段触发断言。
 *
 * 2. 不能只写一段纯 JS 的 Object.defineProperty() 来复现。纯 JS accessor
 *    走的是 QuickJS 自己的属性定义路径，不会经过 QScriptValue::setProperty()。
 *    本问题的根因在 C++ 封装层把 QScriptValue 注册成 getter/setter 时的
 *    JSValue 引用管理，所以测试必须从 C++ 调用 setProperty()。
 *
 * 3. 这个测试只保留触发根因所需的最小条件：
 *    - 创建一个 QScriptEngine；
 *    - 用 engine.newFunction() 创建 native 函数；
 *    - 通过 setProperty(..., PropertyGetter | PropertySetter) 注册读写属性；
 *    - 再注册一个 getter-only 属性覆盖另一条 accessor 分支；
 *    - 读写这些属性确认脚本行为正确；
 *    - 离开作用域触发 QScriptEngine 析构，验证 JSRuntime 可以被正常释放。
 *
 * 4. 如果实现有问题，测试通常会在作用域结束后的 QScriptEngine 析构阶段
 *    触发 QuickJS 断言或卡住；如果实现正确，进程会正常退出并返回 0。
 */
namespace {

QScriptValue accessor(QScriptContext *context, QScriptEngine *, void *)
{
    // QuickQtScript 使用同一个 native 函数模拟 getter 和 setter：
    // setter 调用时会带 1 个参数，getter 调用时没有参数。
    if(context->argumentCount() == 1)
    {
        return QScriptValue(0);
    }

    return QScriptValue(42);
}

bool expectInt(QScriptEngine &engine,
               const QString &script,
               int expected,
               const QString &message)
{
    QScriptValue result = engine.evaluate(script);
    if(result.isError())
    {
        qCritical() << "script error:" << message << result.toString();
        return false;
    }

    const int actual = result.toInt32();
    if(actual != expected)
    {
        qCritical() << "unexpected result:" << message
                    << "expected" << expected
                    << "actual" << actual;
        return false;
    }

    return true;
}

void registerAccessorProperties(QScriptEngine &engine)
{
    // Step 1: 注册读写 accessor 属性。
    //
    // 同一个 QScriptValue 函数同时作为 getter 和 setter 时，QuickJS 会分别
    // 接管 getter 和 setter 两个槽位中的 JSValue。因此封装层必须给 getter
    // 和 setter 各自准备独立引用，不能把同一个 JSValue 引用同时交给两边。
    //
    // 注册多组属性不是为了依赖数量触发崩溃，而是让引用释放路径更稳定地暴露；
    // 32 组已经足够覆盖重复注册场景，同时保持测试很轻。
    constexpr int propertyCount = 32;
    for(int i = 0; i < propertyCount; ++i)
    {
        QScriptValue readWriteAccessor = engine.newFunction(accessor, nullptr);
        const QString name = QStringLiteral("ACCESSOR_RW_%1")
                                 .arg(i + 1, 2, 10, QLatin1Char('0'));

        engine.globalObject().setProperty(
            name,
            readWriteAccessor,
            QScriptValue::PropertyGetter | QScriptValue::PropertySetter);
    }

    // Step 2: 注册 getter-only accessor 属性。
    //
    // 这覆盖另一条关键路径：当 flags 表示 accessor 属性时，不应该提前创建普通
    // value 属性使用的 JSValue。否则即使没有 setter，也会因为提前创建的引用
    // 没有交给 QuickJS 接管而泄漏，最终在 JSRuntime 释放阶段被发现。
    QScriptValue readOnlyAccessor = engine.newFunction(accessor, nullptr);
    engine.globalObject().setProperty(
        QStringLiteral("ACCESSOR_READ_ONLY"),
        readOnlyAccessor,
        QScriptValue::PropertyGetter);
}

} // namespace

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif

    QCoreApplication app(argc, argv);
    bool ok = true;

    {
        // Step 3: 用局部作用域控制 QScriptEngine 生命周期。
        //
        // 本问题的观察点在析构阶段，所以不能只验证 evaluate() 返回值。这里让
        // engine 在块结束时析构，测试才会真正走到 JS_FreeRuntime()。
        QScriptEngine engine;

        // Step 4: 附加 agent，覆盖带调试跟踪对象的常见生命周期。
        //
        // agent 不是触发根因的必要条件，但原崩溃栈包含脚本执行线程和引擎析构。
        // 保留它可以让测试更接近实际的 QScriptEngine 生命周期，同时仍然保持独立。
        QScriptEngineAgent agent(&engine);

        // Step 5: 通过 C++ 封装层注册 accessor 属性，命中真正的 setProperty()
        // 代码路径；纯 JS 定义 accessor 无法覆盖这里。
        registerAccessorProperties(engine);

        // Step 6: 先验证脚本层行为正确。
        //
        // 如果 getter/setter 行为本身不对，后面的析构检查就没有意义；这些断言
        // 确保属性确实按 accessor 的方式被读写。
        ok &= expectInt(engine,
                        QStringLiteral("ACCESSOR_RW_01"),
                        42,
                        QStringLiteral("read getter"));
        ok &= expectInt(engine,
                        QStringLiteral("ACCESSOR_RW_01 = 7; ACCESSOR_RW_01"),
                        42,
                        QStringLiteral("write through setter and read getter"));
        ok &= expectInt(engine,
                        QStringLiteral("ACCESSOR_READ_ONLY"),
                        42,
                        QStringLiteral("read getter-only property"));

        // Step 7: 离开此作用域后 engine 析构。
        //
        // 旧实现会在这里附近触发 JS_FreeRuntime() 断言；修复后应正常继续执行。
    }

    if(!ok)
    {
        return 1;
    }

    qInfo() << "QScriptEngine accessor lifetime test passed";
    return 0;
}
