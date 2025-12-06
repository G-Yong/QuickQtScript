#pragma once

#include <QtGlobal>
#include <QString>
#include <QVariant>
#include <QDateTime>
#include <QObject>

extern "C" {
#include "../quickjs/quickjs.h"
}

class QScriptEngine;

class QScriptValue
{
public:
    enum ResolveFlag {
        ResolveLocal        = 0x00,
        ResolvePrototype    = 0x01,
        ResolveScope        = 0x02,
        ResolveFull         = ResolvePrototype | ResolveScope
    };
    Q_DECLARE_FLAGS(ResolveFlags, ResolveFlag)

    enum PropertyFlag {
        ReadOnly            = 0x00000001,
        Undeletable         = 0x00000002,
        SkipInEnumeration   = 0x00000004,

        PropertyGetter      = 0x00000008,
        PropertySetter      = 0x00000010,

        QObjectMember       = 0x00000020,

        KeepExistingFlags   = 0x00000800,

        UserRange           = 0xff000000            // Users may use these as they see fit.
    };
    Q_DECLARE_FLAGS(PropertyFlags, PropertyFlag)

    enum SpecialValue {
        NullValue,
        UndefinedValue
    };

public:
    QScriptValue();
    QScriptValue(const char *value);
    QScriptValue(const QString &value);
    QScriptValue(double value);
    QScriptValue(uint value);
    QScriptValue(int value);
    QScriptValue(bool value);
    QScriptValue(JSContext *ctx, JSValue val, QScriptEngine *engine = nullptr);
    QScriptValue(const QScriptValue &other);
    QScriptValue &operator=(const QScriptValue &other);
    ~QScriptValue();

    QVariant data() const;
    QScriptEngine *engine() const;

    bool equals(const QScriptValue &other) const;

    bool isArray() const;
    bool isBool() const;
    bool isDate() const;
    bool isError() const; // 此函数还有问题
    bool isFunction() const;
    bool isNull() const;
    bool isNumber() const;
    bool isObject() const;
    bool isRegExp() const;
    bool isString() const;
    bool isUndefined() const;
    bool isValid() const;
    bool isVariant() const;

    bool isQMetaObject() const;
    bool isQObject() const;

    QScriptValue property(const QString &name) const;
    QScriptValue property(quint32 arrayIndex) const;


    void setProperty(const QString &name,
                     const QScriptValue &value,
                     const PropertyFlags &flags = KeepExistingFlags);
    void setProperty(quint32 arrayIndex,
                     const QScriptValue &value,
                     const PropertyFlags &flags = KeepExistingFlags);
    void setProperty(const char *name,
                     const QScriptValue &value,
                     const PropertyFlags &flags = KeepExistingFlags);

    bool strictlyEquals(const QScriptValue &other) const;

    bool toBool() const;
    QDateTime toDateTime() const;
    qint32 toInt32() const;
    double toInteger() const;
    double toNumber() const;
    QString toString() const;
    quint32 toUInt32() const;
    quint16 toUInt16() const;
    QVariant toVariant() const;
    QObject *toQObject() const;

     /* 以下函数仅供内部使用*/
    JSValue rawValue() const { return m_value; }
    static JSValue toJSValue(JSContext *ctx, QVariant var);

private:
    JSContext *m_ctx{nullptr};
    JSValue m_value{JS_UNDEFINED};
    QScriptEngine *m_engine{nullptr};
    mutable QVariant m_cachedVariant;
    bool m_isVariant{false};
    QVariant m_variant;
};

// 添加这行宏声明，启用 Flags 的按位或运算符
Q_DECLARE_OPERATORS_FOR_FLAGS(QScriptValue::ResolveFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QScriptValue::PropertyFlags)
