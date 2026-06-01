#include <QCoreApplication>
#include <QDebug>
#include <QScriptEngine>
#include <QScriptValue>
#include <QString>
#include <cstdio>

extern "C" {
#include "quickjs.h"
}

#ifdef Q_OS_WIN
#include <crtdbg.h>
#endif

#ifdef Q_OS_WIN
#pragma execution_character_set("utf-8")
#endif

/*
 * 测试职责说明：
 *
 * 1. 本工程只验证 QScriptEngine 封装层的内存安全边界，不覆盖原 accessor
 *    生命周期回归测试。accessor 的 getter/setter 析构路径保留在
 *    tests/qscriptengine_lifetime 中，避免两个测试的目的混在一起。
 *
 * 2. 这里覆盖三类容易在运行时变成泄漏、double free 或长期占用的问题：
 *    - setProperty() 写入新属性：JS_GetOwnProperty() 返回 false 时 descriptor 未填充，
 *      如果仍释放 descriptor 成员，可能释放未初始化 JSValue；
 *    - JS_SetProperty*() 失败分支：QuickJS 已经接管并释放传入的 value，如果封装层
 *      再释放一次，可能提前析构仍被其它持有者引用的对象；
 *    - 未处理 Promise rejection：诊断列表如果不设置上限，会长期持有 rejection reason。
 *
 * 3. 这些问题不一定表现为每次稳定崩溃。测试把 QuickJS 的接口契约写成可执行
 *    用例，目的是防止后续维护时重新引入错误释放或无限持有 JSValue 的逻辑。
 */
namespace {

constexpr int kMaxStoredPromiseRejections = 64;

enum class PropertyWriteKind
{
    Named,
    Indexed,
};

struct FinalizerCounter
{
    int count = 0;
};

struct OwnershipObservation
{
    bool setFailed = false;
    bool hadPendingException = false;
    int finalizersAfterSetFailure = 0;
    int finalizersAfterHandling = 0;
    int finalizersAfterOwnerRelease = 0;
};

JSClassID &trackedClassId()
{
    static JSClassID classId = 0;
    return classId;
}

void trackedValueFinalizer(JSRuntime *rt, JSValueConst value)
{
    Q_UNUSED(rt);

    FinalizerCounter *counter = static_cast<FinalizerCounter *>(JS_GetOpaque(value, trackedClassId()));
    if(counter)
    {
        ++counter->count;
    }
}

bool ensureTrackedValueClass(JSRuntime *rt)
{
    JSClassID &classId = trackedClassId();
    if(classId == 0)
    {
        JS_NewClassID(rt, &classId);
    }

    if(JS_IsRegisteredClass(rt, classId))
    {
        return true;
    }

    JSClassDef classDef = {};
    classDef.class_name = "TrackedValue";
    classDef.finalizer = trackedValueFinalizer;
    return JS_NewClass(rt, classId, &classDef) == 0;
}

bool takePendingException(JSContext *ctx)
{
    /*
     * QuickJS 的失败通常有两个信号：C API 返回 -1，同时把真正的异常对象暂存在
     * JSContext 里。JS_GetException() 会取走这个 pending exception，所以测试读取后
     * 必须立即释放，避免它影响后续脚本或探针。
     *
     * 对风险 2 来说，pending exception 证明 JS_SetProperty*() 确实进入了失败分支；
     * finalizer 计数则证明失败返回前传入的 value 已经被 QuickJS 接管并释放了一次。
     */
    JSValue exception = JS_GetException(ctx);
    const bool hadException = !JS_IsUndefined(exception);
    JS_FreeValue(ctx, exception);
    return hadException;
}

bool runCheck(const char *name, bool (*check)())
{
    /*
     * 各检查函数内部保留 qCritical() 的详细上下文。
     * 这里额外输出稳定的用例名，便于命令行或 CI 在 Qt 日志不可见时定位失败项。
     */
    const bool passed = check();
    if(!passed)
    {
        std::fprintf(stderr, "FAILED: %s\n", name);
    }

    return passed;
}

void printOwnershipObservation(const char *apiName,
                               const char *pathName,
                               const OwnershipObservation &observation)
{
    /*
     * 正常通过时也打印观察值，方便直接比较“正确释放路径”和“旧释放路径”。
     * pendingException 证明 JS_SetProperty*() 已经失败；finalizer 计数证明 value
     * 在不同释放路径下的生命周期是否被提前结束。
     */
    std::fprintf(stdout,
                 "%s %s: setFailed=%d pendingException=%d "
                 "finalizers(afterSetFailure=%d, afterHandling=%d, afterOwnerRelease=%d)\n",
                 apiName,
                 pathName,
                 observation.setFailed ? 1 : 0,
                 observation.hadPendingException ? 1 : 0,
                 observation.finalizersAfterSetFailure,
                 observation.finalizersAfterHandling,
                 observation.finalizersAfterOwnerRelease);
}

JSValue newTrackedValue(JSContext *ctx, FinalizerCounter *counter)
{
    JSValue value = JS_NewObjectClass(ctx, trackedClassId());
    if(JS_IsException(value))
    {
        return JS_EXCEPTION;
    }

    if(JS_SetOpaque(value, counter) < 0)
    {
        JS_FreeValue(ctx, value);
        return JS_EXCEPTION;
    }

    return value;
}

int setBlockedProperty(JSContext *ctx, JSValueConst target, PropertyWriteKind kind, JSValue value)
{
    if(kind == PropertyWriteKind::Indexed)
    {
        return JS_SetPropertyUint32(ctx, target, 0, value);
    }

    JSAtom atom = JS_NewAtom(ctx, "blocked");
    const int ret = JS_SetProperty(ctx, target, atom, value);
    JS_FreeAtom(ctx, atom);
    return ret;
}

bool observeSetPropertyFailure(PropertyWriteKind kind,
                               bool simulateOldExtraFree,
                               OwnershipObservation *observation)
{
    /*
     * 不可扩展目标对象能稳定制造 JS_SetProperty*() 失败。测试额外保留一份
     * retainedOwner 引用，使“旧代码失败后再次 JS_FreeValue(value)”不会依赖崩溃，
     * 而是表现为 tracked value 提前触发 finalizer。
     */
    QScriptEngine engine;
    JSContext *ctx = engine.ctx();
    if(!ensureTrackedValueClass(engine.runtime()))
    {
        qCritical() << "failed to register tracked value class";
        return false;
    }

    JSValue target = (kind == PropertyWriteKind::Indexed) ? JS_NewArray(ctx) : JS_NewObject(ctx);
    if(JS_IsException(target))
    {
        return false;
    }

    if(JS_PreventExtensions(ctx, target) < 0)
    {
        takePendingException(ctx);
        JS_FreeValue(ctx, target);
        return false;
    }

    FinalizerCounter counter;
    JSValue value = newTrackedValue(ctx, &counter);
    if(JS_IsException(value))
    {
        JS_FreeValue(ctx, target);
        return false;
    }

    JSValue retainedOwner = JS_DupValue(ctx, value);
    const int ret = setBlockedProperty(ctx, target, kind, value);

    observation->setFailed = ret < 0;
    observation->hadPendingException = takePendingException(ctx);
    observation->finalizersAfterSetFailure = counter.count;

    if(ret >= 0)
    {
        JS_FreeValue(ctx, retainedOwner);
        JS_FreeValue(ctx, target);
        observation->finalizersAfterOwnerRelease = counter.count;
        return true;
    }

    if(simulateOldExtraFree)
    {
        /*
         * 模拟修复前的错误：JS_SetProperty*() 失败时已经释放了传入的 value，
         * 这里再释放一次会消耗 retainedOwner 那份仍然有效的引用。此时对象提前
         * 触发 finalizer，说明旧写法破坏了其它持有者的生命周期。
         */
        JS_FreeValue(ctx, value);
        observation->finalizersAfterHandling = counter.count;

        // retainedOwner 已经被上面的错误释放消耗，不能再释放这个悬空值。
        JS_FreeValue(ctx, target);
        observation->finalizersAfterOwnerRelease = counter.count;
        return true;
    }

    /*
     * 修复后的正确行为：失败后只释放本函数仍然拥有的 target，不再碰 value。
     * tracked value 应该一直活到 retainedOwner 被显式释放。
     */
    observation->finalizersAfterHandling = counter.count;
    JS_FreeValue(ctx, retainedOwner);
    observation->finalizersAfterOwnerRelease = counter.count;
    JS_FreeValue(ctx, target);
    return true;
}

bool verifyNewPropertyKeepExistingFlags()
{
    /*
     * QScriptValue::setProperty() 默认使用 KeepExistingFlags。对不存在的属性，
     * QuickJS 的 JS_GetOwnProperty() 返回 false，且不会填充 descriptor。
     * 封装层只能在 ret > 0 时释放 descriptor 成员；否则释放未初始化的
     * JSValue 会变成未定义行为。
     */
    QScriptEngine engine;
    QScriptValue obj = engine.newObject();

    constexpr int propertyCount = 128;
    for(int i = 0; i < propertyCount; ++i)
    {
        const QString name = QStringLiteral("newProp_%1").arg(i);
        obj.setProperty(name, i);

        const int actual = obj.property(name).toInt32();
        if(actual != i)
        {
            qCritical() << "new property was not stored correctly:"
                        << name << "expected" << i << "actual" << actual;
            return false;
        }
    }

    return true;
}

bool verifySetPropertyFailureOwnsValue()
{
    /*
     * 本用例验证 QuickJS C API 的所有权契约，不验证 QScriptValueIterator 的真实
     * 生产路径。它的目的，是把风险 2 的底层差异打印出来，避免只看到最终 passed。
     *
     * QuickJS 规定：参数类型是 JSValue 的函数会接管该值，即使函数返回失败。
     * JS_SetProperty()/JS_SetPropertyUint32() 失败时会返回 -1，并在 JSContext 中留下
     * pending exception；同时，传入的 value 已经被释放了一次。
     *
     * 这个测试不用崩溃作为判断标准，而是用 tracked value 的 finalizer 次数证明：
     * - 修复后的写法不再碰 value，finalizer 只会在 retainedOwner 释放时发生；
     * - 修复前的写法失败后再次 JS_FreeValue(value)，会提前消耗 retainedOwner 的引用。
     */
    bool ok = true;

    const auto verifyKind = [&ok](PropertyWriteKind kind, const char *name) {
        OwnershipObservation fixedPath;
        OwnershipObservation oldPath;

        if(!observeSetPropertyFailure(kind, false, &fixedPath) ||
           !observeSetPropertyFailure(kind, true, &oldPath))
        {
            qCritical() << "failed to observe" << name << "ownership path";
            ok = false;
            return;
        }

        printOwnershipObservation(name, "fixed", fixedPath);
        printOwnershipObservation(name, "old-extra-free", oldPath);

        if(!fixedPath.setFailed || !fixedPath.hadPendingException ||
           fixedPath.finalizersAfterSetFailure != 0 ||
           fixedPath.finalizersAfterHandling != 0 ||
           fixedPath.finalizersAfterOwnerRelease != 1)
        {
            qCritical() << name << "fixed ownership path is unexpected"
                        << "setFailed" << fixedPath.setFailed
                        << "pendingException" << fixedPath.hadPendingException
                        << "afterFailure" << fixedPath.finalizersAfterSetFailure
                        << "afterHandling" << fixedPath.finalizersAfterHandling
                        << "afterOwnerRelease" << fixedPath.finalizersAfterOwnerRelease;
            ok = false;
        }

        if(!oldPath.setFailed || !oldPath.hadPendingException ||
           oldPath.finalizersAfterSetFailure != 0 ||
           oldPath.finalizersAfterHandling != 1)
        {
            qCritical() << name << "old ownership path did not show early finalizer"
                        << "setFailed" << oldPath.setFailed
                        << "pendingException" << oldPath.hadPendingException
                        << "afterFailure" << oldPath.finalizersAfterSetFailure
                        << "afterHandling" << oldPath.finalizersAfterHandling;
            ok = false;
        }
    };

    verifyKind(PropertyWriteKind::Named, "JS_SetProperty");
    verifyKind(PropertyWriteKind::Indexed, "JS_SetPropertyUint32");
    return ok;
}

bool verifyPromiseRejectionStorageLimit()
{
    /*
     * QScriptEngine 会保存未处理 Promise rejection，便于调用方诊断异步错误。
     * 后台或长期运行脚本不能无限保留这些 JSValue，否则每个 reason 都会延长
     * 对应 JS 对象的生命周期。测试生成超过上限的 rejection，验证只保留最近
     * 一批诊断信息。
     */
    QScriptEngine engine;
    QString script;
    constexpr int rejectionCount = kMaxStoredPromiseRejections + 16;
    for(int i = 0; i < rejectionCount; ++i)
    {
        script += QStringLiteral("Promise.reject(new Error('rejection-%1'));\n").arg(i);
    }
    script += QStringLiteral("void 0;\n");

    QScriptValue result = engine.evaluate(script);
    if(result.isError())
    {
        qCritical() << "creating unhandled promise rejections should not throw synchronously"
                    << result.toString();
        return false;
    }

    const int storedCount = engine.uncaughtPromiseRejections().size();
    if(storedCount != kMaxStoredPromiseRejections)
    {
        qCritical() << "unexpected promise rejection retention count:"
                    << "expected" << kMaxStoredPromiseRejections
                    << "actual" << storedCount;
        return false;
    }

    return true;
}

} // namespace

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    const int dbgFlags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    _CrtSetDbgFlag(dbgFlags | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
#endif

    QCoreApplication app(argc, argv);
    Q_UNUSED(app);

    bool ok = true;
    ok &= runCheck("verifyNewPropertyKeepExistingFlags", verifyNewPropertyKeepExistingFlags);
    ok &= runCheck("verifySetPropertyFailureOwnsValue", verifySetPropertyFailureOwnsValue);
    ok &= runCheck("verifyPromiseRejectionStorageLimit", verifyPromiseRejectionStorageLimit);

    if(!ok)
    {
        return 1;
    }

    qInfo() << "QScriptEngine memory safety test passed";
    return 0;
}
