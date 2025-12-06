#include "QScriptEngineAgent.h"
#include "QScriptEngine.h"

QScriptEngineAgent::QScriptEngineAgent(QScriptEngine *engine)
    : m_engine(engine)
{
}

QScriptEngineAgent::~QScriptEngineAgent()
{
}

void QScriptEngineAgent::contextPop()
{

}

void QScriptEngineAgent::contextPush()
{

}

QScriptEngine *QScriptEngineAgent::engine() const
{
    return m_engine;
}

void QScriptEngineAgent::exceptionCatch(qint64 scriptId, const QScriptValue &exception)
{

}

void QScriptEngineAgent::exceptionThrow(qint64 scriptId, const QScriptValue &exception, bool hasHandler)
{

}

// QVariant QScriptEngineAgent::extension(QScriptEngineAgent::Extension extension, const QVariant &argument)
// {

// }

void QScriptEngineAgent::functionEntry(qint64 scriptId)
{

}

void QScriptEngineAgent::functionExit(qint64 scriptId, const QScriptValue &returnValue)
{

}

void QScriptEngineAgent::positionChange(qint64 scriptId, int lineNumber, int columnNumber)
{

}

void QScriptEngineAgent::scriptLoad(qint64 id, const QString &program, const QString &fileName, int baseLineNumber)
{

}

void QScriptEngineAgent::scriptUnload(qint64 id)
{

}

// bool QScriptEngineAgent::supportsExtension(QScriptEngineAgent::Extension extension) const
// {

// }
