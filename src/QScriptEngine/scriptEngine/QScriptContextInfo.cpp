#include "QScriptContextInfo.h"
#include "QScriptContext.h"
#include "QScriptValue.h"

#include <QStringList>

QScriptContextInfo::QScriptContextInfo()
{
}

QScriptContextInfo::QScriptContextInfo(const QScriptContext *context)
{
    if (!context) {
        m_valid = false;
        return;
    }

    // Try to extract the first backtrace line from the context and parse it.
    QStringList bt = context->backtrace();
    if (bt.isEmpty()) {
        m_valid = false;
        return;
    }

    QString first = bt.first().trimmed();
    // Typical stack frames look like:
    // "at funcName (file.js:123:45)" or "file.js:123:45" or "at file.js:123:45"

    QString inside;
    int lp = first.lastIndexOf('(');
    int rp = first.lastIndexOf(')');
    if (lp != -1 && rp > lp) {
        inside = first.mid(lp + 1, rp - lp - 1).trimmed();
        // function name before '('
        QString before = first.left(lp).trimmed();
        // remove leading "at " if present
        if (before.startsWith("at ", Qt::CaseInsensitive))
            before = before.mid(3).trimmed();
        m_functionName = before;
    } else {
        // no parentheses, try to find after "at "
        if (first.startsWith("at ", Qt::CaseInsensitive))
            inside = first.mid(3).trimmed();
        else
            inside = first;
    }

    // inside now should be like file:line:column or file:line
    // Parse from end splitting by ':'
    QString filePart = inside;
    int col = -1;
    int line = -1;

    // split by ':' from end
    QStringList parts = inside.split(':');
    if (parts.size() >= 2) {
        bool ok1 = false;
        bool ok2 = false;
        col = parts.takeLast().toInt(&ok1);
        line = parts.takeLast().toInt(&ok2);
        filePart = parts.join(":");
        if (!ok1)
            col = -1;
        if (!ok2) {
            // maybe only one numeric part (line)
            if (ok1) {
                line = col;
                col = -1;
            } else {
                line = -1;
            }
        }
    }

    m_fileName = filePart;
    m_line = (line >= 0) ? line : 0;
    m_column = (col >= 0) ? col : 0;
    m_valid = true;

    // If function name was not parsed earlier, try to find one in the frame prefix
    if (m_functionName.isEmpty()) {
        // examples: "at funcName (file:line)" handled above; if none, try heuristics
        // look for "at <name>" prefix
        if (first.startsWith("at ", Qt::CaseInsensitive)) {
            QString rest = first.mid(3).trimmed();
            int sp = rest.indexOf(' ');
            if (sp > 0)
                m_functionName = rest.left(sp).trimmed();
        }
    }
}

bool QScriptContextInfo::isValid() const
{
    return m_valid;
}

QString QScriptContextInfo::fileName() const
{
    return m_fileName;
}

int QScriptContextInfo::lineNumber() const
{
    return m_line;
}

int QScriptContextInfo::columnNumber() const
{
    return m_column;
}

QString QScriptContextInfo::functionName() const
{
    return m_functionName;
}

QScriptValue QScriptContextInfo::function() const
{
    Q_UNUSED(this);
    // We cannot reliably obtain the JS function object via public QuickJS API
    return QScriptValue();
}

QString QScriptContextInfo::toString() const
{
    if (!m_valid)
        return QString();
    if (!m_fileName.isEmpty()) {
        if (m_line > 0)
            return QString("%1:%2").arg(m_fileName).arg(m_line);
        return m_fileName;
    }
    return QString();
}
