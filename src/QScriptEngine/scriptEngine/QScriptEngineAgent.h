#pragma once

#include <QString>

class QScriptEngine;
class QScriptContext;
class QScriptValue;

class QScriptEngineAgent
{
public:

    explicit QScriptEngineAgent(QScriptEngine *engine = nullptr);
    virtual ~QScriptEngineAgent();

    virtual void contextPop();
    virtual void contextPush();
    QScriptEngine *engine() const;
    virtual void exceptionCatch(qint64 scriptId, const QScriptValue &exception);
    virtual void exceptionThrow(qint64 scriptId, const QScriptValue &exception, bool hasHandler);
    // virtual QVariant extension(QScriptEngineAgent::Extension extension, const QVariant &argument = QVariant());
    virtual void functionEntry(qint64 scriptId);
    virtual void functionExit(qint64 scriptId, const QScriptValue &returnValue);
    virtual void positionChange(qint64 scriptId, int lineNumber, int columnNumber);
    virtual void scriptLoad(qint64 id, const QString &program, const QString &fileName, int baseLineNumber);
    virtual void scriptUnload(qint64 id);
    // virtual bool supportsExtension(QScriptEngineAgent::Extension extension) const;

private:
    QScriptEngine *m_engine{nullptr};
};
