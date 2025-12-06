#pragma once

#include <QString>

class QScriptSyntaxCheckResult
{
public:
    enum State {
        NoError,
        Error
    };

    QScriptSyntaxCheckResult();
    QScriptSyntaxCheckResult(const QString &errorMessage, int errorLine = -1, int errorColumn = -1);
    QScriptSyntaxCheckResult(const QScriptSyntaxCheckResult &other);
    QScriptSyntaxCheckResult &operator=(const QScriptSyntaxCheckResult &other);
    ~QScriptSyntaxCheckResult();

    // Qt-style API
    int errorColumnNumber() const;
    int errorLineNumber() const;
    QString errorMessage() const;
    QScriptSyntaxCheckResult::State state() const;

    // compatibility helpers
    bool isValid() const { return state() == NoError; }
    int errorLine() const { return errorLineNumber(); }
    int errorColumn() const { return errorColumnNumber(); }
    QString toString() const;

private:
    QScriptSyntaxCheckResult::State m_state{NoError};
    QString m_errorMessage;
    int m_errorLine{-1};
    int m_errorColumn{-1};
};
