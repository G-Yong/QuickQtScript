#include <QScriptEngine>
#include <QScriptValue>
#include <QScriptContext>
#include <QScriptEngineAgent>
#include <QMetaProperty>

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>

#include <mutex>
#include <vector>
#include <atomic>

extern "C" {
#include "quickjs.h"
}

// 辅助函数：创建 C 函数导出项
inline JSCFunctionListEntry JS_CFUNC_DEF_CPP(const char* name, uint8_t length, JSCFunction* func1) {
    JSCFunctionListEntry entry = {};
    entry.name = name;
    entry.prop_flags = JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE;
    entry.def_type = JS_DEF_CFUNC;
    entry.magic = 0;
    entry.u.func.length = length;
    entry.u.func.cproto = JS_CFUNC_generic;
    entry.u.func.cfunc.generic = func1;
    return entry;
}

// 辅助函数：创建 int32 属性导出项
inline JSCFunctionListEntry JS_PROP_INT32_DEF_CPP(const char* name, int32_t val, uint8_t prop_flags) {
    JSCFunctionListEntry entry = {};
    entry.name = name;
    entry.prop_flags = prop_flags;
    entry.def_type = JS_DEF_PROP_INT32;
    entry.magic = 0;
    entry.u.i32 = val;
    return entry;
}

// 辅助函数：创建 int64 属性导出项
inline JSCFunctionListEntry JS_PROP_INT64_DEF_CPP(const char* name, int64_t val, uint8_t prop_flags) {
    JSCFunctionListEntry entry = {};
    entry.name = name;
    entry.prop_flags = prop_flags;
    entry.def_type = JS_DEF_PROP_INT64;
    entry.magic = 0;
    entry.u.i64 = val;
    return entry;
}

// 辅助函数：创建 double 属性导出项
inline JSCFunctionListEntry JS_PROP_DOUBLE_DEF_CPP(const char* name, double val, uint8_t prop_flags) {
    JSCFunctionListEntry entry = {};
    entry.name = name;
    entry.prop_flags = prop_flags;
    entry.def_type = JS_DEF_PROP_DOUBLE;
    entry.magic = 0;
    entry.u.f64 = val;
    return entry;
}

// 辅助函数：创建字符串属性导出项
inline JSCFunctionListEntry JS_PROP_STRING_DEF_CPP(const char* name, const char* cstr, uint8_t prop_flags) {
    JSCFunctionListEntry entry = {};
    entry.name = name;
    entry.prop_flags = prop_flags;
    entry.def_type = JS_DEF_PROP_STRING;
    entry.magic = 0;
    entry.u.str = cstr;
    return entry;
}

// 辅助函数：创建对象导出项
inline JSCFunctionListEntry JS_OBJECT_DEF_CPP(const char* name, const JSCFunctionListEntry* tab, int len, uint8_t prop_flags) {
    JSCFunctionListEntry entry = {};
    entry.name = name;
    entry.prop_flags = prop_flags;
    entry.def_type = JS_DEF_OBJECT;
    entry.magic = 0;
    entry.u.prop_list.tab = tab;
    entry.u.prop_list.len = len;
    return entry;
}

// 读取本地模块加载函数
static JSModuleDef *js_module_loader_qt(JSContext *ctx,
                                        const char *module_name,
                                        void *opaque) {
    QScriptEngine *engine = static_cast<QScriptEngine*>(opaque);
    if (!engine) return nullptr;

    QString moduleName(module_name);

    // 检查是否是已注册的 C++ 模块
    if (engine->m_moduleRegistry.contains(moduleName)) {
        // 1. 先创建模块
        JSModuleDef *m = JS_NewCModule(ctx, module_name, QScriptEngine::moduleInitCallback);
        if (!m) return nullptr;

        // 2. 动态构建导出声明列表
        auto exports = engine->m_moduleRegistry.value(moduleName);
        std::vector<JSCFunctionListEntry> entries;
        entries.reserve(exports.size());

        for (const auto &exp : exports) {
            const char *name = exp.nameUtf8.constData();

            // 根据类型构建对应的导出项
            switch (exp.type) {
            case QScriptEngine::ModuleExport::Int32:
                entries.push_back(JS_PROP_INT32_DEF_CPP(
                    name,
                    exp.value.toInt(),
                    JS_PROP_C_W_E  // 可写、可配置、可枚举
                    ));
                break;

            case QScriptEngine::ModuleExport::Int64:
                entries.push_back(JS_PROP_INT64_DEF_CPP(
                    name,
                    exp.value.toLongLong(),
                    JS_PROP_C_W_E
                    ));
                break;

            case QScriptEngine::ModuleExport::Double:
                entries.push_back(JS_PROP_DOUBLE_DEF_CPP(
                    name,
                    exp.value.toDouble(),
                    JS_PROP_C_W_E
                    ));
                break;

            case QScriptEngine::ModuleExport::String:
                entries.push_back(JS_PROP_STRING_DEF_CPP(
                    name,
                    exp.value.toString().toUtf8().constData(),
                    JS_PROP_C_W_E
                    ));
                break;

            case QScriptEngine::ModuleExport::Object:
                // 对象类型在 moduleInitCallback 中动态设置
                // 这里只需声明为普通变量
                entries.push_back(JS_PROP_INT32_DEF_CPP(name, 0, JS_PROP_C_W_E)); // 占位
                break;

            case QScriptEngine::ModuleExport::Function:
                // 函数类型需要特别注意
                if (exp.scriptValue.isValid() && exp.scriptValue.isFunction()) {
                    // QScriptValue 函数 - 添加占位符，将在 init 回调中设置实际值
                    entries.push_back(JS_PROP_INT32_DEF_CPP(name, 0, JS_PROP_C_W_E));
                }
                break;
            }
        }


        if (!entries.empty()) {
            JS_AddModuleExportList(ctx, m, entries.data(), entries.size());
        }

        return m;
    }

    // 默认路径为main.cpp的目录
    moduleName = "../../" + moduleName;
    // 处理文件路径
    QString filename = QDir::current().filePath(moduleName);

    // 检查文件是否存在
    QFileInfo fileInfo(filename);
    if (!fileInfo.exists()) {
        qDebug() << "Module file not found:" << filename;
        JS_ThrowReferenceError(ctx, "Cannot find module: %s", module_name);
        return nullptr;
    }

    // 获取绝对路径
    QString absolutePath = fileInfo.absoluteFilePath();
    qDebug() << "Absolute path:" << absolutePath;

    // 使用 QFile 读取文件
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file:" << absolutePath;
        JS_ThrowReferenceError(ctx, "Cannot open module: %s", module_name);
        return nullptr;
    }

    QByteArray content = file.readAll();
    file.close();

    // 编译并执行模块
    JSValue val = JS_Eval(ctx,
                          content.constData(),
                          content.size(),
                          module_name,
                          JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);

    if (JS_IsException(val)) {
        qDebug() << "模块存在语法错误！！";
        return nullptr;
    }

    // 获取并返回模块对象
    JSModuleDef *m = static_cast<JSModuleDef *>JS_VALUE_GET_PTR(val);
    JS_FreeValue(ctx, val);
    return m;
}

// 下面这堆操作是为了实现 QScriptEngine::ScriptOwnership
// 当指定 ownership 为 QScriptEngine::ScriptOwnership时，脚本引擎在合适的时候释放掉QObject资源
struct ObjectProp
{
    QObject *object = nullptr;
    QMetaProperty prop;

    QVariant read()
    {
        return prop.read(object);
    }

    bool write(QVariant var)
    {
        return prop.write(object, var);
    }
};
struct ObjectMethod
{
    QObject *object = nullptr;
    QMetaMethod method;

    QVariant invoke(QList<QGenericArgument> argList)
    {
        bool success = false;

        QString var;
        QGenericReturnArgument retVal = Q_RETURN_ARG(QString, var);

        // QGenericReturnArgument retVal(method.typeName());

        // Qt最多支持10个参数的，后面再补上
        switch (argList.length()) {
        case 0: // 添加无参数的情况
            success = method.invoke(object, retVal);
            break;
        case 1:
            success = method.invoke(object, retVal, argList[0]);
            break;
        case 2:
            success = method.invoke(object, retVal, argList[0], argList[1]);
            break;
        case 3:
            success = method.invoke(object, retVal, argList[0], argList[1], argList[2]);
            break;
        case 4:
            success = method.invoke(object, retVal, argList[0], argList[1], argList[2], argList[3]);
            break;
        default:
            break;
        }

        qDebug() << "success:" << success;

        if(success == false)
        {
            return QVariant();
        }

        // 返回值这里还有问题，不能这样返
        switch (method.returnType()) {
        case QMetaType::QString:
            return *(QString*)retVal.data();
            break;
        default:
            break;
        }

        return QVariant();
    }

};
// QObject wrapper for QuickJS opaque
struct QObjectWrapper {
    QObject *obj;
    QScriptEngine::ValueOwnership ownership;

    QList<ObjectProp>   propList;
    QList<ObjectMethod> methodList;
};
static JSClassID s_qobjectClassId = 0;
static void qobject_finalizer(JSRuntime *rt, JSValueConst val)
{
    void *p = JS_GetOpaque(val, s_qobjectClassId);
    if (!p)
        return;

    qDebug() << "qobject_finalizer:------------>";

    QObjectWrapper *w = static_cast<QObjectWrapper*>(p);
    if (w->ownership == QScriptEngine::ScriptOwnership) {
        if (w->obj) {
            // delete QObject immediately. In complex apps you might prefer deleteLater().
            delete w->obj;
            w->obj = nullptr;
        }
    }
    // clear lists (no heap allocations for elements)
    w->propList.clear();
    w->methodList.clear();

    delete w;
    JS_SetOpaque(val, nullptr);
}

// 要明确知道什么时候该用JS_DupValue/JS_FreeValue，什么时候不该用
// 不然就会出现 资源未释放/资源重复释放的问题


// 中断函数，用于实现 abortEval
static int custom_interrupt_handler(JSRuntime *rt, void *opaque) {
    QScriptEngine *engine = static_cast<QScriptEngine*>(opaque);
    if (!engine)
        return 1;

    // 检查中断标志
    if (std::atomic_load(&engine->interrupt_flag)) {
        // 可以在这里进行清理
        qDebug() << "Interrupting script execution" << QDateTime::currentDateTime();
        return 1;  // 返回1表示请求中断
    }
    return 0;
}

// 处理注册的c++函数
static JSValue nativeFunctionShim(JSContext *ctx,
                                  JSValueConst this_val,
                                  int argc, JSValueConst *argv,
                                  int magic)
{
    // qDebug() << "magic:" << magic;

    // retrieve the QScriptEngine associated with this JSContext
    void *opaque = JS_GetContextOpaque(ctx);
    QScriptEngine *engine = static_cast<QScriptEngine*>(opaque);
    if (!engine)
        return JS_UNDEFINED;

    // 检查中断标志
    if (std::atomic_load(&engine->interrupt_flag)) {
        auto value = JS_ThrowTypeError(ctx, "%s", "stop");
        return value;
    }

    QScriptEngine::FunctionWithArgSignature func = nullptr;
    void *arg = nullptr;
    JSValue callee;
    if (!engine->getNativeEntry(magic, func, &arg, callee))
        return JS_UNDEFINED;

    // 进入函数
    auto agent = engine->agent();
    if(agent != nullptr)
    {
        agent->functionEntry(-1);
    }

    QScriptContext qctx(ctx, this_val, argc, argv, engine, callee);
    // detect whether function was called as constructor
    bool calledAsCtor = JS_IsConstructor(ctx, this_val);
    qctx.setCalledAsConstructor(calledAsCtor);
    QScriptValue res = func(&qctx, engine, arg);

    // 退出函数
    if(agent != nullptr)
    {
        agent->functionExit(-1, res);
    }

    // qDebug() << "the returnd val:"
    //          << res.data()
    //          << res.engine()
    //          << JS_IsUndefined(res.rawValue())
    //          << res.toString();

    // 假如返回值是普通值，需要构建成JSValue
    if (res.isVariant()) {
        return QScriptValue::toJSValue(ctx, res.data());
    }

    // 如果返回的是 undefined/null 或 无效值，且这是构造调用（new Foo()），
    // 则应返回 thisObject()（遵循 Qt 的 QScriptBehavior）。
    if (!res.isValid() || res.isUndefined()) {
        // qDebug() << !res.isValid() << res.isUndefined();
        if (qctx.isCalledAsConstructor()) {
            QScriptValue thisObj = qctx.thisObject();
            if (thisObj.isValid()) {
                return JS_DupValue(ctx, thisObj.rawValue());
            }
        }
        return JS_UNDEFINED;
    }

    // 有效的 JS 值，直接 dup 并返回
    auto val = JS_DupValue(ctx, res.rawValue());
    return val;
}

// Adapter: wrap old FunctionSignature into FunctionWithArgSignature
static QScriptValue functionSignatureAdapter(QScriptContext *context, QScriptEngine *engine, void *arg)
{
    if (!arg)
        return QScriptValue();

    // arg stores the FunctionSignature as a pointer-sized value
    auto sig = reinterpret_cast<QScriptEngine::FunctionSignature>(arg);
    if (!sig)
        return QScriptValue();

    return sig(context, engine);
}

QScriptEngine::QScriptEngine(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<QScriptValue>();

    m_rt = JS_NewRuntime();
    if(!m_rt)
    {
        qCritical() << "create js runtime fail";
        return;
    }

    // // JS_SetMemoryLimit 设置堆的大小
    // // JS_SetMaxStackSize 设置栈的大小
    // // JS_SetGCThreshold 设置GC触发阈值的大小
    // JS_SetMemoryLimit(m_rt, 0);
    // JS_SetMaxStackSize(m_rt, 0);
    // JS_SetGCThreshold(m_rt, -1);


    m_ctx = JS_NewContext(m_rt);
    if(!m_ctx)
    {
        qCritical() << "create js context fail";
        JS_FreeRuntime(m_rt);
        m_rt = nullptr;
        return;
    }

    // associate C++ object with the QuickJS context
    JS_SetContextOpaque(m_ctx, this);

    // 重置中断标志
    std::atomic_store(&interrupt_flag, 0);
    // 设置中断处理器
    JS_SetInterruptHandler(m_rt, custom_interrupt_handler, this);

    // 这样是实现对QObject对象的析构
    // Register a QuickJS class to wrap QObject pointers
    if (m_rt) {
        JS_NewClassID(m_rt, &m_qobjectClassId);
        s_qobjectClassId = m_qobjectClassId;
        JSClassDef cd;
        memset(&cd, 0, sizeof(cd));
        cd.class_name = "QScriptQObject";
        cd.finalizer = qobject_finalizer;
        JS_NewClass(m_rt, m_qobjectClassId, &cd);
    }

    if(mCurCtx == nullptr)
    {
        mCurCtx = new QScriptContext(m_ctx, JS_UNDEFINED, 0, nullptr, this, JS_UNDEFINED);
    }

    // GlobalObject
    {
        JSValue g = JS_GetGlobalObject(m_ctx);

        mGlobalObject = new QScriptValue(m_ctx, g, this);

        // 这个不能调用释放，一旦释放会报错
        // 这里奇怪得很，假如在debug模式，不执行释放的话，会导致报错：list_empty(&rt->gc_obj_list)
        // 但是，在release时，假如加上了，又会报0xc000005错误,估计是重复释放
        JS_FreeValue(m_ctx, g);
    }
    JS_SetModuleLoaderFunc(m_rt, nullptr, js_module_loader_qt, this); // 传递 this
}

QScriptEngine::~QScriptEngine()
{
    clearDefaultPrototypes(); // 首先清空存储的默认类型，不然会崩溃
    
    if(agent() != nullptr)
    {
        // 这里会导致程序崩溃，后面再处理
        // for (int i = 0; i < mFileNameBuffer.length(); ++i) {
        //     agent()->scriptUnload(i);
        // }
    }

    // 清理模块系统
    m_moduleRegistry.clear();
    JS_SetModuleLoaderFunc(m_rt, nullptr, nullptr, nullptr);
    JS_SetInterruptHandler(m_rt, nullptr, nullptr);
    
    // 要先释放申请的资源，最后再释放引擎
    if(mCurCtx != nullptr)
    {
        delete mCurCtx;
        mCurCtx = nullptr;
    }
    if(mGlobalObject != nullptr)
    {
        delete mGlobalObject;
        mGlobalObject = nullptr;
    }

    if (m_ctx) {
        // clear context opaque to avoid dangling pointer
        JS_SetContextOpaque(m_ctx, nullptr);
        JS_FreeContext(m_ctx);
        m_ctx = nullptr;
    }

    if (m_rt) {
        JS_SetRuntimeOpaque(m_rt, nullptr);
        JS_FreeRuntime(m_rt);
        m_rt = nullptr;
    }
}

bool QScriptEngine::isEvaluating() const
{
    return m_evalCount.load(std::memory_order_relaxed) > 0;
}

void QScriptEngine::abortEvaluation(const QScriptValue &result)
{
    // if (!m_ctx)
    //     return;
    // JS_Throw(m_ctx, JS_DupValue(m_ctx, result.rawValue()));

    std::atomic_store(&interrupt_flag, 1);
}

void QScriptEngine::setAgent(QScriptEngineAgent *agent)
{
    m_agent = agent;
}

QScriptEngineAgent *QScriptEngine::agent() const
{
    return m_agent;
}

void QScriptEngine::collectGarbage()
{
    if (m_rt)
        JS_RunGC(m_rt);
}

QScriptContext *QScriptEngine::currentContext() const
{
    // QScriptContext
    return mCurCtx;
}

QScriptValue QScriptEngine::evaluate(const QString &program, const QString &fileName, int lineNumber)
{
    if (!m_ctx)
        return QScriptValue();

    mFileNameBuffer.push_back(fileName);
    qint64 scriptId = mFileNameBuffer.length() - 1;
    if(agent() != nullptr)
    {
        agent()->scriptLoad(scriptId, program, fileName, lineNumber);
        agent()->functionEntry(scriptId);
        agent()->mFuncStackCounter++;
    }

    struct EvalGuard {
        std::atomic<int> &cnt;
        EvalGuard(std::atomic<int> &c) : cnt(c) { cnt.fetch_add(1, std::memory_order_relaxed); }
        ~EvalGuard() { cnt.fetch_sub(1, std::memory_order_relaxed); }
    } guard(m_evalCount);

    // 中断标志位复位
    std::atomic_store(&interrupt_flag, 0);

    QByteArray ba   = program.toUtf8();
    QByteArray fnba = fileName.toUtf8();
    const char *fn  = fileName.isEmpty() ? "<eval>" : fnba.constData();
    JSValue val;

    // 使用带有flag的eval调用函数
    JSEvalOptions options;
    options.version    = JS_EVAL_OPTIONS_VERSION;
    options.eval_flags = JS_EVAL_TYPE_MODULE;  //JS_EVAL_TYPE_GLOBAL
    options.filename   = fn;
    options.line_num   = (lineNumber > 0) ? lineNumber : 1;

    // 这是加载外部的模块（js实现）
    // JS_SetModuleLoaderFunc(m_rt, nullptr, js_module_loader_qt, nullptr);

    val = JS_Eval2(m_ctx, ba.constData(), ba.size(), &options);
    QScriptValue qVal = QScriptValue(m_ctx, val, const_cast<QScriptEngine*>(this));

    // 需要通知agent
    if (JS_IsException(val))
    {
        // 假如是主动停止导致抛出的异常，那就不要通知 agent
        // 否则就要通知
        if (std::atomic_load(&interrupt_flag) == 0)
        {
            // wrap exception
            JSValue exception = JS_GetException(m_ctx);
            QScriptValue qVal = QScriptValue(m_ctx, exception, const_cast<QScriptEngine*>(this));

            // JS_GetException 之后，exception会被复位为 JS_UNINITIALIZED
            // 因此自己再搞回去
            JS_Throw(m_ctx, exception);

            // 放回去之后，就不能再调用这个了
            // JS_FreeValue(m_ctx, exception);

            if(agent() != nullptr)
            {
                agent()->exceptionThrow(scriptId, qVal, false);
            }
        }
    }

    if(agent() != nullptr)
    {
        agent()->checkFunctionPair(scriptId, qVal);
    }

    JS_FreeValue(m_ctx, val);

    return qVal;
}

QScriptValue QScriptEngine::globalObject() const
{
    return *mGlobalObject;
}

void QScriptEngine::setGlobalObject(const QScriptValue &object)
{
    Q_UNUSED(object);
    // Not implemented: replacing the global object is non-trivial in QuickJS.
}

QScriptValue QScriptEngine::newObject()
{
    if (!m_ctx)
        return QScriptValue();

    JSValue val = JS_NewObject(m_ctx);

    QScriptValue qVal = QScriptValue(m_ctx, val, this);

    JS_FreeValue(m_ctx, val);

    return qVal;
}

QScriptValue QScriptEngine::newObject(QScriptClass *scriptClass, const QScriptValue &data)
{
    Q_UNUSED(scriptClass);
    Q_UNUSED(data);
    return newObject();
}

QScriptValue QScriptEngine::newArray(uint length)
{
    if (!m_ctx)
        return QScriptValue();

    JSValue arr = JS_NewArray(m_ctx);

    if (length > 0)
        JS_SetLength(m_ctx, arr, length);

    QScriptValue qVal = QScriptValue(m_ctx, arr, this);

    JS_FreeValue(m_ctx, arr);

    return qVal;
}


QScriptValue QScriptEngine::newFunction(FunctionSignature signature, int length)
{
    if (!m_ctx)
        return QScriptValue();

    // Wrap the FunctionSignature into a FunctionWithArgSignature via adapter.
    void *arg = reinterpret_cast<void *>(signature);
    return registerNativeFunction(functionSignatureAdapter, arg, length, JS_CFUNC_constructor_or_func_magic);
}

QScriptValue QScriptEngine::newFunction(FunctionSignature signature,
                                        const QScriptValue &prototype,
                                        int length)
{
    if (!m_ctx)
        return QScriptValue();

    void *arg = reinterpret_cast<void *>(signature);
    QScriptValue fn = registerNativeFunction(functionSignatureAdapter, arg, length, JS_CFUNC_constructor_or_func_magic);

    if (prototype.isValid()) {
        // set function prototype using QuickJS constructor helper
        JS_SetConstructor(m_ctx, fn.rawValue(), prototype.rawValue());
    }

    return fn;
}

QScriptValue QScriptEngine::newFunction(FunctionWithArgSignature signature, void *arg)
{
    if (!m_ctx)
        return QScriptValue();

    return registerNativeFunction(signature, arg, 0, JS_CFUNC_generic_magic);
}

QScriptValue QScriptEngine::newVariant(const QVariant &value)
{
    if (!m_ctx)
        return QScriptValue();

    // 下面这些都得释放掉
    JSValue jVal = JS_UNDEFINED;

    if (value.type() == QVariant::String) {
        jVal = JS_NewString(m_ctx, value.toString().toUtf8().constData());
    }
    else if (value.type() == QVariant::Bool) {
        jVal = JS_NewBool(m_ctx, value.toBool());
    }
    else if (value.canConvert<double>()) {
        jVal = JS_NewFloat64(m_ctx, value.toDouble());
    }

    QScriptValue qVal = QScriptValue(m_ctx, jVal, this);

    if(!JS_IsUndefined(jVal))
    {
        JS_FreeValue(m_ctx, jVal);
    }

    return qVal;
}

QScriptValue QScriptEngine::newVariant(const QScriptValue &object, const QVariant &value)
{
    Q_UNUSED(object);
    return newVariant(value);
}

QScriptValue QScriptEngine::newQObject(QObject *object,
                                       QScriptEngine::ValueOwnership ownership,
                                       const QScriptEngine::QObjectWrapOptions &options)
{
    Q_UNUSED(options);
    if (!m_ctx || !object)
        return QScriptValue();

    JSValue jsObj = JS_NewObjectClass(m_ctx, m_qobjectClassId);
    if (JS_IsException(jsObj))
        return QScriptValue();

    QObjectWrapper *w = new QObjectWrapper{object, ownership};
    JS_SetOpaque(jsObj, w);

    // JS_SetPropertyFunctionList(ctx, jsObj, rect_proto_funcs, countof(rect_proto_funcs));

    QScriptValue qVal = QScriptValue(m_ctx, jsObj, this);

    // 创建好对象后，还得将QObject携带的属性、槽函数注册进去（信号比较麻烦，先不实现了）
    // 枚举对象的所有属性
    auto metaObj = object->metaObject();
    // reserve to avoid QList reallocation while taking element addresses
    w->propList.reserve(metaObj->propertyCount());
    w->methodList.reserve(metaObj->methodCount());
    for (int i = 0; i < metaObj->propertyCount(); ++i) {
        auto prop = metaObj->property(i);
        w->propList << ObjectProp{object, prop};
        // 使用setter/getter实现
        // 只能使用[]，不能使用[=]、[&]，否则签名对不上
        // qDebug() << prop.name();
        qVal.setProperty(prop.name(), this->newFunction([](QScriptContext *context, QScriptEngine *engine, void *data)->QScriptValue {
            ObjectProp *objProp = static_cast<ObjectProp*>(data);
            if(objProp == nullptr)
            {
                return QScriptValue();
            }

            if(context->argumentCount() == 0)
            {
                // getter
                return QScriptValue(objProp->read());
            }
            else
            {
                // setter
                bool ret = objProp->write(context->argument(0).toVariant());
                return QScriptValue(ret);
            }

            return QScriptValue();

        }, &w->propList.last()), QScriptValue::PropertyGetter | QScriptValue::PropertySetter);
    }

    // 枚举对象所有槽函数
    // Q_INVOKABLE
    for (int i = 0; i < metaObj->methodCount(); ++i) {
        auto method = metaObj->method(i);

        // qDebug() << "method"
        //          << method.name()
        //          << method.typeName()
        //          << method.methodSignature()
        //          << method.methodType();

        // 只注册部分
        switch (method.methodType()) {
        case QMetaMethod::Method:
        case QMetaMethod::Slot:
            break;
        default:
            continue;
            break;
        }
        w->methodList << ObjectMethod{object, method};

        // qDebug() << "method"
        //          << method.name();
        //          << method.typeName()
        //          << method.methodSignature()
        //          << method.parameterNames()
        //          << method.parameterTypes();

        qVal.setProperty(method.name().data(), this->newFunction([](QScriptContext *context, QScriptEngine *engine, void *data)->QScriptValue {
            ObjectMethod *objMethod = static_cast<ObjectMethod*>(data);
            if(objMethod == nullptr)
            {
                return QScriptValue();
            }

            QList<QGenericArgument> argList;
            QString arg0 = context->argument(0).toString();
            for (int argIdx = 0; argIdx < context->argumentCount(); ++argIdx) {
                argList << QGenericArgument(objMethod->method.parameterTypes().at(argIdx).data(),
                                            &arg0);
            }

            qDebug() << "invoke method:" << argList.length() << context->argument(0).toVariant();

            // 调用函数
            auto ret = objMethod->invoke(argList);

            return QScriptValue(ret);

        }, &w->methodList.last()));

    }

    JS_FreeValue(m_ctx, jsObj);

    return qVal;
}

QScriptValue QScriptEngine::newQObject(const QScriptValue &scriptObject,
                                       QObject *qtObject,
                                       QScriptEngine::ValueOwnership ownership,
                                       const QScriptEngine::QObjectWrapOptions &options)
{
    Q_UNUSED(options);
    if (!m_ctx || !qtObject)
        return QScriptValue();

    JSValue wrapper = JS_NewObjectClass(m_ctx, m_qobjectClassId);
    if (JS_IsException(wrapper))
        return QScriptValue();

    QObjectWrapper *w = new QObjectWrapper{qtObject, ownership};
    JS_SetOpaque(wrapper, w);

    if (scriptObject.isValid() && scriptObject.isObject())
    {
        // attach the wrapper to the provided scriptObject as a property named "__qt__"
        JS_SetPropertyStr(m_ctx, scriptObject.rawValue(), "__qt__", JS_DupValue(m_ctx, wrapper));
    }

    QScriptValue qVal = QScriptValue(m_ctx, wrapper, this);

    JS_FreeValue(m_ctx, wrapper);

    return qVal;
}

QScriptValue QScriptEngine::defaultPrototype(int metaTypeId) const
{
    if (!m_ctx)
        return QScriptValue();
    auto it = m_defaultPrototypes.find(metaTypeId);
    if (it == m_defaultPrototypes.end())
        return QScriptValue();
    return it.value();
}

void QScriptEngine::setDefaultPrototype(int metaTypeId, const QScriptValue &prototype)
{
    if (!m_ctx)
        return;
    // store a copy of the prototype QScriptValue
    if (prototype.isValid())
        m_defaultPrototypes.insert(metaTypeId, prototype);
    else
        m_defaultPrototypes.remove(metaTypeId);
}

QScriptValue QScriptEngine::toScriptValue(const QVariant &value)
{
    if (!m_ctx)
        return QScriptValue();

    // For simple QVariant types, keep current behavior
    if (value.type() == QVariant::String || value.type() == QVariant::Bool || value.canConvert<double>()) {
        return QScriptValue(value);
    }

    // For user types, create an object and apply default prototype if available
    JSValue obj = JS_NewObject(m_ctx);
    if (JS_IsException(obj))
        return QScriptValue();

    QScriptValue qObj(m_ctx, obj, this);

    int mt = value.userType();
    auto it = m_defaultPrototypes.find(mt);
    if (it != m_defaultPrototypes.end()) {
        QScriptValue proto = it.value();
        if (proto.isValid()) {
            // use QScriptValue API to avoid direct QuickJS calls in callers
            qObj.setPrototype(proto);
        }
    }

    JS_FreeValue(m_ctx, obj);
    return qObj;
}

void QScriptEngine::clearDefaultPrototypes()
{
    m_defaultPrototypes.clear();
}

QScriptValue QScriptEngine::registerNativeFunction(FunctionWithArgSignature signature,
                                                   void *arg,
                                                   int length,
                                                   int cproto)
{
    std::lock_guard<std::mutex> lk(m_nativeFunctionsMutex);

    // 将要新增的项目的index，恰好是目前的长度
    int magicCode = m_nativeFunctions.size();

    // 使用 JS_NewCFunctionMagic 搭配 JS_CFUNC_generic_magic
    // 可以实现使用同一个名字，注册多个函数？
    JSValue fn = JS_NewCFunctionMagic(m_ctx,
                                      nativeFunctionShim,
                                      "native",
                                      length,
                                      (JSCFunctionEnum)cproto,
                                      magicCode);

    QScriptValue qVal = QScriptValue(m_ctx, fn, this);

    JS_FreeValue(m_ctx, fn);

    NativeFunctionEntry e{signature, arg, fn};
    m_nativeFunctions.push_back(e);

    return qVal;
}

bool QScriptEngine::getNativeEntry(int idx,
                                   FunctionWithArgSignature &outFunc,
                                   void **outArg,
                                   JSValue &callee) const
{
    std::lock_guard<std::mutex> lk(m_nativeFunctionsMutex);
    if (idx < 0 || idx >= (int)m_nativeFunctions.size())
        return false;
    outFunc = m_nativeFunctions[idx].func;
    *outArg = m_nativeFunctions[idx].arg;
    callee  = m_nativeFunctions[idx].callee;
    return true;
}

QObject *QScriptEngine::qobjectFromJSValue(JSContext *ctx, JSValueConst val) const
{
    if (!ctx)
        return nullptr;
    void *p = JS_GetOpaque2(ctx, val, m_qobjectClassId);
    if (!p)
        return nullptr;
    QObjectWrapper *w = static_cast<QObjectWrapper*>(p);
    return w->obj;
}

int QScriptEngine::moduleInitCallback(JSContext *ctx, JSModuleDef *m) {
    QScriptEngine *engine = static_cast<QScriptEngine*>(JS_GetContextOpaque(ctx));
    if (!engine) return -1;

    // 获取模块名称
    JSAtom nameAtom = JS_GetModuleName(ctx, m);
    const char *nameCStr = JS_AtomToCString(ctx, nameAtom);
    QString moduleName = QString::fromUtf8(nameCStr);
    JS_FreeCString(ctx, nameCStr);
    JS_FreeAtom(ctx, nameAtom);

    // 获取导出列表
    auto exports = engine->m_moduleRegistry.value(moduleName);

    // 现在可以安全地设置导出值（var_ref 已存在）
    for (const auto &exp : exports) {
        JSAtom atom = JS_NewAtom(ctx,exp.nameUtf8.constData());
        JSValue val = JS_UNDEFINED;

        // 根据类型创建 JSValue
        switch (exp.type) {
        case ModuleExport::Int32:
            val = JS_NewInt32(ctx, exp.value.toInt());
            break;
        case ModuleExport::Int64:
            val = JS_NewInt64(ctx, exp.value.toLongLong());
            break;
        case ModuleExport::Double:
            val = JS_NewFloat64(ctx, exp.value.toDouble());
            break;
        case ModuleExport::String:
            val = JS_NewStringLen(ctx, exp.value.toString().toUtf8().constData(), exp.value.toString().toUtf8().size());
            break;
        case ModuleExport::Object:
            // 处理嵌套对象（如 QObject）
            if (exp.value.userType() == qMetaTypeId<QScriptValue>()) {
                QScriptValue sv = exp.value.value<QScriptValue>();
                val = JS_DupValue(ctx, sv.rawValue());
            } else {
                val = JS_NewObject(ctx); // 空对象占位
            }
            break;
        case ModuleExport::Function:
            // QScriptValue 函数需要在这里设置实际值
            if (exp.scriptValue.isValid()) {
                val = JS_DupValue(ctx, exp.scriptValue.rawValue());
            }
            break;
        }

        // 设置导出值
        int ret = JS_SetModuleExport(ctx, m, exp.nameUtf8.constData(), val);
        if (ret < 0) {
            qWarning() << "Failed to set module export:" << exp.nameUtf8.constData();
        }

        // 这里不能释放value，不然释放运行时调用GC的时候会报错
        // JS_FreeValue(ctx, val);
        JS_FreeAtom(ctx, atom);
    }

    return 0; // 成功
}

bool QScriptEngine::hasUncaughtException() const
{
    if (!m_ctx)
        return false;
    return JS_HasException(m_ctx);
}

QScriptValue QScriptEngine::uncaughtException() const
{
    if (!m_ctx)
        return QScriptValue();

    JSValue exc = JS_GetException(m_ctx);

    QScriptValue qVal = QScriptValue(m_ctx, exc, const_cast<QScriptEngine*>(this));

    JS_FreeValue(m_ctx, exc);

    return qVal;
}

int QScriptEngine::uncaughtExceptionLineNumber() const
{
    Q_UNUSED(this);
    return -1;
}

QStringList QScriptEngine::uncaughtExceptionBacktrace() const
{
    return QStringList();
}

QScriptValue QScriptEngine::nullValue()
{
    if (!m_ctx) return QScriptValue();
    return QScriptValue(m_ctx, JS_NULL, this);
}

QScriptValue QScriptEngine::undefinedValue()
{
    if (!m_ctx) return QScriptValue();
    return QScriptValue(m_ctx, JS_UNDEFINED, this);
}

void QScriptEngine::registerModule(const QString &moduleName, const QList<ModuleExport> &exports)
{
    // 保存到注册表，供模块加载器使用
    m_moduleRegistry[moduleName] = exports;
}

QScriptSyntaxCheckResult QScriptEngine::checkSyntax(const QString &program)
{
    auto rt = JS_NewRuntime();
    if(!rt)
    {
        qCritical() << "create js runtime fail";
        return QScriptSyntaxCheckResult();
    }

    auto ctx = JS_NewContext(rt);
    if(!ctx)
    {
        qCritical() << "create js context fail";
        JS_FreeRuntime(rt);
        rt = nullptr;
        return QScriptSyntaxCheckResult();
    }

    QByteArray ba = program.toUtf8();
    const char *fn = "<check>";
    int flags = JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY;

    JSValue val = JS_Eval(ctx, ba.constData(), ba.size(), fn, flags);

    if (!JS_IsException(val)) {
        JS_FreeValue(ctx, val);

        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);

        // qDebug() << "not exception:-----";
        return QScriptSyntaxCheckResult();
    }

    QString message;
    int line = -1;
    int column = -1;

    QScriptValue aVal = QScriptValue(ctx, val, nullptr);

    JS_FreeValue(ctx, val);

    message = aVal.toString();

    // Try to parse file:line:column from message
    QString first = message.trimmed();
    QString inside;
    int lp = first.lastIndexOf('(');
    int rp = first.lastIndexOf(')');
    if (lp != -1 && rp > lp) {
        inside = first.mid(lp + 1, rp - lp - 1).trimmed();
    } else {
        if (first.contains(" at ", Qt::CaseInsensitive)) {
            int at = first.lastIndexOf(" at ", -1, Qt::CaseInsensitive);
            if (at >= 0)
                inside = first.mid(at + 4).trimmed();
        } else {
            inside = first;
        }
    }

    QStringList parts = inside.split(':');
    if (parts.size() >= 2) {
        bool ok1 = false;
        bool ok2 = false;
        column = parts.takeLast().toInt(&ok1);
        line = parts.takeLast().toInt(&ok2);
        if (!ok1) column = -1;
        if (!ok2) {
            if (ok1) { line = column; column = -1; }
            else line = -1;
        }
    }

    return QScriptSyntaxCheckResult(message, line, column);
}
