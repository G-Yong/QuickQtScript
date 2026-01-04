#include "jssyntaxhighlighter.h"
#include <QTextBlock>

//==============================================================================
//  JavaScript语法高亮器实现
//==============================================================================
JSSyntaxHighlighter::JSSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    qRegisterMetaType<QTextBlock>("QTextBlock");
    qRegisterMetaType<QTextCursor>("QTextCursor");

    HighlightingRule rule;

    // JavaScript关键字格式
    keywordFormat.setForeground(QColor(86, 156, 214)); // VS Code蓝色
    keywordFormat.setFontWeight(QFont::Bold);
    QStringList keywordPatterns;
    keywordPatterns << "\\bvar\\b" << "\\blet\\b" << "\\bconst\\b"
                    << "\\bfunction\\b" << "\\breturn\\b" << "\\bif\\b"
                    << "\\belse\\b" << "\\bfor\\b" << "\\bwhile\\b"
                    << "\\bdo\\b" << "\\bswitch\\b" << "\\bcase\\b"
                    << "\\bbreak\\b" << "\\bcontinue\\b" << "\\btry\\b"
                    << "\\bcatch\\b" << "\\bfinally\\b" << "\\bthrow\\b"
                    << "\\bnew\\b" << "\\bthis\\b" << "\\btrue\\b"
                    << "\\bfalse\\b" << "\\bnull\\b" << "\\bundefined\\b"
                    << "\\btypeof\\b" << "\\binstanceof\\b" << "\\bin\\b"
                    << "\\bof\\b" << "\\bclass\\b" << "\\bextends\\b"
                    << "\\bsuper\\b" << "\\bstatic\\b" << "\\basync\\b"
                    << "\\bawait\\b" << "\\byield\\b" << "\\bimport\\b"
                    << "\\bexport\\b" << "\\bfrom\\b" << "\\bdefault\\b";

    foreach (const QString &pattern, keywordPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    // 类名格式
    classFormat.setForeground(QColor(78, 201, 176)); // 青色
    classFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression("\\b[A-Z][a-zA-Z_0-9]*\\b");
    rule.format = classFormat;
    highlightingRules.append(rule);

    // 函数名格式
    functionFormat.setForeground(QColor(220, 220, 170)); // 淡黄色
    rule.pattern = QRegularExpression("\\b[a-zA-Z_][a-zA-Z_0-9]*(?=\\s*\\()");
    rule.format = functionFormat;
    highlightingRules.append(rule);

    // 数字格式
    numberFormat.setForeground(QColor(181, 206, 168)); // 淡绿色
    rule.pattern = QRegularExpression("\\b\\d+(\\.\\d+)?\\b");
    rule.format = numberFormat;
    highlightingRules.append(rule);

    // 运算符格式
    operatorFormat.setForeground(QColor(212, 212, 212)); // 浅灰色
    QStringList operatorPatterns;
    operatorPatterns << "\\+" << "\\-" << "\\*" << "/" << "%" << "="
                     << "==" << "===" << "!=" << "!==" << "<" << ">"
                     << "<=" << ">=" << "&&" << "\\|\\|" << "!" << "&"
                     << "\\|" << "\\^" << "~" << "<<" << ">>" << "\\+\\+"
                     << "\\-\\-" << "\\+=" << "\\-=" << "\\*=" << "/=" << "%=";
    
    foreach (const QString &pattern, operatorPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = operatorFormat;
        highlightingRules.append(rule);
    }

    // 字符串格式
    quotationFormat.setForeground(QColor(206, 145, 120)); // 橙色
    rule.pattern = QRegularExpression("\".*?\"|'.*?'|`.*?`");
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    // 单行注释格式
    singleLineCommentFormat.setForeground(QColor(106, 153, 85)); // 绿色
    singleLineCommentFormat.setFontItalic(true);
    rule.pattern = QRegularExpression("//[^\n]*");
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);

    // 多行注释格式
    multiLineCommentFormat.setForeground(QColor(106, 153, 85)); // 绿色
    multiLineCommentFormat.setFontItalic(true);

    // 正则表达式格式
    regExpressFormat.setForeground(QColor(245, 113, 37)); // 深橙色 // 要求 / 前面不能是标识符或数字（排除除法场景）
    rule.pattern = QRegularExpression(R"((?<![a-zA-Z0-9_$])\s*\/(?:[^\/\\\n]|\\[^\n]|\\\[(?:\\.|[^\]\n])*\\])+\/[gimuy]*)");
    rule.format = regExpressFormat;
    highlightingRules.append(rule);
}

void JSSyntaxHighlighter::highlightBlock(const QString &text)
{
    AnnotationUserData *data = dynamic_cast<AnnotationUserData *>(currentBlock().userData());
    if (data && data->isAnnotation()) {
        QTextCharFormat fmt;
        fmt.setForeground(QColor(200, 200, 200));
        fmt.setFontItalic(true);
        setFormat(0, text.length(), fmt);
        return;
    }

    // // 应用所有高亮规则
    // foreach (const HighlightingRule &rule, highlightingRules) {
    //     QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
    //     while (matchIterator.hasNext()) {
    //         QRegularExpressionMatch match = matchIterator.next();
    //         setFormat(match.capturedStart(), match.capturedLength(), rule.format);
    //     }
    // }

    // 先处理单行注释（记录位置，最后应用） // 不处理会跟正则表达式冲突 比如注释 //abc//edf 中的 /abc/ 会被识别成正则表达式
    QList<QPair<int, int>> commentRanges;
    QRegularExpression commentPat("//[^\n]*");
    for (auto it = commentPat.globalMatch(text); it.hasNext(); ) {
        auto m = it.next();
        commentRanges.append({m.capturedStart(), m.capturedLength()});
    }

    // 收集字符串/模板字面量范围 // 不处理会跟正则表达式冲突 比如字符串 './path/module.js' 中的 /path/m 会被识别成正则表达式
    QList<QPair<int, int>> stringRanges;
    QRegularExpression stringPat("\".*?\"|'.*?'|`.*?`");
    for (auto it = stringPat.globalMatch(text); it.hasNext(); ) {
        auto m = it.next();
        stringRanges.append({m.capturedStart(), m.capturedLength()});
    }

    // 应用除注释外的所有规则
    foreach (const auto &rule, highlightingRules) {
        // 跳过注释规则，避免重复
        if (rule.pattern.pattern() == "//[^\\n]*") continue;

        for (auto it = rule.pattern.globalMatch(text); it.hasNext(); ) {
            auto m = it.next();
            int start = m.capturedStart(), len = m.capturedLength();

            // 检查是否在注释范围内
            bool inComment = false;
            for (auto &c : commentRanges) {
                if (start >= c.first && start < c.first + c.second) {
                    inComment = true; break;
                }
            }
            if (inComment) continue;

            // 正则表达式规则跳过字符串范围
            if (rule.format == regExpressFormat) {
                bool inString = false;
                for (auto &s : stringRanges) {
                    if (start >= s.first && start < s.first + s.second) {
                        inString = true; break;
                    }
                }
                if (inString) continue;
            }
            setFormat(start, len, rule.format);
        }
    }

    // 最后应用注释格式（确保覆盖）
    for (auto &c : commentRanges) {
        setFormat(c.first, c.second, singleLineCommentFormat);
    }

    // 处理多行注释
    setCurrentBlockState(0);

    QRegularExpression startExpression("/\\*");
    QRegularExpression endExpression("\\*/");

    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = text.indexOf(startExpression);

    while (startIndex >= 0) {
        QRegularExpressionMatch match = endExpression.match(text, startIndex);
        int endIndex = match.capturedStart();
        int commentLength = 0;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + match.capturedLength();
        }
        setFormat(startIndex, commentLength, multiLineCommentFormat);
        startIndex = text.indexOf(startExpression, startIndex + commentLength);
    }
}
