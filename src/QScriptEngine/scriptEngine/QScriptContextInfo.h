#pragma once

#include <QString>

class QScriptContext;
class QScriptValue;

class QScriptContextInfo
{
public:
    QScriptContextInfo();
    explicit QScriptContextInfo(const QScriptContext *context);

    // Copy semantics
    QScriptContextInfo(const QScriptContextInfo &other) = default;
    QScriptContextInfo &operator=(const QScriptContextInfo &other) = default;

    bool isValid() const;
    QString fileName() const;
    int lineNumber() const;
    int columnNumber() const;
    QString functionName() const;
    QScriptValue function() const;
    QString toString() const;

private:
    bool m_valid{false};
    QString m_fileName;
    int m_line{0};
    int m_column{0};
    QString m_functionName;
};
