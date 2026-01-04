#ifndef JSSYNTAXHIGHLIGHTER_H
#define JSSYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QRegularExpression>
#include <QTextCharFormat>

#ifdef Q_OS_WIN
#pragma execution_character_set("utf-8")
#endif

// 注释块用户数据类
class AnnotationUserData : public QTextBlockUserData
{
public:
    AnnotationUserData(bool isAnnotation = false) : m_isAnnotation(isAnnotation) {}
    bool isAnnotation() const { return m_isAnnotation; }
private:
    bool m_isAnnotation;
};

//==============================================================================
//  JavaScript语法高亮器
//==============================================================================
class JSSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit JSSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;

    QTextCharFormat keywordFormat;
    QTextCharFormat classFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat multiLineCommentFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat functionFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat operatorFormat;
    QTextCharFormat regExpressFormat;
};

#endif // JSSYNTAXHIGHLIGHTER_H
