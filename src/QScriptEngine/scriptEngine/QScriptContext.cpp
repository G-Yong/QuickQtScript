#include <QScriptContext>
#include <QScriptValue>
#include <QScriptEngine>
#include <QDebug>

extern "C" {
#include "quickjs.h"
}

QScriptContext::QScriptContext(JSContext *ctx,
                               JSValueConst this_val,
                               int argc,
                               JSValueConst *argv,
                               QScriptEngine *engine, JSValue callee)
    : m_ctx(ctx),
    m_this(JS_DupValue(ctx, this_val)),
    m_engine(engine),
    m_callee(callee)
{
    for (int i = 0; i < argc; ++i) {
        m_args.push_back(JS_DupValue(ctx, argv[i]));
    }
}

QScriptContext::~QScriptContext()
{
    if(m_ctx)
    {
        if (!JS_IsUndefined(m_this) && !JS_IsNull(m_this)) {
            JS_FreeValue(m_ctx, m_this);
        }

        if (!JS_IsUndefined(m_activation)) {
            JS_FreeValue(m_ctx, m_activation);
            m_activation = JS_UNDEFINED;
        }

        for (int i = 0; i < m_args.size(); ++i) {
            JS_FreeValue(m_ctx, m_args.at(i));
        }
    }
}

QScriptValue QScriptContext::activationObject() const
{
    if (!m_ctx)
        return QScriptValue();

    if (!JS_IsUndefined(m_activation))
    {
        auto value = JS_DupValue(m_ctx, m_activation);
        auto qVal  = QScriptValue(m_ctx, value, m_engine);

        JS_FreeValue(m_ctx, value);

        return qVal;
    }

    // fallback to thisObject
    return thisObject();
}

QScriptValue QScriptContext::argumentsObject() const
{
    if (!m_ctx)
        return QScriptValue();
    uint32_t n = (uint32_t)m_args.size();
    JSValue arr = JS_NewArray(m_ctx);
    for (uint32_t i = 0; i < n; ++i) {
        JS_SetPropertyUint32(m_ctx, arr, i, JS_DupValue(m_ctx, m_args[i]));
    }

    auto qVal = QScriptValue(m_ctx, arr, m_engine);

    JS_FreeValue(m_ctx, arr);

    return qVal;
}

QScriptValue QScriptContext::callee() const
{
    Q_UNUSED(this);

    return QScriptValue(m_ctx, m_callee, m_engine);
}

bool QScriptContext::isCalledAsConstructor() const
{
    return m_calledAsConstructor;
}

void QScriptContext::setCalledAsConstructor(bool called)
{
    m_calledAsConstructor = called;
}

QScriptContext *QScriptContext::parentContext() const
{
    Q_UNUSED(this);
    // Not implemented: QuickJS does not expose parent C stack frames via public API
    return nullptr;
}

void QScriptContext::setActivationObject(const QScriptValue &activation)
{
    if (!m_ctx)
        return;
    if (!JS_IsUndefined(m_activation)) {
        JS_FreeValue(m_ctx, m_activation);
        m_activation = JS_UNDEFINED;
    }
    if (activation.isValid()) {
        m_activation = JS_DupValue(m_ctx, activation.rawValue());
    }
}

void QScriptContext::setThisObject(const QScriptValue &thisObject)
{
    if (!m_ctx)
        return;
    if (!JS_IsUndefined(m_this) && !JS_IsNull(m_this)) {
        JS_FreeValue(m_ctx, m_this);
    }
    if (thisObject.isValid())
        m_this = JS_DupValue(m_ctx, thisObject.rawValue());
    else
        m_this = JS_UNDEFINED;
}

QScriptContext::ExecutionState QScriptContext::state() const
{
    // Minimal implementation: always NormalState. A richer implementation
    // would inspect the QuickJS stack frame for return/exception state.
    return QScriptContext::NormalState;
}

QScriptValue QScriptContext::argument(int index) const
{
    if (!m_ctx || index < 0 || index >= (int)m_args.size())
        return QScriptValue();

    // auto str = JS_ToCString(m_ctx, m_args[index]);
    // qDebug() << "argv:" << index << str << JS_IsObject(m_args[index]);
    // JS_FreeCString(m_ctx, str);

    return QScriptValue(m_ctx, m_args[index], m_engine);
}

int QScriptContext::argumentCount() const
{
    return (int)m_args.size();
}

QStringList QScriptContext::backtrace() const
{
    QStringList res;
    if (!m_ctx)
        return res;

    // Save and remove current exception from context
    JSValue saved_exception = JS_GetException(m_ctx);
    if (JS_IsUndefined(saved_exception) || JS_IsNull(saved_exception)) {
        // no exception, nothing to do
        if (!JS_IsUndefined(saved_exception))
            JS_Throw(m_ctx, saved_exception); // restore if non-null
        return res;
    }

    // Try to read 'stack' property from exception
    JSValue stack = JS_GetPropertyStr(m_ctx, saved_exception, "stack");
    if (!JS_IsUndefined(stack) && !JS_IsNull(stack)) {
        if (JS_IsString(stack)) {
            JSValue s = JS_ToString(m_ctx, stack);
            const char *c = JS_ToCString(m_ctx, s);
            if (c) {
                // split by newline
                QByteArray ba(c);
                JS_FreeCString(m_ctx, c);
                QStringList lines = QString::fromUtf8(ba).split('\n', Qt::SkipEmptyParts);
                for (QString &line : lines) {
                    QString t = line.trimmed();
                    if (!t.isEmpty())
                        res << t;
                }
            }
            JS_FreeValue(m_ctx, s);
        } else if (JS_IsArray(stack)) {
            int64_t len = 0;
            if (JS_GetLength(m_ctx, stack, &len) >= 0) {
                for (uint32_t i = 0; i < (uint32_t)len; ++i) {
                    JSValue el = JS_GetPropertyUint32(m_ctx, stack, i);
                    if (JS_IsString(el) || !JS_IsUndefined(el)) {
                        JSValue s = JS_ToString(m_ctx, el);
                        const char *c = JS_ToCString(m_ctx, s);
                        if (c) {
                            QString t = QString::fromUtf8(c).trimmed();
                            if (!t.isEmpty())
                                res << t;
                            JS_FreeCString(m_ctx, c);
                        }
                        JS_FreeValue(m_ctx, s);
                    }
                    JS_FreeValue(m_ctx, el);
                }
            }
        }
    }

    // free stack value
    if (!JS_IsUndefined(stack) && !JS_IsNull(stack))
        JS_FreeValue(m_ctx, stack);

    // restore exception to context so caller behavior is unchanged
    JS_Throw(m_ctx, saved_exception);

    return res;
}

QScriptEngine *QScriptContext::engine() const
{
    return m_engine;
}

QScriptValue QScriptContext::thisObject() const
{
    if (!m_ctx)
        return QScriptValue();

    return QScriptValue(m_ctx, m_this, m_engine);
}

QScriptValue QScriptContext::throwError(QScriptContext::Error error, const QString &text)
{
    if (!m_ctx)
        return QScriptValue();

    JSValue value = JS_UNDEFINED;

    switch (error) {
    case UnknownError:
    {
        value = JS_ThrowPlainError(m_ctx, "%s", text.toUtf8().constData());
    }; break;
    case ReferenceError:
    {
        value = JS_ThrowReferenceError(m_ctx, "%s", text.toUtf8().constData());
    }; break;
    case SyntaxError:
    {
        value = JS_ThrowSyntaxError(m_ctx, "%s", text.toUtf8().constData());
    }; break;
    case TypeError:
    {
        value = JS_ThrowTypeError(m_ctx, "%s", text.toUtf8().constData());
    }; break;
    case RangeError:
    {
        value = JS_ThrowRangeError(m_ctx, "%s", text.toUtf8().constData());
    }; break;
    default:
    {
        value = JS_Throw(m_ctx, JS_NewError(m_ctx));
    }; break;
    }

    QScriptValue qVal = QScriptValue(m_ctx, value, m_engine);

    JS_FreeValue(m_ctx, value);

    return qVal;
}

QScriptValue QScriptContext::throwError(const QString &text)
{
    if (!m_ctx)
        return QScriptValue();

    auto value = JS_ThrowPlainError(m_ctx, "%s", text.toUtf8().constData());

    auto qVal = QScriptValue(m_ctx, value, m_engine);

    JS_FreeValue(m_ctx, value);

    return qVal;
}

QScriptValue QScriptContext::throwValue(const QScriptValue &value)
{
    if (!m_ctx)
        return QScriptValue();

    auto val = JS_Throw(m_ctx, JS_DupValue(m_ctx, value.rawValue()));

    auto qVal = QScriptValue(m_ctx, val, m_engine);

    JS_FreeValue(m_ctx, val);

    return qVal;
}

QString QScriptContext::toString() const
{
    if (!m_ctx)
        return QString();
    JSValue s = JS_ToString(m_ctx, m_this);
    const char *c = JS_ToCString(m_ctx, s);
    QString res = QString::fromUtf8(c ? c : "");
    JS_FreeCString(m_ctx, c);
    JS_FreeValue(m_ctx, s);
    return res;
}
