#include <QScriptEngineAgent.h>
#include <QScriptEngine.h>

#include <QDebug>
#include <QScriptContext>

int scriptOPChanged(uint8_t op,
                    const char *fileName,
                    const char *funcName,
                    int line,
                    int col,
                    void *userData
                    )
{
    QScriptEngineAgent *agent = static_cast<QScriptEngineAgent*>(userData);
    if (!agent)
        return -1;

    // qDebug() << OP_call << OP_return;

    // qDebug() << "op changed:"
    //          << (QJDefines::OPCodeEnum)op
    //          << fileName
    //          << funcName
    //          << line
    //          << col;

    int scriptId = agent->scriptId(fileName);

    if(1)
    {
        // 不能每次op变动都调用一次，要行列号变化才调用
        if(agent->isPosChanged(line, col))
        {
            // qDebug() << "script id:" << scriptId << fileName << line << col;
            agent->positionChange(scriptId, line, col);
        }
    }

    // 使用函数名变化来检测函数进入/退出
    // 但是在执行自定义c++函数，不会有函数名的变更。因此对于自定义的函数，需要在QScriptEngine中处理

    // 进入函数
    QString currentFuncName = funcName ? QString(funcName) : QString();
    if(currentFuncName.isEmpty() == false && currentFuncName != "<eval>")
    {
        QList<QString> &funcStack = agent->mFuncStack;
        if(funcStack.length() == 0)
        {
            funcStack << currentFuncName;
            agent->functionEntry(scriptId);
        }
        else if(funcStack.last() != currentFuncName)
        {
            funcStack << currentFuncName;
            agent->functionEntry(scriptId);
        }
    }

    switch(op)
    {
    case QJDefines::OP_return_undef:
    case QJDefines::OP_return_async:
    case QJDefines::OP_return:{
        QList<QString> &funcStack = agent->mFuncStack;
        if(funcStack.length() > 0)
        {
            agent->functionExit(scriptId, QScriptValue());
        }
    };break;
    }

    return 0;
}

QScriptEngineAgent::QScriptEngineAgent(QScriptEngine *engine)
    : m_engine(engine)
{
    mLastLine = -1;
    mLastCol = -1;

    JS_SetOPChangedHandler(engine->ctx(), scriptOPChanged, this);
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
    qDebug() << "function entry";
}

void QScriptEngineAgent::functionExit(qint64 scriptId, const QScriptValue &returnValue)
{
    qDebug() << "function exit";
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
    if(mLastLine != line || mLastCol != col)
    {
        flag = true;
    }
    // if(mLastLine != line)
    // {
    //     flag = true;
    // }

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

// bool QScriptEngineAgent::supportsExtension(QScriptEngineAgent::Extension extension) const
// {

// }

