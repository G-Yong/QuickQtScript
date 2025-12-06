#include "myscriptengineagent.h"
#include <QDebug>


MyScriptEngineAgent::MyScriptEngineAgent(QScriptEngine *engine) :
    QScriptEngineAgent(engine)
{

}

void MyScriptEngineAgent::scriptLoad(qint64 id, const QString &program, const QString &fileName, int baseLineNumber)
{
    // qDebug() << "script load:" << id << program << fileName << baseLineNumber;
}

void MyScriptEngineAgent::scriptUnload(qint64 id)
{

}

void MyScriptEngineAgent::contextPush()
{

}

void MyScriptEngineAgent::contextPop()
{

}

void MyScriptEngineAgent::functionEntry(qint64 scriptId)
{

}

void MyScriptEngineAgent::functionExit(qint64 scriptId, const QScriptValue &returnValue)
{

}

void MyScriptEngineAgent::positionChange(qint64 scriptId, int lineNumber, int columnNumber)
{
    qDebug() << "position changed:" << scriptId << lineNumber << columnNumber;
}

void MyScriptEngineAgent::exceptionThrow(qint64 scriptId, const QScriptValue &exception, bool hasHandler)
{

}

void MyScriptEngineAgent::exceptionCatch(qint64 scriptId, const QScriptValue &exception)
{

}
