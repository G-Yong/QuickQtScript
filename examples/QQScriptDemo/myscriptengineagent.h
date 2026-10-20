#ifndef MYSCRIPTENGINEAGENT_H
#define MYSCRIPTENGINEAGENT_H

#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QWaitCondition>

#include <QScriptEngineAgent>
#include <QScriptContext>

#pragma execution_character_set("utf-8")

struct PosInfo
{
    QString file;
    qint64  line   = -1;
    qint64  column = -1;
};

class MyScriptEngineAgent :public QObject,  public QScriptEngineAgent
{
    Q_OBJECT

public:
    // 调试模式枚举
    enum DebugMode {
        NoDebug,        // 不调试，正常运行
        StepIn,         // 单步进入
        StepOver,       // 单步跳过
        StepOut,        // 单步跳出
        Continue        // 继续运行到下一个断点
    };

    // 断点结构
    struct Breakpoint {
        QString filePath;
        int lineNumber;
        bool enabled;

        Breakpoint(QString path = "", int line = -1, bool en = true)
            : filePath(path), lineNumber(line), enabled(en) {}

        bool operator==(const Breakpoint& other) const {
            return filePath == other.filePath && lineNumber == other.lineNumber;
        }
    };

    MyScriptEngineAgent(QScriptEngine *engine);
    ~MyScriptEngineAgent();

    virtual void scriptLoad(qint64 id, const QString &program,
                            const QString &fileName, int baseLineNumber);
    virtual void scriptUnload(qint64 id);

    virtual void contextPush();
    virtual void contextPop();

    virtual void functionEntry(qint64 scriptId);
    virtual void functionExit(qint64 scriptId,
                              const QScriptValue &returnValue);

    virtual void positionChange(qint64 scriptId,
                                int lineNumber, int columnNumber);
    virtual void runToLineTargetReached(qint64 scriptId,
                                        int lineNumber, int columnNumber);

    virtual void exceptionThrow(qint64 scriptId,
                                const QScriptValue &exception,
                                bool hasHandler);
    virtual void exceptionCatch(qint64 scriptId,
                                const QScriptValue &exception);

    // virtual bool supportsExtension(Extension extension) const;
    // virtual QVariant extension(Extension extension,
    //                            const QVariant &argument = QVariant());


    PosInfo currentPos();

    // 调试控制接口
    void setDebugMode(DebugMode mode);
    DebugMode debugMode();

    // 断点管理
    void addBreakpoint(QString filePath, int lineNumber);
    void removeBreakpoint(QString filePath, int lineNumber);
    void clearBreakpoints();
    bool hasBreakpoint(QString filePath, int lineNumber);
    void enableBreakpoint(QString filePath, int lineNumber, bool enabled);
    QList<Breakpoint> breakpoints();

    // 调试控制
    void continueExecution();      // 继续执行
    void stepInto();               // 单步进入
    void stepOver();               // 单步跳过
    void stepOut();                // 单步跳出
    // 仅 Running -> Requested 转换成功时返回 true；再次点击停止可据此执行终止。
    bool requestPause();
    bool isPaused();
    void pauseCheckpoint() override;
    void stopDebugging();

    // 获取当前状态
    int currentDepth();

signals:
    void posChanged(PosInfo info);

private:
    enum class PauseState {
        Running,
        Requested,
        Paused
    };

    // 以下三个辅助函数都要求调用方已经持有 m_mutex。
    bool shouldPauseAtPositionLocked(qint64 scriptId, int lineNumber);
    void resumeExecutionLocked(DebugMode mode);
    void waitWhilePausedLocked();

private:
    PosInfo mCurPos;
    QMap<qint64, QString> mFileMap;

    // 调试状态
    DebugMode m_debugMode;
    int m_stepOverDepth;           // stepOver时记录的函数深度
    int m_stepOutDepth;            // stepOut时记录的函数深度
    int m_currentDepth;            // 当前函数调用深度

    // 断点管理
    QList<Breakpoint> m_breakpoints;

    // 线程同步
    QMutex m_mutex;
    QWaitCondition m_waitCondition;
    // 单一状态机保证暂停请求不会在 Paused 标志设置前被另一个线程误判。
    PauseState m_pauseState{PauseState::Running};

    // QuickJS 已验证目标函数和目标行；这里只桥接紧随其后的 positionChange。
    bool m_runToLineStopPending{false};

};

#endif // MYSCRIPTENGINEAGENT_H
