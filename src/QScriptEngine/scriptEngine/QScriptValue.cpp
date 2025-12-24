#include <QScriptValue>
#include <QScriptEngine>

#include <QStringList>
#include <QDebug>
#include <QVariantMap>
#include <QVariantList>

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

QString QScriptValue::toString() const
{
    if (m_isVariant)
        return m_variant.toString();
    if (!m_ctx)
        return QString();

    QString res;

    if(JS_IsException(m_value) == false)
    {
        if (JS_IsObject(m_value) && !JS_IsFunction(m_ctx, m_value) && !JS_IsArray(m_value))
        {
            // 处理对象，输出格式为 { prop1: value1, prop2: value2 }
            res = "{ ";
            
            JSPropertyEnum *props = nullptr;
            uint32_t plen = 0;
            int ret = JS_GetOwnPropertyNames(m_ctx, &props, &plen, m_value, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY);
            if (ret >= 0 && props) {
                for (uint32_t i = 0; i < plen; ++i) {
                    if (i > 0) {
                        res += ", ";
                    }
                    
                    JSAtom atom = props[i].atom;
                    const char *name = JS_AtomToCString(m_ctx, atom);
                    QString key = name ? QString::fromUtf8(name) : QString();
                    JS_FreeCString(m_ctx, name);
                    
                    res += key + ": ";
                    
                    JSValue propValue = JS_GetProperty(m_ctx, m_value, atom);
                    QScriptValue scriptPropValue(m_ctx, propValue, m_engine);
                    QString propStr = scriptPropValue.toString();
                    
                    // 如果属性值是字符串，需要添加引号
                    if (JS_IsString(propValue)) {
                        propStr = "\"" + propStr + "\"";
                    } else if (JS_IsUndefined(propValue)) {
                        propStr = "undefined";
                    }
                    
                    res += propStr;
                    JS_FreeValue(m_ctx, propValue);
                }
                JS_FreePropertyEnum(m_ctx, props, plen);
            }
            
            res += " }";
            return res;
        }
        else if (JS_IsArray(m_value))
        {
            // 处理数组类型，输出格式为 [value1, value2, ...]
            res = "[ ";
            
            int64_t len = 0;
            if (JS_GetLength(m_ctx, m_value, &len) >= 0) {
                for (int64_t i = 0; i < len; ++i) {
                    if (i > 0) {
                        res += ", ";
                    }
                    
                    JSValue element = JS_GetPropertyUint32(m_ctx, m_value, (uint32_t)i);
                    QScriptValue scriptElement(m_ctx, element, m_engine);
                    QString elementStr = scriptElement.toString();
                    
                    // 如果元素是字符串，需要添加引号
                    if (JS_IsString(element)) {
                        elementStr = "\"" + elementStr + "\"";
                    } else if (JS_IsUndefined(element)) {
                        elementStr = "undefined";
                    }
                    
                    res += elementStr;
                    JS_FreeValue(m_ctx, element);
                }
            }
            
            res += " ]";
            return res;
        }
        else if (JS_IsSymbol(m_value))
        {
            // 处理 Symbol 类型
            JSValue description = JS_GetPropertyStr(m_ctx, m_value, "description");
            if (!JS_IsUndefined(description)) {
                const char *desc = JS_ToCString(m_ctx, description);
                QString symbolDesc = QString::fromUtf8(desc ? desc : "");
                JS_FreeCString(m_ctx, desc);
                JS_FreeValue(m_ctx, description);
                return "Symbol(" + symbolDesc + ")";
            } else {
                JS_FreeValue(m_ctx, description);
                return "Symbol()";
            }
        }
        JSValue s = JS_ToString(m_ctx, m_value);
        //  有可能调用toString()失败;
        //  对于symbol类型，也不能直接用toString;  // 可能需要在这里额外处理
        //  对于object类型，也不会自动转成格式化的输出，例如：{ value: undefined, done: false } // 可能需要在这里额外处理
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

    }
    else
    {
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

    return QVariant();
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

    return QVariant();
}

quint16 QScriptValue::toUInt16() const
{
    return (quint16)toUInt32();
}
