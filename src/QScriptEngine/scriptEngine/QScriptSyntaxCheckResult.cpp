#include <QScriptSyntaxCheckResult>

// Implementation of QScriptSyntaxCheckResult with Qt-compatible API
QScriptSyntaxCheckResult::QScriptSyntaxCheckResult()
    : m_state(Valid), m_errorLine(-1), m_errorColumn(-1)
{
}

QScriptSyntaxCheckResult::QScriptSyntaxCheckResult(const QString &errorMessage, int errorLine, int errorColumn)
    : m_state(Error), m_errorMessage(errorMessage), m_errorLine(errorLine), m_errorColumn(errorColumn)
{
}

QScriptSyntaxCheckResult::QScriptSyntaxCheckResult(const QScriptSyntaxCheckResult &other)
    : m_state(other.m_state), m_errorMessage(other.m_errorMessage), m_errorLine(other.m_errorLine), m_errorColumn(other.m_errorColumn)
{
}

QScriptSyntaxCheckResult &QScriptSyntaxCheckResult::operator=(const QScriptSyntaxCheckResult &other)
{
    if (this == &other)
        return *this;
    m_state = other.m_state;
    m_errorMessage = other.m_errorMessage;
    m_errorLine = other.m_errorLine;
    m_errorColumn = other.m_errorColumn;
    return *this;
}

QScriptSyntaxCheckResult::~QScriptSyntaxCheckResult() = default;

int QScriptSyntaxCheckResult::errorColumnNumber() const
{
    return m_errorColumn;
}

int QScriptSyntaxCheckResult::errorLineNumber() const
{
    return m_errorLine;
}

QString QScriptSyntaxCheckResult::errorMessage() const
{
    return m_errorMessage;
}

QScriptSyntaxCheckResult::State QScriptSyntaxCheckResult::state() const
{
    return m_state;
}

// QString QScriptSyntaxCheckResult::toString() const
// {
//     if (m_state == NoError)
//         return QString();
//     if (m_errorLine >= 0) {
//         if (m_errorColumn >= 0)
//             return QString("%1 at %2:%3").arg(m_errorMessage).arg(m_errorLine).arg(m_errorColumn);
//         return QString("%1 at %2").arg(m_errorMessage).arg(m_errorLine);
//     }
//     return m_errorMessage;
// }
