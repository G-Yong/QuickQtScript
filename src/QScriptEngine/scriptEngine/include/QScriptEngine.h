#ifndef QSCRIPTENGINE_QSCRIPTENGINE_H
#define QSCRIPTENGINE_QSCRIPTENGINE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QMutex>
#include <QDebug>
#include <QSharedPointer>

#include <atomic>
#include <vector>
#include <mutex>
#ifdef QUICKQTSCRIPT_ENGINE_EXTENSIONS
#include <functional>
#endif // QUICKQTSCRIPT_ENGINE_EXTENSIONS

extern "C" {
#include "quickjs.h"
}

#include <QScriptValue>
#include <QScriptSyntaxCheckResult>
#include <QHash>

class QScriptEngineAgent;
class QScriptContext;
class QScriptClass;

class QScriptEngine : public QObject
{
    Q_OBJECT

public:
    enum ValueOwnership {
        QtOwnership,
        ScriptOwnership,
        AutoOwnership
    };

    enum QObjectWrapOption {
        ExcludeChildObjects = 0x0001,
        ExcludeSuperClassMethods = 0x0002,
        ExcludeSuperClassProperties = 0x0004,
        ExcludeSuperClassContents = 0x0006,
        SkipMethodsInEnumeration = 0x0008,
        ExcludeDeleteLater = 0x0010,
        ExcludeSlots = 0x0020,

        AutoCreateDynamicProperties = 0x0100,
        PreferExistingWrapperObject = 0x0200
    };
    Q_DECLARE_FLAGS(QObjectWrapOptions, QObjectWrapOption)
public:
    explicit QScriptEngine(QObject *parent = nullptr);
    ~QScriptEngine();

    bool isEvaluating() const;
    void abortEvaluation(const QScriptValue &result = QScriptValue());

    void setAgent(QScriptEngineAgent *agent);
    QScriptEngineAgent *agent() const;
    void collectGarbage();

    QScriptContext *currentContext() const;

    QScriptValue evaluate(const QString &program, const QString &fileName = QString(), int lineNumber = 1);

    QScriptValue globalObject() const;
    void setGlobalObject(const QScriptValue &object);

    QScriptValue newObject();
    QScriptValue newObject(QScriptClass *scriptClass, const QScriptValue &data = QScriptValue());
    QScriptValue newArray(uint length = 0);

    typedef QScriptValue (*FunctionSignature)(QScriptContext *, QScriptEngine *);
    QScriptValue newFunction(FunctionSignature signature, int length = 0);
    QScriptValue newFunction(FunctionSignature signature, const QScriptValue &prototype, int length = 0);

    typedef QScriptValue (*FunctionWithArgSignature)(QScriptContext *, QScriptEngine *, void *);
    QScriptValue newFunction(FunctionWithArgSignature signature, void *arg);
    QScriptValue newVariant(const QVariant &value);
    QScriptValue newVariant(const QScriptValue &object, const QVariant &value);

    QScriptValue newQObject(QObject *object, QScriptEngine::ValueOwnership ownership = QtOwnership, const QScriptEngine::QObjectWrapOptions &options = QObjectWrapOptions());
    QScriptValue newQObject(const QScriptValue &scriptObject, QObject *qtObject, QScriptEngine::ValueOwnership ownership = QtOwnership, const QScriptEngine::QObjectWrapOptions &options = QObjectWrapOptions());

    QScriptValue defaultPrototype(int metaTypeId) const;
    void setDefaultPrototype(int metaTypeId, const QScriptValue &prototype);

    bool hasUncaughtException() const;
    QScriptValue uncaughtException() const;
    int uncaughtExceptionLineNumber() const;
    QStringList uncaughtExceptionBacktrace() const;

    QScriptValue nullValue();
    QScriptValue undefinedValue();

    static QScriptSyntaxCheckResult checkSyntax(const QString &program);

    // template <typename T>
    // QScriptValue toScriptValue(const T &value) { Q_UNUSED(value); return QScriptValue(); }
    template<typename T>
    QScriptValue toScriptValue(const T &value) {
        QVariant var = QVariant::fromValue(value);
        // return QScriptValue(value);
        return toScriptValue(var);
    }

    // Convert a QVariant into a QScriptValue, applying default prototype
    QScriptValue toScriptValue(const QVariant &value);

public:
#ifdef QUICKQTSCRIPT_ENGINE_EXTENSIONS
    // 状态由QSharedPointer管理，调用方可使用heartbeatState().data()作为存活期内的实例键
    struct HeartbeatState {
        std::atomic<quint64> revision{0};
        std::atomic_bool paused{false};
    };

    // 执行心跳属于 QuickQtScript 扩展，用于监测字节码及原生等待期间的引擎活性
    quint64 executionRevision() const;
    void advanceExecutionRevision();
    void setExecutionPaused(bool paused);
    QSharedPointer<HeartbeatState> heartbeatState() const;

    // 执行检查预算由宿主按运行模式设置；0关闭，真实运动完成下发后可重置
    // QuickJS 每轮询一定数量的控制流字节码才调用一次中断处理器，此值不是精确字节码数
    void setExecutionCheckLimit(int checks);
    void resetExecutionCheckLimit();
    bool consumeExecutionCheck();
    bool executionCheckLimitExceeded() const;

    // 耗时 native 函数在安全的循环边界调用此接口，协作响应暂停请求。
    // 普通 JavaScript 语句由 OP_debug 回调暂停，不需要显式调用。
    void pauseCheckpoint();

    // 以下定位接口由 ScriptEngine.pri 开启，使用 Qt 自带 script 库时不参与类定义
    // 启动前解析 run-to-line 请求
    struct RunToLineInfo {
        bool enabled = false;
        int resolvedLine = 0;
        QString warningText;
    };
    RunToLineInfo resolveRunToLine(const QString &program,
                                   const QString &fileName,
                                   int requestedLine);
    void setRunToLineInfo(const RunToLineInfo &info);
    void clearRunToLineInfo();
    // 让多文件定义和入口调用共享同一轮定位状态
    struct ScriptSource { QString fileName; QString program; };
    QScriptValue evaluateWithEntry(const QList<ScriptSource> &sources,
                                  const QString &entryName, const QString &targetFileName);
    bool isFastForwarding() const;

    // 注册信息保存在原生入口上，脚本别名不能绕过宿主过滤策略
    struct NativeFunctionOptions {
        QString registeredName;             // C++ 注册时的稳定名称
        int policyTag = 0;                  // 由宿主解释的指令类别
        bool enterDuringRunToLine = false;  // 快进期是否先进入宿主过滤器
    };
    struct NativeCallInfo {
        NativeFunctionOptions options;
        QString fileName;
        int line = 0;
        bool directEntryFrame = false;
    };
    struct NativeCallDecision { bool execute = true; QScriptValue replacement; };
    using NativeCallFilter = std::function<NativeCallDecision(QScriptContext *, const NativeCallInfo &)>;
    QScriptValue newFunction(FunctionWithArgSignature signature, void *arg,
                             const NativeFunctionOptions &options);
    void setNativeCallFilter(NativeCallFilter filter);
    void setEntryFunction(const QScriptValue &entry);
    NativeCallDecision filterNativeCall(int index, QScriptContext *context);
    void setNativeCallPosition(const QString &fileName, int line);
    // 快进也更新源码位置，预算退出时可作为异常调用栈缺失时的提示依据
    QPair<QString, int> executionPosition() const { return {m_nativeFileName, m_nativeLine}; }

    // QuickJS 模块注册能力由当前实现提供，不属于 QtScript 的 QScriptEngine 接口
    // 模块导出项结构
    struct ModuleExport {
        QByteArray nameUtf8;
        QVariant value;

        // 导出项类型（用于构建 JSCFunctionListEntry）
        enum ExportType {
            Int32,
            Int64,
            Double,
            String,
            Object,      // 嵌套对象
            Function     // 自定义 函数
        };
        ExportType type;

        // 如果是函数导出，需要这些额外信息
        uint8_t length = 0;      // 函数参数个数
        JSCFunctionEnum cproto = JS_CFUNC_generic;
        QScriptValue scriptValue;

        // 构造函数：基本类型
        ModuleExport(const QString &n, int32_t val, ExportType t)
            : nameUtf8(n.toUtf8()), value(QVariant::fromValue(val)), type(t) {}
        ModuleExport(const QString &n, int64_t val, ExportType t)
            : nameUtf8(n.toUtf8()), value(QVariant::fromValue(val)), type(t) {}
        ModuleExport(const QString &n, double val, ExportType t)
            : nameUtf8(n.toUtf8()), value(QVariant::fromValue(val)), type(t) {}
        ModuleExport(const QString &n, const QString &val, ExportType t)
            : nameUtf8(n.toUtf8()), value(QVariant::fromValue(val)), type(t) {}
        ModuleExport(const QString &n, const QVariant &val, ExportType t)
            : nameUtf8(n.toUtf8()), value(val), type(t) {}

        // 构造函数：函数类型
        ModuleExport(const QString &n,  ExportType t,
                     const QScriptValue &func, uint8_t l = 0)
            : nameUtf8(n.toUtf8()), type(t), length(l), scriptValue(func) {}

    };
    void registerModule(const QString &moduleName, const QList<ModuleExport> &exports);
    QHash<QString, QList<ModuleExport>> m_moduleRegistry;   // c++ 模块列表
    QHash<QString, JSModuleDef*> m_loadedModules;     // 已加载模块缓存（防止重复求值）
    // 静态模块初始化回调
    static int moduleInitCallback(JSContext *ctx, JSModuleDef *m);

    void clearDefaultPrototypes();

    /* 以下接口仅供内部使用，请勿在类外或者子类中使用 */
    JSRuntime *runtime() const { return m_rt; }
    JSContext *ctx() const { return m_ctx; }

    // 由 scriptDebugTrace 在语句边界（OP_debug）调用，记录最近执行到的位置。
    // QuickJS 的异常对象本身不带行号，uncaughtExceptionLineNumber() 靠它实现。
    void setCurrentPosition(int line, int column)
    {
        m_currentLine = line;
        m_currentColumn = column;
    }

    // 中断标志，用于打断执行
    std::atomic_int interrupt_flag{0};

    QStringList fileNameBuffer(){
        return mFileNameBuffer;
    }

    // 获取所有未捕获的 Promise rejection 错误
    QList<QScriptValue> uncaughtPromiseRejections() const;
    // 是否有未捕获的异步错误
    bool hasUncaughtPromiseRejections() const {
        return !m_uncaughtPromiseRejections.isEmpty();
    }

    // 原生桥和 QObject 转换接口只供 QuickQtScript 自身实现使用
    bool getNativeEntry(int idx, FunctionWithArgSignature &outFunc, void **outArg, JSValue &callee) const;
    QObject *qobjectFromJSValue(JSContext *ctx, JSValueConst val) const;
    JSClassID qObjectClassId() const { return m_qobjectClassId; }
#endif // QUICKQTSCRIPT_ENGINE_EXTENSIONS

private:
    QScriptValue registerNativeFunction(FunctionWithArgSignature signature, void *arg, int length = 0, int cproto = JS_CFUNC_generic_magic);
#ifdef QUICKQTSCRIPT_ENGINE_EXTENSIONS
    static void promiseRejectionTracker(JSContext *ctx, JSValueConst promise,
                                        JSValueConst reason,
                                        bool is_handled, void *opaque); // 处理异步 reject 的回调函数
    // run-to-line 的真正执行入口：根脚本照常运行，只改变快进阶段的调试语义
    JSValue evaluateRunToLine(const QString &program,
                              const QString &fileName,
                              int lineNumber,
                              bool isModule,
                              const RunToLineInfo &info);
    mutable QList<QScriptValue> m_uncaughtPromiseRejections;  // 存储所有未处理的 reject
    void clearUncaughtPromiseRejections();  // 清空所有未处理的 reject
    JSValue awaitPromise(JSValue promise, bool isModule = false);  // 等待异步promise处理函数
#endif // QUICKQTSCRIPT_ENGINE_EXTENSIONS

private:
    JSRuntime *m_rt{nullptr};
    JSContext *m_ctx{nullptr};
    QScriptEngineAgent *m_agent{nullptr};
    int m_currentLine{-1};    // 最近执行的语句行号（1 基），-1 表示未知
    int m_currentColumn{-1};  // 最近执行的语句列号（1 基），-1 表示未知
    JSClassID m_qobjectClassId{0};
    std::atomic<int> m_evalCount{0};
    struct NativeFunctionEntry {
        FunctionWithArgSignature func;
        void *arg;
        JSValue callee;
#ifdef QUICKQTSCRIPT_ENGINE_EXTENSIONS
        NativeFunctionOptions options;
#endif // QUICKQTSCRIPT_ENGINE_EXTENSIONS
    };
    std::vector<NativeFunctionEntry> m_nativeFunctions;
    mutable std::mutex m_nativeFunctionsMutex;

    // 为engienAgent提供scriptID;
    QStringList mFileNameBuffer;
    QScriptContext *mCurCtx{nullptr};     // 还未实现
    QScriptValue *mGlobalObject{nullptr};
    QHash<int, QScriptValue> m_defaultPrototypes;
#ifdef QUICKQTSCRIPT_ENGINE_EXTENSIONS
    QSharedPointer<HeartbeatState> m_heartbeatState;
    int m_executionCheckLimit = 0;
    int m_executionChecksRemaining = 0;
    bool m_executionCheckLimitExceeded = false;
    // 只对下一次 evaluate 生效，调用后立即清空
    RunToLineInfo m_runToLineInfo;
    NativeCallFilter m_nativeCallFilter;
    QScriptValue m_entryFunction;
    QString m_nativeFileName;
    int m_nativeLine = 0;
#endif // QUICKQTSCRIPT_ENGINE_EXTENSIONS
};


Q_DECLARE_OPERATORS_FOR_FLAGS(QScriptEngine::QObjectWrapOptions)

#endif // QSCRIPTENGINE_QSCRIPTENGINE_H
