#ifndef QSCRIPTENGINE_QSCRIPTENGINE_H
#define QSCRIPTENGINE_QSCRIPTENGINE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QMutex>
#include <QDebug>

#include <atomic>
#include <vector>
#include <mutex>

extern "C" {
#include "quickjs.h"
}

#include <QScriptValue>
#include <QScriptSyntaxCheckResult>

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
    // 未实现
    QScriptValue newFunction(FunctionSignature signature, const QScriptValue &prototype, int length = 0);

    typedef QScriptValue (*FunctionWithArgSignature)(QScriptContext *, QScriptEngine *, void *);
    QScriptValue newFunction(FunctionWithArgSignature signature, void *arg);

    QScriptValue newVariant(const QVariant &value);
    QScriptValue newVariant(const QScriptValue &object, const QVariant &value);

    // 目前这两个函数还存在重大隐患，暂时不建议使用
    QScriptValue newQObject(QObject *object, QScriptEngine::ValueOwnership ownership = QtOwnership, const QScriptEngine::QObjectWrapOptions &options = QObjectWrapOptions());
    QScriptValue newQObject(const QScriptValue &scriptObject, QObject *qtObject, QScriptEngine::ValueOwnership ownership = QtOwnership, const QScriptEngine::QObjectWrapOptions &options = QObjectWrapOptions());

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
        return QScriptValue(var);
    }


    /* 以下接口仅供内部使用，请勿在类外或者子类中使用 */
    JSRuntime *runtime() const { return m_rt; }
    JSContext *ctx() const { return m_ctx; }

    // 中断标志，用于打断执行
    std::atomic_int interrupt_flag = 0;

    QStringList fileNameBuffer(){
        return mFileNameBuffer;
    }

public:
    bool getNativeEntry(int idx,
                        FunctionSignature &outSign1,
                        FunctionWithArgSignature &outSign2,
                        void **outArg,
                        JSValue &callee) const;
    QObject *qobjectFromJSValue(JSContext *ctx, JSValueConst val) const;
    JSClassID qObjectClassId() const { return m_qobjectClassId; }

private:
    QScriptValue registerNativeFunction(FunctionWithArgSignature signature, void *arg, int length = 0, int cproto = JS_CFUNC_generic_magic);

private:
    JSRuntime *m_rt{nullptr};
    JSContext *m_ctx{nullptr};
    QScriptEngineAgent *m_agent{nullptr};
    JSClassID m_qobjectClassId{0};
    std::atomic<int> m_evalCount{0};
    struct NativeFunctionEntry {
        FunctionSignature        sign1 = nullptr;
        FunctionWithArgSignature sign2 = nullptr;
        void *arg = nullptr;
        JSValue callee;
    };
    std::vector<NativeFunctionEntry> m_nativeFunctions;
    mutable std::mutex m_nativeFunctionsMutex;

    // 为engienAgent提供scriptID;
    QStringList mFileNameBuffer;
    // 还未实现
    QScriptContext *mCurCtx{nullptr};
    QScriptValue *mGlobalObject{nullptr};
};


Q_DECLARE_OPERATORS_FOR_FLAGS(QScriptEngine::QObjectWrapOptions)

#endif // QSCRIPTENGINE_QSCRIPTENGINE_H
