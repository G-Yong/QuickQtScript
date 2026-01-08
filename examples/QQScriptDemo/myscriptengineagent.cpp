#include "myscriptengineagent.h"
#include <QDebug>

#include <QScriptEngine>

MyScriptEngineAgent::MyScriptEngineAgent(QScriptEngine *engine) :
    QScriptEngineAgent(engine)
    , m_debugMode(NoDebug)
    , m_stepOverDepth(0)
    , m_stepOutDepth(0)
    , m_currentDepth(0)
    , m_currentScriptId(-1)
    , m_paused(false)
{
    qRegisterMetaType<PosInfo>("PosInfo");
}

MyScriptEngineAgent::~MyScriptEngineAgent()
{
    // 确保等待的线程被唤醒
    stopDebugging();
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
    QMutexLocker locker(&m_mutex);
    m_currentDepth++;
}

void MyScriptEngineAgent::functionExit(qint64 scriptId, const QScriptValue &returnValue)
{
    qDebug() << "function exit";
    QMutexLocker locker(&m_mutex);
    m_currentDepth--;
}

void MyScriptEngineAgent::positionChange(qint64 scriptId, int lineNumber, int columnNumber)
{
    qDebug() << "position changed:" << scriptId << lineNumber << columnNumber;
    mCurPos = PosInfo{mFileMap[scriptId], lineNumber, columnNumber};
    emit posChanged(mCurPos);

    // 更新当前位置
    {
        QMutexLocker locker(&m_mutex);
        m_currentScriptId     = scriptId;
    }

    // 检查是否需要暂停
    if (shouldPauseAtPosition(scriptId, lineNumber, columnNumber)) {
        waitForContinue();
    }
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

// ============ 调试控制实现 ============

void MyScriptEngineAgent::setDebugMode(DebugMode mode)
{
    QMutexLocker locker(&m_mutex);
    m_debugMode = mode;
}

MyScriptEngineAgent::DebugMode MyScriptEngineAgent::debugMode()
{
    QMutexLocker locker(&m_mutex);
    return m_debugMode;
}

void MyScriptEngineAgent::addBreakpoint(QString filePath, int lineNumber)
{
    QMutexLocker locker(&m_mutex);
    Breakpoint bp(filePath, lineNumber, true);
    if (!m_breakpoints.contains(bp)) {
        m_breakpoints.append(bp);
        qDebug() << "添加断点：file=" << filePath << "lineNumber=" << lineNumber;
    }
}

void MyScriptEngineAgent::removeBreakpoint(QString filePath, int lineNumber)
{
    QMutexLocker locker(&m_mutex);
    Breakpoint bp(filePath, lineNumber);
    m_breakpoints.removeAll(bp);
    qDebug() << "移除断点：scriptId=" << filePath << "lineNumber=" << lineNumber;
}

void MyScriptEngineAgent::clearBreakpoints()
{
    QMutexLocker locker(&m_mutex);
    m_breakpoints.clear();
    qDebug() << "清除所有断点";
}

bool MyScriptEngineAgent::hasBreakpoint(QString filePath, int lineNumber)
{
    QMutexLocker locker(&m_mutex);
    for (const Breakpoint& bp : m_breakpoints) {
        if (bp.filePath == filePath && bp.lineNumber == lineNumber && bp.enabled) {
            return true;
        }
    }
    return false;
}

void MyScriptEngineAgent::enableBreakpoint(QString filePath, int lineNumber, bool enabled)
{
    QMutexLocker locker(&m_mutex);
    for (Breakpoint& bp : m_breakpoints) {
        if (bp.filePath == filePath && bp.lineNumber == lineNumber) {
            bp.enabled = enabled;
            qDebug() << (enabled ? "启用" : "禁用") << "断点：scriptId=" << filePath << "lineNumber=" << lineNumber;
            break;
        }
    }
}

QList<MyScriptEngineAgent::Breakpoint> MyScriptEngineAgent::breakpoints()
{
    QMutexLocker locker(&m_mutex);
    return m_breakpoints;
}

void MyScriptEngineAgent::continueExecution()
{
    QMutexLocker locker(&m_mutex);
    m_debugMode = Continue;
    m_paused = false;
    m_waitCondition.wakeAll();
    qDebug() << "继续执行";
}

void MyScriptEngineAgent::stepInto()
{
    QMutexLocker locker(&m_mutex);
    m_debugMode = StepIn;
    m_paused = false;
    m_waitCondition.wakeAll();
    qDebug() << "单步进入";
}

void MyScriptEngineAgent::stepOver()
{
    QMutexLocker locker(&m_mutex);
    m_debugMode = StepOver;
    m_stepOverDepth = m_currentDepth;
    m_paused = false;
    m_waitCondition.wakeAll();
    qDebug() << "单步跳过，当前深度：" << m_stepOverDepth;
}

void MyScriptEngineAgent::stepOut()
{
    QMutexLocker locker(&m_mutex);
    m_debugMode = StepOut;
    m_stepOutDepth = m_currentDepth - 1;  // 跳出到上一层
    m_paused = false;
    m_waitCondition.wakeAll();
    qDebug() << "单步跳出，目标深度：" << m_stepOutDepth;
}

void MyScriptEngineAgent::pause()
{
    QMutexLocker locker(&m_mutex);
    m_debugMode = StepIn;  // 暂停后下一步就停
    qDebug() << "暂停执行";
}

void MyScriptEngineAgent::stopDebugging()
{
    setDebugMode(NoDebug);

    QMutexLocker locker(&m_mutex);
    m_paused = false;
    m_waitCondition.wakeAll();
}

int MyScriptEngineAgent::currentDepth()
{
    QMutexLocker locker(&m_mutex);
    return m_currentDepth;
}

bool MyScriptEngineAgent::shouldPauseAtPosition(qint64 scriptId, int lineNumber, int columnNumber)
{
    QMutexLocker locker(&m_mutex);

    // 不在调试模式，不暂停
    if (m_debugMode == NoDebug) {
        return false;
    }

    // 已经暂停中，不重复暂停
    if (m_paused) {
        return false;
    }

    // 检查断点
    bool hasBreakpointHere = false;
    for (const Breakpoint& bp : m_breakpoints) {
        if (bp.filePath == mFileMap[scriptId] && bp.lineNumber == lineNumber && bp.enabled) {
            hasBreakpointHere = true;
            break;
        }
    }

    // Continue模式：只在断点处暂停
    if (m_debugMode == Continue) {
        return hasBreakpointHere;
    }

    // StepIn模式：每行都暂停（或遇到断点）
    if (m_debugMode == StepIn) {
        return true;
    }

    // StepOver模式：在当前深度或更浅的深度暂停（或遇到断点）
    if (m_debugMode == StepOver) {
        return hasBreakpointHere || (m_currentDepth <= m_stepOverDepth);
    }

    // StepOut模式：在比开始深度更浅的深度暂停（或遇到断点）
    if (m_debugMode == StepOut) {
        return hasBreakpointHere || (m_currentDepth <= m_stepOutDepth);
    }

    return false;
}

void MyScriptEngineAgent::waitForContinue()
{
    QMutexLocker locker(&m_mutex);
    m_paused = true;

    // qDebug() << "脚本已暂停在：scriptId=" << m_currentScriptId
    //          << "depth=" << m_currentDepth;

    // 阻塞等待继续命令
    while (m_paused) {
        m_waitCondition.wait(&m_mutex);
    }
}

