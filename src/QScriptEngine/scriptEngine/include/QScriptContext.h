#ifndef QSCRIPTENGINE_QSCRIPTCONTEXT_H
#define QSCRIPTENGINE_QSCRIPTCONTEXT_H

#include <QString>
#include <QStringList>
#include <vector>

extern "C" {
#include "quickjs.h"
}

class QScriptEngine;
class QScriptValue;

class QScriptContext
{
public:
    enum ExecutionState {
        NormalState,
        ExceptionState
    };

    enum Error {
        UnknownError,
        ReferenceError,
        SyntaxError,
        TypeError,
        RangeError,
        URIError
    };

    QScriptContext(JSContext *ctx,
                   JSValueConst this_val,
                   int argc,
                   JSValueConst *argv,
                   QScriptEngine *engine,
                   JSValueConst callee);
    ~QScriptContext();

    // 禁止拷贝构造函数以及拷贝赋值运算符
    QScriptContext(const QScriptContext&) = delete;
    QScriptContext& operator=(const QScriptContext&) = delete;

    QScriptValue activationObject() const;
    QScriptValue argumentsObject() const;
    QScriptValue callee() const;
    bool isCalledAsConstructor() const;
    void setCalledAsConstructor(bool called);
    QScriptContext *parentContext() const;
    void setActivationObject(const QScriptValue &activation);
    void setThisObject(const QScriptValue &thisObject);


    QScriptContext::ExecutionState state() const;

    QScriptValue argument(int index) const;
    int argumentCount() const;
    QStringList backtrace() const;
    QScriptEngine *engine() const;
    QScriptValue thisObject() const;
    QScriptValue throwError(QScriptContext::Error error, const QString &text);
    QScriptValue throwError(const QString &text);
    QScriptValue throwValue(const QScriptValue &value);
    QString toString() const;

private:
    JSContext *m_ctx{nullptr};
    JSValue m_this{JS_UNDEFINED};
    JSValue m_activation{JS_UNDEFINED};
    std::vector<JSValue> m_args;
    QScriptEngine *m_engine{nullptr};
    JSValue m_callee;
    bool m_calledAsConstructor{false};
};

#endif // QSCRIPTENGINE_QSCRIPTCONTEXT_H
