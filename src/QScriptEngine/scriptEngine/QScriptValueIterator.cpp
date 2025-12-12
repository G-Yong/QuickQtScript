#include <QScriptValueIterator>
#include <QScriptEngine>
#include <QString>
#include <QDebug>

extern "C" {
#include "quickjs.h"
}

QScriptValueIterator::QScriptValueIterator(const QScriptValue &object)
    : m_object(object), m_props(nullptr), m_len(0), m_index(0), m_currentAtom(0)
{
    if (!m_object.isValid() || !m_object.isObject())
        return;
    JSContext *ctx = m_object.engine() ? m_object.engine()->ctx() : nullptr;
    if (!ctx)
        return;
    JSValue obj = m_object.rawValue();
    // get own property names (enumerable string+symbol)
    if (JS_GetOwnPropertyNames(ctx, &m_props, &m_len, obj, JS_GPN_ENUM_ONLY | JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK) < 0) {
        m_props = nullptr;
        m_len = 0;
        return;
    }
    m_index = 0;
}

QScriptValueIterator::~QScriptValueIterator()
{
    if (m_props && m_object.isValid()) {
        JSContext *ctx = m_object.engine() ? m_object.engine()->ctx() : nullptr;
        if (ctx)
            JS_FreePropertyEnum(ctx, m_props, m_len);
    }
    m_props = nullptr;
}

bool QScriptValueIterator::hasNext() const
{
    return m_props && (m_index < m_len);
}

void QScriptValueIterator::next()
{
    if (!hasNext())
        return;
    m_currentAtom = m_props[m_index].atom;
    m_index++;
}

QString QScriptValueIterator::name() const
{
    if (!m_currentAtom)
        return QString();
    JSContext *ctx = m_object.engine() ? m_object.engine()->ctx() : nullptr;
    if (!ctx)
        return QString();
    const char *cstr = JS_AtomToCString(ctx, m_currentAtom);
    QString res = QString::fromUtf8(cstr ? cstr : "");
    JS_FreeCString(ctx, cstr);
    return res;
}

// 使用JSON方法深复制
static JSValue js_json_deep_clone(JSContext *ctx, JSValueConst this_val) {

    // 获取JSON对象
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue json = JS_GetPropertyStr(ctx, global, "JSON");
    JSValue stringify = JS_GetPropertyStr(ctx, json, "stringify");
    JSValue parse = JS_GetPropertyStr(ctx, json, "parse");

    // 序列化
    JSValue str = JS_Call(ctx, stringify, json, 1, &this_val);
    if (JS_IsException(str)) {
        JS_FreeValue(ctx, str);
        JS_FreeValue(ctx, parse);
        JS_FreeValue(ctx, stringify);
        JS_FreeValue(ctx, json);
        JS_FreeValue(ctx, global);
        return JS_EXCEPTION;
    }

    // 反序列化
    JSValue cloned = JS_Call(ctx, parse, json, 1, &str);

    // 清理
    JS_FreeValue(ctx, str);
    JS_FreeValue(ctx, parse);
    JS_FreeValue(ctx, stringify);
    JS_FreeValue(ctx, json);
    JS_FreeValue(ctx, global);

    return cloned;
}

// 这种方式存在问题
JSValue JS_DeepCloneValue(JSContext *ctx, JSValueConst val) {
    size_t size = 0;
    // 使用 JS_WRITE_OBJ_REFERENCE 标志来支持对象引用，从而处理循环依赖
    int write_flags = JS_WRITE_OBJ_REFERENCE;

    // 1. 将JSValue序列化为字节数组
    uint8_t *buf = JS_WriteObject(ctx, &size, val, write_flags);
    if (!buf) {
        // 如果序列化失败（例如，包含不支持的外部C对象），则返回异常
        return JS_EXCEPTION;
    }

    // 2. 从字节数组反序列化为新的JSValue
    // 同样需要 JS_READ_OBJ_REFERENCE 标志来正确解析对象引用
    int read_flags = JS_READ_OBJ_REFERENCE;
    JSValue cloned_val = JS_ReadObject(ctx, buf, size, read_flags);

    // 3. 释放临时的字节数组
    // 注意：JS_WriteObject返回的内存需要使用js_free_rt释放
    js_free_rt(JS_GetRuntime(ctx), buf);

    return cloned_val; // 返回深复制后的新JSValue
}

QScriptValue QScriptValueIterator::value() const
{
    if (!m_currentAtom)
        return QScriptValue();
    JSContext *ctx = m_object.engine() ? m_object.engine()->ctx() : nullptr;
    if (!ctx)
        return QScriptValue();

    JSValue v = JS_GetProperty(ctx, m_object.rawValue(), m_currentAtom);

    auto k = js_json_deep_clone(ctx, v);
    // auto k = JS_DeepCloneValue(ctx, v);
    QScriptValue qVal = QScriptValue(ctx, k, m_object.engine());

    // QScriptValue qVal = QScriptValue(ctx, v, m_object.engine());

    // 构建QScriptValue时已复制，因此需要清理掉
    JS_FreeValue(ctx, v);

    JS_FreeValue(ctx, k);

    return qVal;
}

void QScriptValueIterator::setValue(const QScriptValue &value)
{
    if (!m_currentAtom)
        return;
    JSContext *ctx = m_object.engine() ? m_object.engine()->ctx() : nullptr;
    if (!ctx)
        return;
    JS_SetProperty(ctx, m_object.rawValue(), m_currentAtom, JS_DupValue(ctx, value.rawValue()));
}

void QScriptValueIterator::remove()
{
    if (!m_currentAtom)
        return;
    JSContext *ctx = m_object.engine() ? m_object.engine()->ctx() : nullptr;
    if (!ctx)
        return;
    JS_DeleteProperty(ctx, m_object.rawValue(), m_currentAtom, 0);
}
