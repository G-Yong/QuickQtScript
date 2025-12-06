#include <QScriptEngine.h>
#include <QScriptValue.h>
#include <QScriptContext.h>
#include <QScriptEngineAgent>

#include <QDebug>

#include <mutex>
#include <vector>
#include <atomic>

extern "C" {
#include "../quickjs/quickjs.h"
}

// QObject wrapper for QuickJS opaque
struct QObjectWrapper {
    QObject *obj;
    QScriptEngine::ValueOwnership ownership;
};

static JSClassID s_qobjectClassId = 0;

static void qobject_finalizer(JSRuntime *rt, JSValueConst val)
{
    void *p = JS_GetOpaque(val, s_qobjectClassId);
    if (!p)
        return;
    QObjectWrapper *w = static_cast<QObjectWrapper*>(p);
    if (w->ownership == QScriptEngine::ScriptOwnership) {
        if (w->obj) {
            // delete QObject immediately. In complex apps you might prefer deleteLater().
            delete w->obj;
            w->obj = nullptr;
        }
    }
    delete w;
    JS_SetOpaque(val, nullptr);
}

// 要明确知道什么时候该用JS_DupValue，什么时候不该用


// 中断函数，实现abortEval
static int custom_interrupt_handler(JSRuntime *rt, void *opaque) {
    QScriptEngine *engine = static_cast<QScriptEngine*>(opaque);
    if (!engine)
        return 1;

    // 检查中断标志
    if (std::atomic_load(&engine->interrupt_flag)) {
        // 可以在这里进行清理
        qDebug() << "Interrupting script execution" << QDateTime::currentDateTime();
        return 1;  // 返回1表示请求中断
    }
    return 0;
}

// 处理注册的c++函数
static JSValue nativeFunctionShim(JSContext *ctx,
                                  JSValueConst this_val,
                                  int argc, JSValueConst *argv,
                                  int magic)
{
    // qDebug() << "magic:" << magic;

    // retrieve the QScriptEngine associated with this JSContext
    void *opaque = JS_GetContextOpaque(ctx);
    QScriptEngine *engine = static_cast<QScriptEngine*>(opaque);
    if (!engine)
        return JS_UNDEFINED;

    QScriptEngine::FunctionWithArgSignature func = nullptr;
    void *arg = nullptr;
    if (!engine->getNativeEntry(magic, func, &arg))
        return JS_UNDEFINED;

    QScriptContext qctx(ctx, this_val, argc, argv, engine);
    QScriptValue res = func(&qctx, engine, arg);

    // qDebug() << "the returnd val:"
    //          << res.data()
    //          << res.engine()
    //          << JS_IsUndefined(res.rawValue())
    //          << res.toString();

    // 假如返回值是普通值，需要构建成JSValue
    if(res.isVariant())
    {
        return QScriptValue::toJSValue(ctx, res.data());
    }

    // Don't use the C++ '!' operator on JSValue; instead check validity/undefined
    if (!res.isValid() || res.isUndefined())
        return JS_UNDEFINED;

    // qDebug() << "return dup";

    auto val = JS_DupValue(ctx, res.rawValue());

    return val;
}

QScriptEngine::QScriptEngine(QObject *parent)
    : QObject(parent)
{
    m_rt = JS_NewRuntime();
    if(!m_rt)
    {
        qCritical() << "create js runtime fail";
        return;
    }

    m_ctx = JS_NewContext(m_rt);
    if(!m_ctx)
    {
        qCritical() << "create js context fail";
        JS_FreeRuntime(m_rt);
        m_rt = nullptr;
        return;
    }

    // associate C++ object with the QuickJS context
    JS_SetContextOpaque(m_ctx, this);

    // 重置中断标志
    std::atomic_store(&interrupt_flag, 0);
    // 设置中断处理器
    JS_SetInterruptHandler(m_rt, custom_interrupt_handler, this);

    // Register a QuickJS class to wrap QObject pointers
    if (m_rt) {
        JS_NewClassID(m_rt, &m_qobjectClassId);
        s_qobjectClassId = m_qobjectClassId;
        JSClassDef cd;
        memset(&cd, 0, sizeof(cd));
        cd.class_name = "QScriptQObject";
        cd.finalizer = qobject_finalizer;
        JS_NewClass(m_rt, m_qobjectClassId, &cd);
    }
}

QScriptEngine::~QScriptEngine()
{
    if(agent() != nullptr)
    {
        for (int i = 0; i < mFileNameBuffer.length(); ++i) {
            agent()->scriptUnload(i);
        }
    }

    if (m_ctx) {
        // JS_RunGC(m_rt);

        // clear context opaque to avoid dangling pointer
        JS_SetContextOpaque(m_ctx, nullptr);
        JS_FreeContext(m_ctx);
        m_ctx = nullptr;
    }
    if (m_rt) {
        JS_FreeRuntime(m_rt);
        m_rt = nullptr;
    }
}

bool QScriptEngine::isEvaluating() const
{
    return m_evalCount.load(std::memory_order_relaxed) > 0;
}

void QScriptEngine::abortEvaluation(const QScriptValue &result)
{
    // if (!m_ctx)
    //     return;
    // JS_Throw(m_ctx, JS_DupValue(m_ctx, result.rawValue()));

    std::atomic_store(&interrupt_flag, 1);
}

void QScriptEngine::setAgent(QScriptEngineAgent *agent)
{
    m_agent = agent;
}

QScriptEngineAgent *QScriptEngine::agent() const
{
    return m_agent;
}

void QScriptEngine::collectGarbage()
{
    if (m_rt)
        JS_RunGC(m_rt);
}

QScriptContext *QScriptEngine::currentContext() const
{
    return nullptr;
}

QScriptValue QScriptEngine::evaluate(const QString &program, const QString &fileName, int lineNumber)
{
    if (!m_ctx)
        return QScriptValue();

    mFileNameBuffer.push_back(fileName);
    if(agent() != nullptr)
    {
        agent()->scriptLoad(mFileNameBuffer.length() - 1, program, fileName, lineNumber);
    }

    struct EvalGuard {
        std::atomic<int> &cnt;
        EvalGuard(std::atomic<int> &c) : cnt(c) { cnt.fetch_add(1, std::memory_order_relaxed); }
        ~EvalGuard() { cnt.fetch_sub(1, std::memory_order_relaxed); }
    } guard(m_evalCount);

    // 中断标志位复位
    std::atomic_store(&interrupt_flag, 0);

    QByteArray ba = program.toUtf8();
    QByteArray fnba = fileName.toUtf8();
    const char *fn = fileName.isEmpty() ? "<eval>" : fnba.constData();
    JSValue val = JS_Eval(m_ctx,
                          ba.constData(),
                          ba.size(),
                          fn,
                          JS_EVAL_TYPE_GLOBAL);

    // if (JS_IsException(val))
    // {
    //     // wrap exception
    //     JSValue exception = JS_GetException(m_ctx);
    //     QScriptValue qVal = QScriptValue(m_ctx, exception, const_cast<QScriptEngine*>(this));

    //     JS_FreeValue(m_ctx, val);
    //     JS_FreeValue(m_ctx, exception);

    //     // qDebug() << "exception:" << qVal.toString();

    //     return qVal;
    // }

    QScriptValue qVal = QScriptValue(m_ctx, val, const_cast<QScriptEngine*>(this));

    JS_FreeValue(m_ctx, val);

    return qVal;
}

QScriptValue QScriptEngine::globalObject() const
{
    if (!m_ctx)
        return QScriptValue();

    JSValue g = JS_GetGlobalObject(m_ctx);
    // return QScriptValue(m_ctx, g, const_cast<QScriptEngine*>(this));

    QScriptValue qVal = QScriptValue(m_ctx, g, const_cast<QScriptEngine*>(this));

    JS_FreeValue(m_ctx, g);

    return qVal;
}

void QScriptEngine::setGlobalObject(const QScriptValue &object)
{
    Q_UNUSED(object);
    // Not implemented: replacing the global object is non-trivial in QuickJS.
}

QScriptValue QScriptEngine::newObject()
{
    if (!m_ctx)
        return QScriptValue();

    JSValue val = JS_NewObject(m_ctx);

    QScriptValue qVal = QScriptValue(m_ctx, val, this);

    JS_FreeValue(m_ctx, val);

    return qVal;
}

QScriptValue QScriptEngine::newObject(QScriptClass *scriptClass, const QScriptValue &data)
{
    Q_UNUSED(scriptClass);
    Q_UNUSED(data);
    return newObject();
}

QScriptValue QScriptEngine::newArray(uint length)
{
    if (!m_ctx)
        return QScriptValue();

    JSValue arr = JS_NewArray(m_ctx);

    if (length > 0)
        JS_SetLength(m_ctx, arr, length);

    QScriptValue qVal = QScriptValue(m_ctx, arr, this);

    JS_FreeValue(m_ctx, arr);

    return qVal;
}

QScriptValue QScriptEngine::newFunction(FunctionWithArgSignature signature, void *arg)
{
    if (!m_ctx)
        return QScriptValue();
    int idx = registerNativeFunction(signature, arg);

    // qDebug() << "the func id:" << idx;

    // 使用 JS_NewCFunctionMagic 搭配 JS_CFUNC_generic_magic
    // 可以实现使用同一个名字，注册多个函数？
    JSValue fn = JS_NewCFunctionMagic(m_ctx,
                                      nativeFunctionShim,
                                      "native",
                                      0,
                                      JS_CFUNC_generic_magic,
                                      idx);

    QScriptValue qVal = QScriptValue(m_ctx, fn, this);

    JS_FreeValue(m_ctx, fn);

    return qVal;
}

QScriptValue QScriptEngine::newVariant(const QVariant &value)
{
    if (!m_ctx)
        return QScriptValue();

    // 下面这些都得释放掉
    JSValue jVal = JS_UNDEFINED;

    if (value.type() == QVariant::String) {
        jVal = JS_NewString(m_ctx, value.toString().toUtf8().constData());
    }
    if (value.type() == QVariant::Bool) {
        jVal = JS_NewBool(m_ctx, value.toBool());
    }
    if (value.canConvert<double>()) {
        jVal = JS_NewFloat64(m_ctx, value.toDouble());
    }

    QScriptValue qVal = QScriptValue(m_ctx, jVal, this);

    if(!JS_IsUndefined(jVal))
    {
        JS_FreeValue(m_ctx, jVal);
    }

    return qVal;
}

QScriptValue QScriptEngine::newVariant(const QScriptValue &object, const QVariant &value)
{
    Q_UNUSED(object);
    return newVariant(value);
}

QScriptValue QScriptEngine::newQObject(QObject *object, QScriptEngine::ValueOwnership ownership, const QScriptEngine::QObjectWrapOptions &options)
{
    Q_UNUSED(options);
    if (!m_ctx || !object)
        return QScriptValue();

    JSValue jsObj = JS_NewObjectClass(m_ctx, m_qobjectClassId);
    if (JS_IsException(jsObj))
        return QScriptValue();

    QObjectWrapper *w = new QObjectWrapper{object, ownership};
    JS_SetOpaque(jsObj, w);

    QScriptValue qVal = QScriptValue(m_ctx, jsObj, this);

    JS_FreeValue(m_ctx, jsObj);

    return qVal;
}

QScriptValue QScriptEngine::newQObject(const QScriptValue &scriptObject, QObject *qtObject, QScriptEngine::ValueOwnership ownership, const QScriptEngine::QObjectWrapOptions &options)
{
    Q_UNUSED(options);
    if (!m_ctx || !qtObject)
        return QScriptValue();

    JSValue wrapper = JS_NewObjectClass(m_ctx, m_qobjectClassId);
    if (JS_IsException(wrapper))
        return QScriptValue();

    QObjectWrapper *w = new QObjectWrapper{qtObject, ownership};
    JS_SetOpaque(wrapper, w);

    if (scriptObject.isValid() && scriptObject.isObject()) {
        // attach the wrapper to the provided scriptObject as a property named "__qt__"
        JS_SetPropertyStr(m_ctx, scriptObject.rawValue(), "__qt__", JS_DupValue(m_ctx, wrapper));
    }

    QScriptValue qVal = QScriptValue(m_ctx, wrapper, this);

    JS_FreeValue(m_ctx, wrapper);

    return qVal;
}

int QScriptEngine::registerNativeFunction(FunctionWithArgSignature signature, void *arg)
{
    std::lock_guard<std::mutex> lk(m_nativeFunctionsMutex);
    NativeFunctionEntry e{signature, arg};
    m_nativeFunctions.push_back(e);
    return (int)m_nativeFunctions.size() - 1;
}

bool QScriptEngine::getNativeEntry(int idx, FunctionWithArgSignature &outFunc, void **outArg) const
{
    std::lock_guard<std::mutex> lk(m_nativeFunctionsMutex);
    if (idx < 0 || idx >= (int)m_nativeFunctions.size())
        return false;
    outFunc = m_nativeFunctions[idx].func;
    *outArg = m_nativeFunctions[idx].arg;
    return true;
}

QObject *QScriptEngine::qobjectFromJSValue(JSContext *ctx, JSValueConst val) const
{
    if (!ctx)
        return nullptr;
    void *p = JS_GetOpaque2(ctx, val, m_qobjectClassId);
    if (!p)
        return nullptr;
    QObjectWrapper *w = static_cast<QObjectWrapper*>(p);
    return w->obj;
}

bool QScriptEngine::hasUncaughtException() const
{
    if (!m_ctx)
        return false;
    return JS_HasException(m_ctx);
}

QScriptValue QScriptEngine::uncaughtException() const
{
    if (!m_ctx)
        return QScriptValue();

    JSValue exc = JS_GetException(m_ctx);

    QScriptValue qVal = QScriptValue(m_ctx, exc, const_cast<QScriptEngine*>(this));

    JS_FreeValue(m_ctx, exc);

    return qVal;
}

int QScriptEngine::uncaughtExceptionLineNumber() const
{
    Q_UNUSED(this);
    return -1;
}

QStringList QScriptEngine::uncaughtExceptionBacktrace() const
{
    return QStringList();
}

QScriptValue QScriptEngine::nullValue()
{
    if (!m_ctx) return QScriptValue();
    return QScriptValue(m_ctx, JS_NULL, this);
}

QScriptValue QScriptEngine::undefinedValue()
{
    if (!m_ctx) return QScriptValue();
    return QScriptValue(m_ctx, JS_UNDEFINED, this);
}

QScriptSyntaxCheckResult QScriptEngine::checkSyntax(const QString &program)
{
    return QScriptSyntaxCheckResult();

    // if (!m_ctx)
    //     return QScriptSyntaxCheckResult();

    // QByteArray ba = program.toUtf8();
    // const char *fn = "<check>";
    // int flags = JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY;

    // JSValue val = JS_Eval(m_ctx, ba.constData(), ba.size(), fn, flags);

    // if (!JS_IsException(val)) {
    //     JS_FreeValue(m_ctx, val);
    //     return QScriptSyntaxCheckResult();
    // }

    // JSValue exception = JS_GetException(m_ctx);

    // QString message;
    // int line = -1;
    // int column = -1;

    // JSValue msgProp = JS_GetPropertyStr(m_ctx, exception, "message");
    // if (!JS_IsUndefined(msgProp) && !JS_IsNull(msgProp)) {
    //     JSValue s = JS_ToString(m_ctx, msgProp);
    //     const char *c = JS_ToCString(m_ctx, s);
    //     if (c)
    //         message = QString::fromUtf8(c);
    //     JS_FreeCString(m_ctx, c);
    //     JS_FreeValue(m_ctx, s);
    // }

    // if (message.isEmpty()) {
    //     JSValue s = JS_ToString(m_ctx, exception);
    //     const char *c = JS_ToCString(m_ctx, s);
    //     if (c)
    //         message = QString::fromUtf8(c);
    //     JS_FreeCString(m_ctx, c);
    //     JS_FreeValue(m_ctx, s);
    // }

    // // Try to parse file:line:column from message
    // QString first = message.trimmed();
    // QString inside;
    // int lp = first.lastIndexOf('(');
    // int rp = first.lastIndexOf(')');
    // if (lp != -1 && rp > lp) {
    //     inside = first.mid(lp + 1, rp - lp - 1).trimmed();
    // } else {
    //     if (first.contains(" at ", Qt::CaseInsensitive)) {
    //         int at = first.lastIndexOf(" at ", -1, Qt::CaseInsensitive);
    //         if (at >= 0)
    //             inside = first.mid(at + 4).trimmed();
    //     } else {
    //         inside = first;
    //     }
    // }

    // QStringList parts = inside.split(':');
    // if (parts.size() >= 2) {
    //     bool ok1 = false;
    //     bool ok2 = false;
    //     column = parts.takeLast().toInt(&ok1);
    //     line = parts.takeLast().toInt(&ok2);
    //     if (!ok1) column = -1;
    //     if (!ok2) {
    //         if (ok1) { line = column; column = -1; }
    //         else line = -1;
    //     }
    // }

    // JS_FreeValue(m_ctx, msgProp);
    // JS_FreeValue(m_ctx, exception);
    // JS_FreeValue(m_ctx, val);

    // return QScriptSyntaxCheckResult(message, line, column);
}
