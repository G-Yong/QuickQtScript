#include <QScriptValue>
#include <QScriptEngine>

#include <QStringList>
#include <QDebug>
#include <QVariantMap>
#include <QVariantList>

#include <vector>

extern "C" {
#include "quickjs.h"
}

// 要明确知道什么时候该用JS_DupValue/JS_FreeValue，什么时候不该用
// 不然就会出现 资源未释放/资源重复释放的问题
QScriptValue::QScriptValue()
    : m_ctx(nullptr), m_value(JS_UNDEFINED), m_engine(nullptr)
{
    m_variant = "this is string";
}

QScriptValue::QScriptValue(const char *value)
    : m_ctx(nullptr), m_value(JS_UNDEFINED), m_engine(nullptr)
{
    m_isVariant = true;
    m_variant = QVariant(QString(value));
}

QScriptValue::QScriptValue(const QString &value)
    : m_ctx(nullptr), m_value(JS_UNDEFINED), m_engine(nullptr)
{
    m_isVariant = true;
    m_variant = QVariant(value);
}

QScriptValue::QScriptValue(double value)
    : m_ctx(nullptr), m_value(JS_UNDEFINED), m_engine(nullptr)
{
    m_isVariant = true;
    m_variant = QVariant(value);
}

QScriptValue::QScriptValue(uint value)
    : m_ctx(nullptr), m_value(JS_UNDEFINED), m_engine(nullptr)
{
    m_isVariant = true;
    m_variant = QVariant((quint32)value);
}

QScriptValue::QScriptValue(int value)
    : m_ctx(nullptr), m_value(JS_UNDEFINED), m_engine(nullptr)
{
    m_isVariant = true;
    m_variant = QVariant((qint32)value);
}

QScriptValue::QScriptValue(bool value)
    : m_ctx(nullptr), m_value(JS_UNDEFINED), m_engine(nullptr)
{
    m_isVariant = true;
    m_variant = QVariant(value);
}

QScriptValue::QScriptValue(const QVariant value)
{
    m_isVariant = true;
    m_variant = value;
}

QScriptValue::QScriptValue(JSContext *ctx, JSValue val, QScriptEngine *engine)
    : m_ctx(ctx), m_value(JS_DupValue(ctx, val)), m_engine(engine)
{
    // qDebug() << "JS_DupValue1";
    // JS_FreeValue(ctx, val);
}

QScriptValue::QScriptValue(const QScriptValue &other)
    : m_ctx(other.m_ctx), m_value(JS_UNDEFINED), m_engine(other.m_engine)
{
    // qDebug() << "JS_DupValue2";
    m_isVariant = other.m_isVariant;
    m_variant   = other.m_variant;

    if (m_ctx)
        m_value = JS_DupValue(m_ctx, other.m_value);

}

QScriptValue &QScriptValue::operator=(const QScriptValue &other)
{
    // qDebug() << "JS_DupValue3";

    if (this == &other)
        return *this;

    m_isVariant = other.m_isVariant;
    m_variant   = other.m_variant;

    if (m_ctx && !JS_IsUndefined(m_value))
        JS_FreeValue(m_ctx, m_value);

    m_ctx    = other.m_ctx;
    m_engine = other.m_engine;

    if (m_ctx)
        m_value = JS_DupValue(m_ctx, other.m_value);
    else
        m_value = JS_UNDEFINED;

    return *this;
}

QScriptValue::~QScriptValue()
{
    if (m_ctx && !JS_IsUndefined(m_value))
        JS_FreeValue(m_ctx, m_value);

    // if(m_ctx)
    //     JS_FreeValue(m_ctx, m_value);
}

QVariant QScriptValue::data() const
{
    return toVariant();
}

QScriptEngine *QScriptValue::engine() const
{
    return m_engine;
}

bool QScriptValue::equals(const QScriptValue &other) const
{
    if (!m_ctx || !other.m_ctx)
        return false;
    return JS_IsEqual(m_ctx, m_value, other.m_value) != 0;
}

QScriptValue QScriptValue::call(const QScriptValue &thisObject, const QScriptValueList &args)
{
    // 如果当前值不是函数，返回无效的 QScriptValue
    if (!m_ctx || !isFunction())
        return QScriptValue();

    // 准备 this 对象
    JSValue thisVal = JS_UNDEFINED;
    if (thisObject.isValid() && thisObject.isObject()) {
        thisVal = thisObject.rawValue();
    } else if (m_engine) {
        // 如果 thisObject 不是对象，使用全局对象作为 this
        thisVal = JS_GetGlobalObject(m_ctx);
    }

    // 准备参数数组
    int argc = args.size();
    std::vector<JSValue> argv(argc);
    for (int i = 0; i < argc; ++i) {
        const QScriptValue &arg = args.at(i);
        if (arg.isVariant()) {
            argv[i] = toJSValue(m_ctx, arg.data());
        } else if (arg.isValid()) {
            argv[i] = JS_DupValue(m_ctx, arg.rawValue());
        } else {
            argv[i] = JS_UNDEFINED;
        }
    }

    // 调用函数
    JSValue result = JS_Call(m_ctx, m_value, thisVal, argc, argc > 0 ? argv.data() : nullptr);

    // 释放参数（由 JS_Call 复制，这里需要释放我们创建的副本）
    for (int i = 0; i < argc; ++i) {
        JS_FreeValue(m_ctx, argv[i]);
    }

    // 如果使用了全局对象作为 this，需要释放
    if (!(thisObject.isValid() && thisObject.isObject()) && m_engine) {
        JS_FreeValue(m_ctx, thisVal);
    }

    // 包装结果并返回
    QScriptValue qResult(m_ctx, result, m_engine);
    JS_FreeValue(m_ctx, result);
    return qResult;
}

QScriptValue QScriptValue::call(const QScriptValue &thisObject, const QScriptValue &arguments)
{
    // 如果当前值不是函数，返回无效的 QScriptValue
    if (!m_ctx || !isFunction())
        return QScriptValue();

    // 将 arguments 转换为参数列表
    QScriptValueList args;

    // arguments 可以是 arguments 对象、数组、null 或 undefined
    if (arguments.isNull() || arguments.isUndefined() || !arguments.isValid()) {
        // 无参数调用
    } else if (arguments.isArray()) {
        // 从数组中提取参数
        int64_t len = 0;
        if (JS_GetLength(m_ctx, arguments.rawValue(), &len) >= 0) {
            for (int64_t i = 0; i < len; ++i) {
                args.append(arguments.property((quint32)i));
            }
        }
    } else if (arguments.isObject()) {
        // 可能是 arguments 对象，尝试读取 length 属性
        QScriptValue lengthVal = arguments.property("length");
        if (lengthVal.isNumber()) {
            int len = lengthVal.toInt32();
            for (int i = 0; i < len; ++i) {
                args.append(arguments.property((quint32)i));
            }
        }
    }

    // 调用带参数列表的 call 重载
    return call(thisObject, args);
}

bool QScriptValue::isArray() const { return m_ctx && JS_IsArray(m_value); }
bool QScriptValue::isBool() const
{
    if (m_isVariant)
        return m_variant.type() == QVariant::Bool;
    return m_ctx && JS_IsBool(m_value);
}
bool QScriptValue::isDate() const { return m_ctx && JS_IsDate(m_value); }
bool QScriptValue::isError() const {
    return m_ctx && (JS_IsError(m_value) || JS_IsException(m_value));
}
bool QScriptValue::isFunction() const { return m_ctx && JS_IsFunction(m_ctx, m_value); }
bool QScriptValue::isNull() const { return m_ctx && JS_IsNull(m_value); }
bool QScriptValue::isNumber() const
{
    if (m_isVariant) {
        QVariant::Type type = m_variant.type();
        return type == QVariant::Int || type == QVariant::UInt || 
               type == QVariant::LongLong || type == QVariant::ULongLong ||
               type == QVariant::Double;
    }
    return m_ctx && JS_IsNumber(m_value);
}
bool QScriptValue::isObject() const { return m_ctx && JS_IsObject(m_value); }
bool QScriptValue::isRegExp() const { return m_ctx && JS_IsRegExp(m_value); }
bool QScriptValue::isString() const
{
    if (m_isVariant)
        return m_variant.type() == QVariant::String;
    return m_ctx && JS_IsString(m_value);
}
bool QScriptValue::isUndefined() const { return m_ctx && JS_IsUndefined(m_value); }
bool QScriptValue::isValid() const { return m_ctx != nullptr; }
bool QScriptValue::isVariant() const { return m_isVariant; }

bool QScriptValue::isQMetaObject() const { return false; }
bool QScriptValue::isQObject() const { return false; }

QScriptValue QScriptValue::property(const QString &name) const
{
    if (!m_ctx)
        return QScriptValue();

    JSValue val = JS_GetPropertyStr(m_ctx, m_value, name.toUtf8().constData());

    QScriptValue qVal = QScriptValue(m_ctx, val, m_engine);

    JS_FreeValue(m_ctx, val);

    return qVal;
}

QScriptValue QScriptValue::property(quint32 arrayIndex) const
{
    if (!m_ctx)
        return QScriptValue();

    JSValue val = JS_GetPropertyUint32(m_ctx, m_value, arrayIndex);

    QScriptValue qVal = QScriptValue(m_ctx, val, m_engine);

    JS_FreeValue(m_ctx, val);

    return qVal;
}

QScriptValue QScriptValue::prototype() const
{
    if (!m_ctx)
        return QScriptValue();

    JSValue proto = JS_GetPrototype(m_ctx, m_value);
    QScriptValue qProto(m_ctx, proto, m_engine);
    JS_FreeValue(m_ctx, proto);
    return qProto;
}

void QScriptValue::setProperty(const char *name, const QScriptValue &value, const PropertyFlags &flags)
{
    setProperty(QString(name), value, flags);
}

void QScriptValue::setProperty(const QString &name, const QScriptValue &value, const PropertyFlags &flags)
{
    if (!m_ctx)
        return;

    JSValue val_to_set = JS_UNDEFINED;
    if (value.isVariant()) {
        val_to_set = toJSValue(m_ctx, value.data());
    } else {
        val_to_set = JS_DupValue(m_ctx, value.rawValue());
        // val_to_set = value.rawValue();
    }

    // compute QuickJS property attribute bits
    int qjs_flags = JS_PROP_C_W_E; // default: configurable, writable, enumerable
    JSAtom atom = JS_NewAtom(m_ctx, name.toUtf8().constData());
    if (atom == JS_ATOM_NULL) {
        JS_FreeValue(m_ctx, val_to_set);
        return;
    }

    if (flags == KeepExistingFlags) {
        JSPropertyDescriptor desc;
        int ret = JS_GetOwnProperty(m_ctx, &desc, m_value, atom);
        if (ret > 0) {
            qjs_flags = desc.flags & JS_PROP_C_W_E;
        }
        JS_FreeValue(m_ctx, desc.getter);
        JS_FreeValue(m_ctx, desc.setter);
        JS_FreeValue(m_ctx, desc.value);
    } else {
        qjs_flags = 0;
        if (!(flags & ReadOnly))          qjs_flags |= JS_PROP_WRITABLE;
        if (!(flags & Undeletable))       qjs_flags |= JS_PROP_CONFIGURABLE;
        if (!(flags & SkipInEnumeration)) qjs_flags |= JS_PROP_ENUMERABLE;
    }

    // getter/setter handling
    if ((flags & PropertyGetter) || (flags & PropertySetter)) {
        JSValue getter = JS_UNDEFINED;
        JSValue setter = JS_UNDEFINED;

        auto handler = JS_DupValue(m_ctx, value.rawValue());
        // auto handler = value.rawValue();

        if (flags & PropertyGetter) {
            // use supplied value as getter function if it's a function
            if (!value.isVariant())
            {
                // getter = JS_DupValue(m_ctx, value.rawValue());
                // getter = value.rawValue();

                getter = handler;
            }
            // qDebug() << "is variant:" << value.isVariant();
        }
        if (flags & PropertySetter) {
            if (!value.isVariant())
            {
                // setter = JS_DupValue(m_ctx, value.rawValue());
                // setter = value.rawValue();

                setter = handler;
            }
        }

        // qDebug() << "get set:" << JS_IsUndefined(getter) << JS_IsUndefined(setter);

        // JS_DefinePropertyGetSet will free getter/setter
        JS_DefinePropertyGetSet(m_ctx, m_value, atom, getter, setter, qjs_flags);
        JS_FreeAtom(m_ctx, atom);
        return;
    }

    // define value property (JS_DefinePropertyValue will free val_to_set)
    JS_DefinePropertyValue(m_ctx, m_value, atom, val_to_set, qjs_flags);
    JS_FreeAtom(m_ctx, atom);
}

void QScriptValue::setProperty(quint32 arrayIndex, const QScriptValue &value, const PropertyFlags &flags)
{
    if (!m_ctx)
        return;

    // For indexed properties, QuickJS doesn't provide a get/set helper
    // that takes flags. We'll treat index properties as value properties.
    JSValue val_to_set = JS_UNDEFINED;

    if(value.isVariant())
    {
        val_to_set = toJSValue(m_ctx, value.data());
    }
    else
    {
        val_to_set = JS_DupValue(m_ctx, value.rawValue());
        // val_to_set = value.rawValue();
    }

    // qDebug() << "set array prop--->" << arrayIndex << value.toString() << value.isVariant();

    JS_DefinePropertyValueUint32(m_ctx, m_value, arrayIndex, val_to_set, JS_PROP_C_W_E);
}

void QScriptValue::setPrototype(const QScriptValue &prototype)
{
    if (!m_ctx)
        return;
    JSValue protoVal = JS_UNDEFINED;
    if (prototype.isValid())
        protoVal = JS_DupValue(m_ctx, prototype.rawValue());
    // JS_SetPrototype takes a JSValueConst; it does not dup the value
    JS_SetPrototype(m_ctx, m_value, protoVal);
    if (!JS_IsUndefined(protoVal))
        JS_FreeValue(m_ctx, protoVal);
}

bool QScriptValue::strictlyEquals(const QScriptValue &other) const
{
    if (!m_ctx || !other.m_ctx)
        return false;
    return JS_IsStrictEqual(m_ctx, m_value, other.m_value);
}

bool QScriptValue::toBool() const
{
    if (m_isVariant)
        return m_variant.toBool();
    if (!m_ctx)
        return false;
    int v = JS_ToBool(m_ctx, m_value);
    if (v < 0)
        return false;
    return v != 0;
}

QDateTime QScriptValue::toDateTime() const
{
    // minimal: if value is date, convert to string and parse
    if (!m_ctx)
        return QDateTime();
    if (!isDate())
        return QDateTime();
    JSValue s = JS_ToString(m_ctx, m_value);
    const char *c = JS_ToCString(m_ctx, s);
    QString str = QString::fromUtf8(c ? c : "");
    JS_FreeCString(m_ctx, c);
    JS_FreeValue(m_ctx, s);
    return QDateTime::fromString(str, Qt::ISODate);
}

qint32 QScriptValue::toInt32() const
{
    if (m_isVariant)
        return m_variant.toInt();
    if (!m_ctx)
        return 0;
    int32_t res = 0;
    if (JS_ToInt32(m_ctx, &res, m_value) < 0)
        return 0;
    return (qint32)res;
}

double QScriptValue::toInteger() const
{
    return toNumber();
}

double QScriptValue::toNumber() const
{
    if (m_isVariant)
        return m_variant.toDouble();
    if (!m_ctx)
        return 0;
    JSValue num = JS_ToNumber(m_ctx, m_value);
    double d = 0.0;
    if (JS_IsException(num)) {
        JS_FreeValue(m_ctx, num);
        return 0;
    }
    if (JS_IsNumber(num)) {
        d = JS_VALUE_GET_NORM_TAG(num) == JS_TAG_FLOAT64 ? JS_VALUE_GET_FLOAT64(num) : (double)JS_VALUE_GET_INT(num);
    }
    JS_FreeValue(m_ctx, num);
    return (double)d;
}

QString QScriptValue::toStringInternal(int depth) const
{
    QString res;
    // Array // 输出格式为: [ val1, val2, ..., valn ]
    if (JS_IsArray(m_value)) {
        if (depth <= 0)
        {
            return "[Array]";
        }
        QString res = "[ ";
        int64_t len = 0;
        if (JS_GetLength(m_ctx, m_value, &len) >= 0) {
            for (int64_t i = 0; i < len; ++i) {
                if (i > 0) res += ", ";
                JSValue element = JS_GetPropertyUint32(m_ctx, m_value, (uint32_t)i);
                QScriptValue v(m_ctx, element, m_engine);
                // QString s = v.toString();
                QString s = v.toStringInternal(depth - 1);
                if (JS_IsString(element)) s = "'" + s + "'"; // 字符串值加上引号
                if (JS_IsUndefined(element)) s = "undefined";
                res += s;
                JS_FreeValue(m_ctx, element);
            }
        }
        return res + " ]";
    }

    // Symbol // 输出格式为: Symbol('description')
    if (JS_IsSymbol(m_value)) {
        JSValue desc = JS_GetPropertyStr(m_ctx, m_value, "description");
        const char *c = JS_ToCString(m_ctx, desc);
        QString result = QString("Symbol(%1)").arg(c ? c : "");
        JS_FreeCString(m_ctx, c);
        JS_FreeValue(m_ctx, desc);
        return result;
    }

    // Set/Map/WeakSet/WeakMap 集中处理
    bool isSet = JS_IsSet(m_value); // 输出格式为: Set(n) { key1, key2, ..., keyn }
    bool isMap = JS_IsMap(m_value); // 输出格式为: Map(n) { key1 => val1, key2 => val2, ..., keyn => valn }
    bool isWeakSet = JS_IsWeakSet(m_value);
    bool isWeakMap = JS_IsWeakMap(m_value);

    if (isSet || isMap || isWeakSet || isWeakMap) {
        if (depth <= 0)
        {
            return (isSet) ? "[Set]" : "[Map]";
        }
        // Weak容器不可迭代，直接返回占位符
        if (isWeakSet) return "{ < WeakSet > }";
        if (isWeakMap) return "{ < WeakMap > }";

        // Set/Map 使用 Array.from 转换
        JSValue global = JS_GetGlobalObject(m_ctx);
        JSValue arrayCtor = JS_GetPropertyStr(m_ctx, global, "Array");
        JSValue fromFn = JS_GetPropertyStr(m_ctx, arrayCtor, "from");

        JSValue argv[1] = { JS_DupValue(m_ctx, m_value) };
        JSValue arr = JS_Call(m_ctx, fromFn, arrayCtor, 1, argv);

        JS_FreeValue(m_ctx, fromFn);
        JS_FreeValue(m_ctx, arrayCtor);
        JS_FreeValue(m_ctx, global);
        JS_FreeValue(m_ctx, argv[0]);

        if (!JS_IsException(arr) && JS_IsArray(arr)) {
            QString res = "";
            int64_t len = 0;
            int64_t i = 0;
            if (JS_GetLength(m_ctx, arr, &len) >= 0) {
                res += "{ ";
                for (i = 0; i < len; ++i) {
                    if (i > 0) res += ", ";
                    JSValue item = JS_GetPropertyUint32(m_ctx, arr, (uint32_t)i);

                    if (isMap) {
                        // Map条目是[key, value]数组
                        JSValue key = JS_GetPropertyUint32(m_ctx, item, 0);
                        JSValue val = JS_GetPropertyUint32(m_ctx, item, 1);

                        QScriptValue qk(m_ctx, key, m_engine);
                        QScriptValue qv(m_ctx, val, m_engine);
                        // QString ks = qk.toString();
                        QString ks = qk.toStringInternal(depth - 1);
                        // QString vs = qv.toString();
                        QString vs = qv.toStringInternal(depth - 1);

                        // 字符串属性值加引号
                        if (JS_IsString(key)) ks = "'" + ks + "'";
                        if (JS_IsString(val)) vs = "'" + vs + "'";

                        res += ks + " => " + vs;
                        JS_FreeValue(m_ctx, key);
                        JS_FreeValue(m_ctx, val);
                    } else {
                        // Set条目是值
                        QScriptValue qv(m_ctx, item, m_engine);
                        // QString s = qv.toString();
                        QString s = qv.toStringInternal(depth - 1);
                        if (JS_IsString(item)) s = "'" + s + "'"; // 字符串值加上引号
                        if (JS_IsUndefined(item)) s = "undefined";
                        res += s;
                    }

                    JS_FreeValue(m_ctx, item);
                }
            }
            JS_FreeValue(m_ctx, arr);
            res += " }";
            res = ((isSet) ? QString("Set(%1) ").arg(i) : QString("Map(%1) ").arg(i)) + res; // 开头添加类型
            return res;
        }

        if (JS_IsException(arr))
        {
            JSValue exception = JS_GetException(m_ctx);
            JSValue ss = JS_ToString(m_ctx, exception);
            const char *c = JS_ToCString(m_ctx, ss);
            res += QString::fromUtf8(c ? c : "");

            JS_FreeCString(m_ctx, c);
            JS_FreeValue(m_ctx, ss);
            JS_FreeValue(m_ctx, arr);
            JS_Throw(m_ctx, exception);
        } else {
            JS_FreeValue(m_ctx, arr);
        }

        return (isSet) ? "Set(0) { }" : "Map(0) { }" ;
    }

    // 普通对象（不包括函数、错误、日期等）
    if (JS_IsObject(m_value)
        && !JS_IsFunction(m_ctx, m_value) // 函数不特殊处理
        && !JS_IsError(m_value)     // 错误不特殊处理
        && !JS_IsDate(m_value)      // 日期不特殊处理
        && !JS_IsRegExp(m_value)    // 正则表达式不特殊处理
        ) {
        // 检查是不是迭代器类型变量
        ClassId cid = static_cast<ClassId>(JS_GetClassID(m_value));
        switch(cid)
        {
        // 目前暂时不处理迭代器类型变量，只做类型提示
        case JS_CLASS_SET_ITERATOR: return "< SET_ITERATOR >";
        case JS_CLASS_MAP_ITERATOR: return "< MAP_ITERATOR >";
        case JS_CLASS_ARRAY_ITERATOR: return "< ARRAY_ITERATOR >";
        case JS_CLASS_STRING_ITERATOR: return "< STRING_ITERATOR >";
        case JS_CLASS_REGEXP_STRING_ITERATOR: return "< REGEXP_STRING_ITERATOR >";
        }
        // Object 输出格式为: { prop1: val1, prop2: val2, ..., propn: valn }
        if (depth <= 0)
        {
            return "[Object]";
        }
        QString res = "{ ";
        JSPropertyEnum *props = nullptr;
        uint32_t plen = 0;
        int flags = JS_GPN_STRING_MASK;
        // if(JS_IsError(m_value) == false) // 弄出来的会有stack信息，冗余
        {
            // error的属性是不可枚举的，强行加这个，会啥都没有
            flags = flags | JS_GPN_ENUM_ONLY;
        }
        int ret = JS_GetOwnPropertyNames(m_ctx, &props, &plen, m_value, flags);

        if (ret >= 0 && props) {
            for (uint32_t i = 0; i < plen; ++i) {
                if (i > 0) res += ", ";

                JSAtom atom = props[i].atom;
                const char *name = JS_AtomToCString(m_ctx, atom);
                res += (name ? name : "") + QString(": ");
                JS_FreeCString(m_ctx, name);

                JSValue propVal = JS_GetProperty(m_ctx, m_value, atom);
                QScriptValue qv(m_ctx, propVal, m_engine);
                // QString s = qv.toString();
                QString s = qv.toStringInternal(depth - 1);
                if (JS_IsString(propVal)) s = "'" + s + "'"; // 字符串值加上引号
                if (JS_IsUndefined(propVal)) s = "undefined";
                res += s;
                JS_FreeValue(m_ctx, propVal);
            }
            JS_FreePropertyEnum(m_ctx, props, plen);
        }
        return res + " }";
    }

    // 普通的类型直接调用JS_ToString()
    JSValue s = JS_ToString(m_ctx, m_value);
    //  有可能调用toString()失败;
    if (JS_IsException(s))
    {
        JSValue exception = JS_GetException(m_ctx);
        JSValue ss = JS_ToString(m_ctx, exception);
        const char *c = JS_ToCString(m_ctx, ss);
        res += QString::fromUtf8(c ? c : "");

        JS_FreeCString(m_ctx, c);
        JS_FreeValue(m_ctx, ss);
        JS_FreeValue(m_ctx, s);
        JS_Throw(m_ctx, exception);
    }
    else
    {
        const char *c = JS_ToCString(m_ctx, s);

        res = QString::fromUtf8(c ? c : "");

        JS_FreeCString(m_ctx, c);
        JS_FreeValue(m_ctx, s);
    }
    return res;
}

QString QScriptValue::toString() const
{
    if (m_isVariant)
        return m_variant.toString();
    if (!m_ctx)
        return QString();

    QString res;

    if(JS_IsException(m_value) == false)
    {
        // 默认递归限制深度为3
        return toStringInternal();
    }
    else
    {
        // 假如一个对象进行JS_IsException时为true，其本身是没有携带多少有用信息的，需要通过JS_GetException才能获取到具体的信息
        // 通过JS_GetException获取到的对象，在进行JS_IsException时反而为false，因为它是一个特殊值（或者是普通值）
        // 我们的engine是把JS_GetException获取到的对象抛出，而返回evaluate的是原始的对象

        // 假如是异常，要特殊处理
        // qDebug() << "is exception";

        // wrap exception
        JSValue exception = JS_GetException(m_ctx);

        {
            JSValue s = JS_ToString(m_ctx, exception); // 取的是 exception
            const char *c = JS_ToCString(m_ctx, s);

            res += QString::fromUtf8(c ? c : "");

            JS_FreeCString(m_ctx, c);
            JS_FreeValue(m_ctx, s);
        }

        // 这里不需要将backtrace加进来
        if(0)
        {
            // 1. 获取 "stack" 对应的原子（atom），这是高效查找属性的键
            JSAtom atom_stack = JS_NewAtom(m_ctx, "stack");

            // 2. 从异常对象中获取 stack 属性的值
            JSValue stack_val = JS_GetProperty(m_ctx, exception, atom_stack);

            // 3. 检查并转换堆栈信息为C字符串
            if (!JS_IsUndefined(stack_val)) {
                const char *stack_str = JS_ToCString(m_ctx, stack_val);
                if (stack_str) {
                    // 4. 打印错误和堆栈
                    fprintf(stderr, "Exception occurred:\n%s\n", stack_str);

                    res += QString("\n") + QString(stack_str);

                    JS_FreeCString(m_ctx, stack_str); // 释放C字符串
                }
            } else {
                // 如果 stack 属性不存在，打印一个提示
                fprintf(stderr, "Exception occurred, but no stack trace is available.\n");
            }

            // 5. 释放所有创建的 JS 值
            JS_FreeValue(m_ctx, stack_val);
            JS_FreeAtom(m_ctx, atom_stack); // 释放原子
        }

        JS_Throw(m_ctx, exception);
        // JS_FreeValue(m_ctx, exception);
    }

    return res;
}

quint32 QScriptValue::toUInt32() const
{
    if (m_isVariant)
        return m_variant.toUInt();
    if (!m_ctx)
        return 0;

    uint32_t v = 0;
    if (JS_ToUint32(m_ctx, &v, m_value) < 0)
        return 0;

    return (quint32)v;
}

static QVariant JSValueToQVariant(JSContext *ctx, JSValueConst val, QScriptEngine *engine, int depth = 8);
QVariant QScriptValue::toVariant() const
{
    if (!m_ctx)
        return m_isVariant ? m_variant : QVariant();
    if (m_isVariant)
        return m_variant;

    if (isString())
        return QVariant(toString());
    if (isBool())
        return QVariant(toBool());
    if (isNumber())
        return QVariant((double)toNumber());

    if(isObject())
    {
        // convert object to QVariant via helper
        return JSValueToQVariant(m_ctx, m_value, m_engine, 8);
    }

    if(isArray())
    {
        // arrays are handled by JSValueToQVariant as well
        return JSValueToQVariant(m_ctx, m_value, m_engine, 8);
    }

    // 假如前面都不符合，就返回字符串形式
    return QVariant(toString());
}

QObject *QScriptValue::toQObject() const
{
    if (!m_ctx || !m_engine)
        return nullptr;

    if (!isObject())
        return nullptr;

    return m_engine->qobjectFromJSValue(m_ctx, m_value);
}

JSValue QScriptValue::toJSValue(JSContext *ctx, QVariant var)
{
    JSValue theVal = JS_UNDEFINED;

    switch (var.type()) {
    case QVariant::Bool:{
        theVal = JS_NewBool(ctx, var.toBool());
    }break;
    case QVariant::Int:
    case QVariant::UInt:
    case QVariant::LongLong:
    case QVariant::ULongLong:{
        theVal = JS_NewInt64(ctx, var.toLongLong());
    }break;
    case QVariant::Double:{
        theVal = JS_NewFloat64(ctx, var.toDouble());
    }break;
    case QVariant::String:{
        QString str = var.toString();
        QByteArray ba = str.toUtf8();
        theVal = JS_NewStringLen(ctx, ba.constData(), ba.size());
    }break;
    default:
        break;
    }

    return theVal;
}

// Recursive helper: convert a QuickJS value to QVariant with depth limit
static QVariant JSValueToQVariant(JSContext *ctx, JSValueConst val, QScriptEngine *engine, int depth)
{
    if (!ctx || depth <= 0)
        return QVariant();

    if (JS_IsUndefined(val) || JS_IsNull(val))
        return QVariant();

    if (JS_IsString(val)) {
        JSValue s = JS_ToString(ctx, val);
        const char *c = JS_ToCString(ctx, s);
        QString res = QString::fromUtf8(c ? c : "");
        JS_FreeCString(ctx, c);
        JS_FreeValue(ctx, s);
        return QVariant(res);
    }
    if (JS_IsBool(val)) {
        int b = JS_ToBool(ctx, val);
        return QVariant(b != 0);
    }
    if (JS_IsNumber(val)) {
        JSValue num = JS_ToNumber(ctx, val);
        double d = 0;
        if (!JS_IsException(num)) {
            if (JS_IsNumber(num)) {
                d = JS_VALUE_GET_NORM_TAG(num) == JS_TAG_FLOAT64 ? JS_VALUE_GET_FLOAT64(num) : (double)JS_VALUE_GET_INT(num);
            }
            JS_FreeValue(ctx, num);
        }
        return QVariant(d);
    }
    if (JS_IsDate(val)) {
        JSValue s = JS_ToString(ctx, val);
        const char *c = JS_ToCString(ctx, s);
        QString str = QString::fromUtf8(c ? c : "");
        JS_FreeCString(ctx, c);
        JS_FreeValue(ctx, s);
        return QVariant(QDateTime::fromString(str, Qt::ISODate));
    }

    // If this object wraps a QObject, return it as a QVariant (QObject*)
    if (engine) {
        QObject *obj = engine->qobjectFromJSValue(ctx, val);
        if (obj) {
            return QVariant::fromValue(obj);
        }
    }

    // Arrays
    if (JS_IsArray(val)) {
        int64_t len = 0;
        if (JS_GetLength(ctx, val, &len) < 0)
            return QVariant();
        QVariantList list;
        for (int64_t i = 0; i < len; ++i) {
            JSValue el = JS_GetPropertyUint32(ctx, val, (uint32_t)i);
            QVariant v = JSValueToQVariant(ctx, el, engine, depth - 1);
            list << v;
            JS_FreeValue(ctx, el);
        }
        return QVariant(list);
    }

    // Objects -> QVariantMap (own enumerable string properties)
    if (JS_IsObject(val)) {

        // qDebug() << "is obj";

        // 检查是否为 Set
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue setConstructor = JS_GetPropertyStr(ctx, global, "Set");
        bool isSet = JS_IsInstanceOf(ctx, val, setConstructor) == 1;
        JS_FreeValue(ctx, setConstructor);
        JS_FreeValue(ctx, global);

        if (isSet) {
            // qDebug() << "is Set object";

            // 方法1：使用 values() 方法
            if(1)
            {
                QVariantList list;

                JSValue valuesFunc = JS_GetPropertyStr(ctx, val, "values");
                if (!JS_IsException(valuesFunc)) {
                    JSValue iterator = JS_Call(ctx, valuesFunc, val, 0, nullptr);
                    JS_FreeValue(ctx, valuesFunc);

                    if (!JS_IsException(iterator)) {
                        JSValue nextFunc = JS_GetPropertyStr(ctx, iterator, "next");

                        while (true) {
                            JSValue nextResult = JS_Call(ctx, nextFunc, iterator, 0, nullptr);
                            if (JS_IsException(nextResult)) {
                                JS_FreeValue(ctx, nextResult);
                                break;
                            }

                            JSValue done = JS_GetPropertyStr(ctx, nextResult, "done");
                            if (JS_ToBool(ctx, done)) {
                                JS_FreeValue(ctx, done);
                                JS_FreeValue(ctx, nextResult);
                                break;
                            }
                            JS_FreeValue(ctx, done);

                            JSValue value = JS_GetPropertyStr(ctx, nextResult, "value");
                            QVariant element = JSValueToQVariant(ctx, value, engine, depth - 1);
                            list.append(element);

                            JS_FreeValue(ctx, value);
                            JS_FreeValue(ctx, nextResult);
                        }

                        JS_FreeValue(ctx, nextFunc);
                        JS_FreeValue(ctx, iterator);
                    }
                }

                return QVariant(list);
            }


            // 方法2：使用 Array.from 将 Set 转换为数组
            if(0)
            {
                JSValue arrayConstructor = JS_GetPropertyStr(ctx, global, "Array");
                JSValue fromMethod = JS_GetPropertyStr(ctx, arrayConstructor, "from");

                JSValue args[] = { val };
                JSValue array = JS_Call(ctx, fromMethod, arrayConstructor, 1, args);

                // 现在将数组转换为 QVariantList
                QVariantList list;
                int64_t len;
                if (JS_IsArray(array)) {
                    JS_GetLength(ctx, array, &len);
                    for (int64_t i = 0; i < len; i++) {
                        JSValue element = JS_GetPropertyUint32(ctx, array, i);
                        QVariant qv = JSValueToQVariant(ctx, element, engine, depth - 1);
                        list.append(qv);
                        JS_FreeValue(ctx, element);
                    }
                }

                JS_FreeValue(ctx, array);
                JS_FreeValue(ctx, fromMethod);
                JS_FreeValue(ctx, arrayConstructor);
                JS_FreeValue(ctx, global);

                return QVariant(list);
            }
        }

        // 属性为名称的对象
        JSPropertyEnum *props = nullptr;
        uint32_t plen = 0;
        QVariantMap map;
        int ret = JS_GetOwnPropertyNames(ctx, &props, &plen, val, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY);
        if (ret >= 0 && props) {
            for (uint32_t i = 0; i < plen; ++i) {
                JSAtom atom = props[i].atom;
                const char *name = JS_AtomToCString(ctx, atom);
                QString key = name ? QString::fromUtf8(name) : QString();
                JS_FreeCString(ctx, name);

                JSValue v = JS_GetProperty(ctx, val, atom);
                QVariant qv = JSValueToQVariant(ctx, v, engine, depth - 1);
                map.insert(key, qv);
                JS_FreeValue(ctx, v);
            }
            JS_FreePropertyEnum(ctx, props, plen);
        }

        return QVariant(map);
    }

    // 假如前面都不符合，那利用原生的JS_ToString返回字符串形式
    QString info;
    {
        JSValue s = JS_ToString(ctx, val);
        const char *c = JS_ToCString(ctx, s);

        info = QString::fromUtf8(c ? c : "");

        JS_FreeCString(ctx, c);
        JS_FreeValue(ctx, s);
    }

    return QVariant(info);
}

quint16 QScriptValue::toUInt16() const
{
    return (quint16)toUInt32();
}
