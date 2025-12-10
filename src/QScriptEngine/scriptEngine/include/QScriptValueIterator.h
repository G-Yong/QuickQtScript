#ifndef QSCRIPTENGINE_QSCRIPTVALUEITERATOR_H
#define QSCRIPTENGINE_QSCRIPTVALUEITERATOR_H

#include <QString>
#include "QScriptValue.h"

class QScriptValueIterator
{
public:
    explicit QScriptValueIterator(const QScriptValue &object);
    ~QScriptValueIterator();

    bool hasNext() const;
    void next();

    QString name() const;
    QScriptValue value() const;

    void setValue(const QScriptValue &value);
    void remove();

private:
    QScriptValue m_object;
    JSPropertyEnum *m_props{nullptr};
    uint32_t m_len{0};
    uint32_t m_index{0};
    JSAtom m_currentAtom{0};
};

#endif // QSCRIPTENGINE_QSCRIPTVALUEITERATOR_H
