#include "QScriptValueIterator.h"
#include "QScriptEngine.h"
#include <QString>
#include <QDebug>

extern "C" {
#include "../quickjs/quickjs.h"
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

QScriptValue QScriptValueIterator::value() const
{
    if (!m_currentAtom)
        return QScriptValue();
    JSContext *ctx = m_object.engine() ? m_object.engine()->ctx() : nullptr;
    if (!ctx)
        return QScriptValue();
    JSValue v = JS_GetProperty(ctx, m_object.rawValue(), m_currentAtom);

    QScriptValue qVal = QScriptValue(ctx, v, m_object.engine());

    // 构建QScriptValue时已复制，因此需要清理掉
    JS_FreeValue(ctx, v);

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
