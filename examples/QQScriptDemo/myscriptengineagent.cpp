#include "myscriptengineagent.h"
#include <QDebug>

#include <QScriptEngine>

MyScriptEngineAgent::MyScriptEngineAgent(QScriptEngine *engine) :
    QScriptEngineAgent(engine)
    , m_debugMode(NoDebug)
    , m_stepOverDepth(0)
    , m_stepOutDepth(0)
    , m_currentDepth(0)
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

    QMutexLocker locker(&m_mutex);
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
    PosInfo current;
    bool shouldWait = false;
    {
        QMutexLocker locker(&m_mutex);
        mCurPos = PosInfo{mFileMap.value(scriptId), lineNumber, columnNumber};
        current = mCurPos;

        shouldWait = shouldPauseAtPositionLocked(scriptId, lineNumber);
        if (shouldWait)
            m_pauseState = PauseState::Paused;
    }

    /* 先提交 Paused 状态，再向 UI 报告位置。这样 UI 收到信号后立即点击
       “继续”也不会丢失唤醒；如果继续发生在重新加锁前，下面的 while 会
       看到 Running 并直接返回。 */
    emit posChanged(current);

    if (shouldWait) {
        QMutexLocker locker(&m_mutex);
        waitWhilePausedLocked();
    }
}

void MyScriptEngineAgent::runToLineTargetReached(qint64 scriptId, int lineNumber, int columnNumber)
{
    Q_UNUSED(scriptId)
    Q_UNUSED(lineNumber)
    Q_UNUSED(columnNumber)

    QMutexLocker locker(&m_mutex);
    /* QuickJS 在同一个 OP_debug 回调中先发出本通知，随后立刻调用
       positionChange。无需在 agent 中再次保存或比较目标行。 */
    m_runToLineStopPending = true;
}

PosInfo MyScriptEngineAgent::currentPos()
{
    QMutexLocker locker(&m_mutex);
    return mCurPos;
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
    resumeExecutionLocked(Continue);
    qDebug() << "继续执行";
}

void MyScriptEngineAgent::stepInto()
{
    QMutexLocker locker(&m_mutex);
    resumeExecutionLocked(StepIn);
    qDebug() << "单步进入";
}

void MyScriptEngineAgent::stepOver()
{
    QMutexLocker locker(&m_mutex);
    m_stepOverDepth = m_currentDepth;
    resumeExecutionLocked(StepOver);
    qDebug() << "单步跳过，当前深度：" << m_stepOverDepth;
}

void MyScriptEngineAgent::stepOut()
{
    QMutexLocker locker(&m_mutex);
    m_stepOutDepth = m_currentDepth - 1;  // 跳出到上一层
    resumeExecutionLocked(StepOut);
    qDebug() << "单步跳出，目标深度：" << m_stepOutDepth;
}

bool MyScriptEngineAgent::requestPause()
{
    QMutexLocker locker(&m_mutex);
    if (m_pauseState != PauseState::Running)
        return false;

    /* 不改变调试模式。暂停请求本身优先于断点/单步判断，恢复时再由
       continueExecution 或各 step 接口明确选择后续行为。 */
    m_pauseState = PauseState::Requested;
    qDebug() << "请求暂停执行";
    return true;
}

bool MyScriptEngineAgent::isPaused()
{
    QMutexLocker locker(&m_mutex);
    return m_pauseState == PauseState::Paused;
}

void MyScriptEngineAgent::pauseCheckpoint()
{
    QMutexLocker locker(&m_mutex);
    if (m_pauseState != PauseState::Requested)
        return;

    /* native 代码只能在自己声明安全的位置协作暂停。保持当前 C++ 调用栈
       不退出，恢复后会从 checkpoint 后的下一条 native 语句继续。 */
    m_pauseState = PauseState::Paused;
    waitWhilePausedLocked();
}

void MyScriptEngineAgent::stopDebugging()
{
    QMutexLocker locker(&m_mutex);
    m_debugMode = NoDebug;
    m_pauseState = PauseState::Running;
    m_runToLineStopPending = false;
    m_waitCondition.wakeAll();
}

int MyScriptEngineAgent::currentDepth()
{
    QMutexLocker locker(&m_mutex);
    return m_currentDepth;
}

bool MyScriptEngineAgent::shouldPauseAtPositionLocked(qint64 scriptId, int lineNumber)
{
    // 用户暂停请求优先于 run-to-line、断点和单步模式。
    if (m_pauseState == PauseState::Requested) {
        // 两个原因落在同一语句时，一次暂停即可同时满足，避免恢复后立即再停。
        m_runToLineStopPending = false;
        return true;
    }

    // 目标回调已由 QuickJS 校验函数帧和目标行，只消费一次。
    if (m_runToLineStopPending) {
        m_runToLineStopPending = false;
        return true;
    }

    // 不在调试模式，不暂停
    if (m_debugMode == NoDebug) {
        return false;
    }

    // positionChange 正常不会在 Paused 时重入，保留检查用于防御异常调用。
    if (m_pauseState == PauseState::Paused) {
        return false;
    }

    // 检查断点
    bool hasBreakpointHere = false;
    for (const Breakpoint& bp : m_breakpoints) {
        if (bp.filePath == mFileMap.value(scriptId) && bp.lineNumber == lineNumber && bp.enabled) {
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

void MyScriptEngineAgent::resumeExecutionLocked(DebugMode mode)
{
    m_debugMode = mode;
    m_pauseState = PauseState::Running;
    m_waitCondition.wakeAll();
}

void MyScriptEngineAgent::waitWhilePausedLocked()
{
    /* 使用 while 而不是 if，以正确处理 QWaitCondition 的伪唤醒。
       调用方持有 m_mutex；wait 会原子释放并在返回前重新获得它。 */
    while (m_pauseState == PauseState::Paused)
        m_waitCondition.wait(&m_mutex);
}

