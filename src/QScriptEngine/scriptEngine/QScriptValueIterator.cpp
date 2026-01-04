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

// 原本的js_json_deep_clone
// static JSValue js_json_deep_clone(JSContext *ctx, JSValueConst this_val) {
//     int32_t tag = JS_VALUE_GET_TAG(this_val);

//     // 1. 基本类型：使用JS_NewXxx创建新值，避免架构特定的内存对齐问题
//     switch (tag) {
//     case JS_TAG_BOOL:
//         return JS_NewBool(ctx, JS_VALUE_GET_BOOL(this_val));

//     case JS_TAG_INT:
//         return JS_NewInt32(ctx, JS_VALUE_GET_INT(this_val));

//     // 序列化
//     JSValue str = JS_Call(ctx, stringify, json, 1, &this_val);
//     if (JS_IsException(str)) {
//         JS_FreeValue(ctx, str);
//         JS_FreeValue(ctx, parse);
//         JS_FreeValue(ctx, stringify);
//         JS_FreeValue(ctx, json);
//         JS_FreeValue(ctx, global);
//         return JS_EXCEPTION;
//     }

//     // 反序列化
//     JSValue cloned = JS_Call(ctx, parse, json, 1, &str);

//     // 清理
//     JS_FreeValue(ctx, str);
//     JS_FreeValue(ctx, parse);
//     JS_FreeValue(ctx, stringify);
//     JS_FreeValue(ctx, json);
//     JS_FreeValue(ctx, global);

//     return cloned;
// }

// 针对LoongArch64架构优化的深拷贝函数
// 深拷贝的通用规则：所有 ECMAScript 不可变类型（Primitive values），深拷贝时都应该共享引用，而非创建新实例。
static JSValue js_deep_clone(JSContext *ctx, JSValueConst this_val) {
    int32_t tag = JS_VALUE_GET_TAG(this_val);

    // 1. 基本类型：使用JS_NewXxx创建新值，避免架构特定的内存对齐问题
    switch (tag) {
    case JS_TAG_BOOL:
        return JS_NewBool(ctx, JS_VALUE_GET_BOOL(this_val));

    case JS_TAG_INT:
        return JS_NewInt32(ctx, JS_VALUE_GET_INT(this_val));

    case JS_TAG_FLOAT64:
        return JS_NewFloat64(ctx, JS_VALUE_GET_FLOAT64(this_val));

    case JS_TAG_NULL:
        return JS_NULL; // 常量，无需复制

    case JS_TAG_UNDEFINED:
        return JS_UNDEFINED; // 常量，无需复制

    case JS_TAG_STRING: {
        // 字符串：提取内容后新建，避免直接引用
        size_t len;
        const char *str = JS_ToCStringLen(ctx, &len, this_val);
        if (!str) {
            return JS_EXCEPTION;
        }
        JSValue ret = JS_NewStringLen(ctx, str, len);
        JS_FreeCString(ctx, str);
        return ret;
    }

    case JS_TAG_SYMBOL: {
        // Symbol：提取描述后新建（创建新实例但保留描述）
        JSValue desc_val = JS_GetPropertyStr(ctx, this_val, "description");
        const char *desc = NULL;

        if (!JS_IsUndefined(desc_val)) {
            desc = JS_ToCString(ctx, desc_val);
        }

        // 创建新的Symbol实例（0表示普通Symbol，非全局）
        JSValue ret = JS_NewSymbol(ctx, desc, 0);

        if (desc) {
            JS_FreeCString(ctx, desc);
        }
        JS_FreeValue(ctx, desc_val);

        return ret;
    }

    // 对于其他基本类型（如果存在）使用默认处理
    default:
        if (tag <= JS_TAG_UNDEFINED) {
            // 其他原始类型（如bigint）直接复制
            return JS_DupValue(ctx, this_val);
        }
        break; // 继续处理对象类型
    }

    // 2. 对象类型需要递归处理
    if (tag == JS_TAG_OBJECT) {
        // 2.1 数组处理：递归复制每个元素
        if (JS_IsArray(this_val)) {
            int64_t len;
            if (JS_GetLength(ctx, this_val, &len) < 0) {
                return JS_EXCEPTION;
            }

            JSValue new_array = JS_NewArray(ctx);
            if (JS_IsException(new_array)) {
                return JS_EXCEPTION;
            }

            for (int64_t i = 0; i < len; i++) {
                JSValue elem = JS_GetPropertyUint32(ctx, this_val, (uint32_t)i);
                if (JS_IsException(elem)) {
                    JS_FreeValue(ctx, new_array);
                    return JS_EXCEPTION;
                }

                JSValue cloned_elem = js_deep_clone(ctx, elem);
                JS_FreeValue(ctx, elem);

                if (JS_IsException(cloned_elem)) {
                    JS_FreeValue(ctx, new_array);
                    return JS_EXCEPTION;
                }

                if (JS_SetPropertyUint32(ctx, new_array, (uint32_t)i, cloned_elem) < 0) {
                    JS_FreeValue(ctx, cloned_elem);
                    JS_FreeValue(ctx, new_array);
                    return JS_EXCEPTION;
                }
            }

            return new_array;
        }

        // 2.2 函数处理：函数不可变，直接复制引用
        if (JS_IsFunction(ctx, this_val)) {
            // 注意：在LoongArch上如果仍有问题，可改为返回函数本身或JS_NULL
            return JS_DupValue(ctx, this_val);
        }

        // 2.3 普通对象：递归复制所有可枚举属性（包括字符串和Symbol）
        JSPropertyEnum *props = NULL;
        uint32_t prop_count = 0;

        // 获取所有可枚举属性（字符串+Symbol）
        int flags = JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK | JS_GPN_ENUM_ONLY;
        if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, this_val, flags) < 0) {
            return JS_EXCEPTION;
        }

        JSValue new_obj = JS_NewObject(ctx);
        if (JS_IsException(new_obj)) {
            JS_FreePropertyEnum(ctx, props, prop_count);
            return JS_EXCEPTION;
        }

        // 递归复制每个属性值
        for (uint32_t i = 0; i < prop_count; i++) {
            JSAtom atom = props[i].atom;
            JSValue prop_val = JS_GetProperty(ctx, this_val, atom);
            if (JS_IsException(prop_val)) {
                JS_FreePropertyEnum(ctx, props, prop_count);
                JS_FreeValue(ctx, new_obj);
                return JS_EXCEPTION;
            }

            JSValue cloned_val = js_deep_clone(ctx, prop_val);
            JS_FreeValue(ctx, prop_val);

            if (JS_IsException(cloned_val)) {
                JS_FreePropertyEnum(ctx, props, prop_count);
                JS_FreeValue(ctx, new_obj);
                return JS_EXCEPTION;
            }

            if (JS_SetProperty(ctx, new_obj, atom, cloned_val) < 0) {
                JS_FreeValue(ctx, cloned_val);
                JS_FreePropertyEnum(ctx, props, prop_count);
                JS_FreeValue(ctx, new_obj);
                return JS_EXCEPTION;
            }
        }

        JS_FreePropertyEnum(ctx, props, prop_count);
        return new_obj;
    }

    // 3. 未知类型：直接复制（应谨慎使用）
    return JS_DupValue(ctx, this_val);
}

QScriptValue QScriptValueIterator::value() const
{
    if (!m_currentAtom)
        return QScriptValue();
    JSContext *ctx = m_object.engine() ? m_object.engine()->ctx() : nullptr;
    if (!ctx)
        return QScriptValue();
    // 目前经过测试，发现在LoongArch64设备中，假如使用浅复制会导致在后续操作中发生segment fail / bus error
    // x86设备无法复现
    QScriptValue qVal;
    if(1) // 深复制
    {
        JSValue v = JS_GetProperty(ctx, m_object.rawValue(), m_currentAtom);
        auto k = js_deep_clone(ctx, v);
        // auto k = js_json_deep_clone(ctx, v);
        JS_FreeValue(ctx, v);

        qVal = QScriptValue(ctx, k, m_object.engine());

        // 构建QScriptValue时已复制，因此需要清理掉
        JS_FreeValue(ctx, k);
    }
    else // 浅复制
    {
        JSValue v = JS_GetProperty(ctx, m_object.rawValue(), m_currentAtom);
        qVal = QScriptValue(ctx, v, m_object.engine());
        // 构建QScriptValue时已复制，因此需要清理掉
        JS_FreeValue(ctx, v);
    }

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
