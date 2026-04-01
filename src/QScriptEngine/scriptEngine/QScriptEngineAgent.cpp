#include <QScriptEngineAgent.h>
#include <QScriptEngine.h>

#include <QDebug>
#include <QScriptContext>

QScriptEngineAgent::QScriptEngineAgent(QScriptEngine *engine)
    : m_engine(engine)
{
    mLastLine = -1;
    mLastCol = -1;

    // 设置 agent 引用，
    // QScriptEngine.cpp中的scriptDebugBreak 会通过 engine->agent()来调用 agent 的函数，所以必须设置引用
    engine->setAgent(this);
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
    // qDebug() << "function entry";
}

void QScriptEngineAgent::functionExit(qint64 scriptId, const QScriptValue &returnValue)
{
    // qDebug() << "function exit";
}

void QScriptEngineAgent::positionChange(qint64 scriptId, int lineNumber, int columnNumber)
{
    // qDebug() << "position changed:" << scriptId << lineNumber << columnNumber;
}

void QScriptEngineAgent::scriptLoad(qint64 id, const QString &program, const QString &fileName, int baseLineNumber)
{
    // qDebug() << "script load:" << id << fileName;
}

void QScriptEngineAgent::scriptUnload(qint64 id)
{
    // qDebug() << "script unload" << id;
}

bool QScriptEngineAgent::isPosChanged(qint64 line, qint64 col)
{
    bool flag = false;
    // if(mLastLine != line || mLastCol != col)
    // {
    //     flag = true;
    // }
    if(mLastLine != line)
    {
        flag = true;
    }

    mLastLine = line;
    mLastCol = col;

    return flag;
}

qint64 QScriptEngineAgent::scriptId(QString fileName)
{
    if(engine() == nullptr)
    {
        return -1;
    }

    return engine()->fileNameBuffer().indexOf(fileName);
}

void QScriptEngineAgent::checkFunctionPair(qint64 scriptId, QScriptValue value)
{
    if(mFuncStackCounter != 0)
    {
        functionExit(scriptId, value);
    }
    mFuncStackCounter = 0;
}

// bool QScriptEngineAgent::supportsExtension(QScriptEngineAgent::Extension extension) const
// {

// }

