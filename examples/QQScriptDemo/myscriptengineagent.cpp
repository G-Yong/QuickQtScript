#include "myscriptengineagent.h"
#include <QDebug>

#include <QScriptEngine>

MyScriptEngineAgent::MyScriptEngineAgent(QScriptEngine *engine) :
    QScriptEngineAgent(engine)
{
    qRegisterMetaType<PosInfo>("PosInfo");
}

void MyScriptEngineAgent::scriptLoad(qint64 id, const QString &program, const QString &fileName, int baseLineNumber)
{
    // qDebug() << "script load:" << id << program << fileName << baseLineNumber;

    mFileMap.insert(id, fileName);
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
    qDebug() << "function entry";
}

void MyScriptEngineAgent::functionExit(qint64 scriptId, const QScriptValue &returnValue)
{
    qDebug() << "function exit";
}

void MyScriptEngineAgent::positionChange(qint64 scriptId, int lineNumber, int columnNumber)
{
    qDebug() << "agent position changed:" << scriptId << lineNumber << columnNumber;

    mCurPos = PosInfo{mFileMap[scriptId], lineNumber, columnNumber};

    emit posChanged(mCurPos);
}

void MyScriptEngineAgent::exceptionThrow(qint64 scriptId, const QScriptValue &exception, bool hasHandler)
{
    auto curCtx = engine()->currentContext();
    qDebug() << curCtx->backtrace() << curCtx->argumentCount();
    qDebug() << "agent exception throw" << exception.toString();
}

void MyScriptEngineAgent::exceptionCatch(qint64 scriptId, const QScriptValue &exception)
{

}
