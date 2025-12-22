#include "jscodeeditor.h"
#include <QPainter>
#include <QTextBlock>
#include <QScrollBar>
#include <QMenu>
#include <QAction>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QCompleter>
#include <QStringListModel>
#include <QAbstractItemView>
#include <QDebug>
#include <QTimer>

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

    // 应用所有高亮规则
    foreach (const HighlightingRule &rule, highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
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

//==============================================================================
//  代码编辑器主类实现
//==============================================================================
JSCodeEditor::JSCodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
    , lineNumberArea(new LineNumberArea(this))
    , codeFoldingArea(new CodeFoldingArea(this))
    , syntaxHighlighter(nullptr)
    , completer(nullptr)
    , codeFoldingEnabled(true)  // 默认启用代码折叠
    , executionArrowEnabled(false)  // 默认不显示执行箭头
    , currentExecutionLine(-1)  // 无执行行
{
    setupEditor();
    setupSyntaxHighlighter();
    setupCompleter();
    setupContextMenu();

    // 初始化防抖计时器与 prevTextLines
    editDebounceTimer = new QTimer(this);
    editDebounceTimer->setSingleShot(true);
    editDebounceTimer->setInterval(20); // 防抖
    connect(editDebounceTimer, &QTimer::timeout, this, [this]() {
        emit contentEditedDebounced();
    });
    prevTextLines = document()->toPlainText().split('\n');

    connect(this, &JSCodeEditor::blockCountChanged, this, &JSCodeEditor::updateLineNumberAreaWidth);
    connect(this, &JSCodeEditor::updateRequest, this, &JSCodeEditor::updateLineNumberArea);
    connect(this, &JSCodeEditor::cursorPositionChanged, this, &JSCodeEditor::highlightCurrentLine);

    // 监听文档变更，启动防抖计时器
    connect(document(), &QTextDocument::contentsChanged, this, [this]() {
        // 只有在编辑器可写时触发防抖（便于外部在执行时禁用编辑）
        if (!isReadOnly()) {
            editDebounceTimer->start();
        }
    });

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

// 在编辑停止（外部在非执行状态下调用）后重映射断点与执行行
void JSCodeEditor::remapBreakpointsAfterEdit()
{
    QStringList currLines = document()->toPlainText().split('\n');
    int oldCount = prevTextLines.size();
    int newCount = currLines.size();
    int minCount = qMin(oldCount, newCount);

    int firstDiff = 0;
    while (firstDiff < minCount && prevTextLines[firstDiff] == currLines[firstDiff]) {
        ++firstDiff;
    }

    if (firstDiff == minCount && oldCount == newCount) {
        // 无变化
        return;
    }

    int oldSuffix = oldCount - 1;
    int newSuffix = newCount - 1;
    while (oldSuffix >= firstDiff && newSuffix >= firstDiff && prevTextLines[oldSuffix] == currLines[newSuffix]) {
        --oldSuffix;
        --newSuffix;
    }

    int delta = newCount - oldCount; // 正为插入，负为删除

    // 更新断点：在修改范围内的断点移除，修改范围后面的断点根据 delta 平移
    QSet<int> newBps;
    for (int bp : breakpoints) {
        int idx = bp - 1; // zero-based
        if (idx < firstDiff) {
            newBps.insert(bp);
        } else if (idx > oldSuffix) {
            int newIdx = idx + delta;
            if (newIdx >= 0) newBps.insert(newIdx + 1);
        } else {
            // bp 在被修改区域内 — 丢弃该断点
        }
    }
    breakpoints = newBps;

    // 更新执行箭头位置：如果在修改区域则清除；在修改区域之后则平移
    if (currentExecutionLine > 0) {
        int execIdx = currentExecutionLine - 1;
        if (execIdx < firstDiff) {
            // unchanged
        } else if (execIdx > oldSuffix) {
            currentExecutionLine = execIdx + delta + 1;
        } else {
            // 执行行被编辑区域覆盖，清除
            currentExecutionLine = -1;
        }
    }

    // 更新 prevTextLines 为当前内容
    prevTextLines = currLines;

    // 更新显示并通知
    lineNumberArea->update();
    viewport()->update();
    emit breakPointsChanged();
}

void JSCodeEditor::setupEditor()
{
    // 设置字体
    QFont font("Consolas", 11);
    font.setFixedPitch(true);
    setFont(font);

    // 设置制表符宽度
    QFontMetrics metrics(font);
    setTabStopWidth(4 * metrics.width(' '));

    // 设置背景色和文字颜色
    QPalette palette = this->palette();
    palette.setColor(QPalette::Base, QColor(30, 30, 30)); // 深色背景
    palette.setColor(QPalette::Text, QColor(212, 212, 212)); // 浅色文字
    setPalette(palette);

    // 设置行间距
    setLineWrapMode(QPlainTextEdit::NoWrap);
    
    // 启用拖放
    setAcceptDrops(true);
}

void JSCodeEditor::setupSyntaxHighlighter()
{
    syntaxHighlighter = new JSSyntaxHighlighter(document());
}

void JSCodeEditor::setupCompleter()
{
    // JavaScript关键字和内置对象列表
    QStringList wordList;
    wordList << "var" << "let" << "const" << "function" << "return" << "if"
             << "else" << "for" << "while" << "do" << "switch" << "case"
             << "break" << "continue" << "try" << "catch" << "finally"
             << "throw" << "new" << "this" << "true" << "false" << "null"
             << "undefined" << "typeof" << "instanceof" << "in" << "of"
             << "class" << "extends" << "super" << "static" << "async"
             << "await" << "yield" << "import" << "export" << "from"
             << "default" << "console" << "document" << "window"
             << "Array" << "Object" << "String" << "Number" << "Boolean"
             << "Date" << "RegExp" << "Math" << "JSON" << "setTimeout"
             << "setInterval" << "clearTimeout" << "clearInterval"
             << "addEventListener" << "removeEventListener" << "querySelector"
             << "querySelectorAll" << "getElementById" << "getElementsByClassName";

    completer = new QCompleter(wordList, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setWrapAround(false);
    completer->setWidget(this);
    completer->setCompletionMode(QCompleter::PopupCompletion);

    connect(completer, QOverload<const QString &>::of(&QCompleter::activated),
            this, [this](const QString &text) {
                QTextCursor cursor = textCursor();
                int extra = text.length() - completer->completionPrefix().length();
                cursor.movePosition(QTextCursor::Left);
                cursor.movePosition(QTextCursor::EndOfWord);
                cursor.insertText(text.right(extra));
                setTextCursor(cursor);
            });
}

void JSCodeEditor::setupContextMenu()
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
                QContextMenuEvent event(QContextMenuEvent::Mouse, pos);
                contextMenuEvent(&event);
            });
}

void JSCodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), QColor(40, 40, 40)); // 深灰色背景

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    // Calculate logical line number for the first visible block
    int logicalLineNumber = 1;
    QTextBlock b = document()->firstBlock();
    while (b.isValid() && b != block) {
        AnnotationUserData *data = dynamic_cast<AnnotationUserData *>(b.userData());
        if (!data || !data->isAnnotation()) {
            logicalLineNumber++;
        }
        b = b.next();
    }

    // 设置行号字体颜色
    painter.setPen(QColor(128, 128, 128));
    painter.setFont(font());

    while (block.isValid() && top <= event->rect().bottom()) {
        AnnotationUserData *data = dynamic_cast<AnnotationUserData *>(block.userData());
        bool isAnnotation = (data && data->isAnnotation());

        if (block.isVisible() && bottom >= event->rect().top()) {
            if (!isAnnotation) {
                QString number = QString::number(logicalLineNumber);
                int lineNum = logicalLineNumber;
                
                // 绘制执行箭头（优先级最高，在最左侧）
                if (executionArrowEnabled && currentExecutionLine == lineNum) {
                    painter.save();
                    painter.setBrush(QColor(255, 255, 0)); // 黄色箭头
                    painter.setPen(QColor(255, 255, 0));
                    
                    // 绘制右向箭头
                    QPolygon arrow;
                    int arrowY = top + fontMetrics().height() / 2;
                    arrow << QPoint(2, arrowY - 5)
                          << QPoint(12, arrowY)
                          << QPoint(2, arrowY + 5);
                    painter.drawPolygon(arrow);
                    painter.restore();
                    
                    // 高亮当前执行行背景
                    painter.fillRect(QRect(0, top, lineNumberArea->width(), 
                                         fontMetrics().height()), QColor(255, 255, 0, 50));
                }
                
                // 绘制断点
                if (hasBreakpoint(lineNum)) {
                    painter.fillRect(QRect(0, top, lineNumberArea->width() - 5, 
                                         fontMetrics().height()), QColor(255, 0, 0, 100));
                    painter.setBrush(QColor(255, 0, 0));
                    painter.drawEllipse(QRect(lineNumberArea->width() - 15, top + 2, 10, 10));
                }
                
                // 绘制行号
                painter.setPen(QColor(128, 128, 128));
                painter.drawText(0, top, lineNumberArea->width() - 20, 
                               fontMetrics().height(), Qt::AlignRight, number);
            }
        }

        if (!isAnnotation) {
            logicalLineNumber++;
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void JSCodeEditor::codeFoldingAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(codeFoldingArea);
    painter.fillRect(event->rect(), QColor(45, 45, 45)); // 深灰色背景

    // 如果代码折叠功能未启用，只绘制背景
    if (!codeFoldingEnabled) {
        return;
    }

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    painter.setPen(QColor(128, 128, 128));

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString text = block.text().trimmed();
            
            // 检查是否是可折叠的代码块开始
            if (isBlockStart(text)) {
                QRect foldRect(5, top + 2, 12, 12);
                painter.drawRect(foldRect);
                
                // 根据折叠状态绘制 + 或 -
                int lineNum = blockNumber + 1;
                if (foldedBlocks.contains(lineNum)) {
                    // 已折叠，显示 + 号
                    painter.drawLine(foldRect.center().x() - 3, foldRect.center().y(),
                                   foldRect.center().x() + 3, foldRect.center().y());
                    painter.drawLine(foldRect.center().x(), foldRect.center().y() - 3,
                                   foldRect.center().x(), foldRect.center().y() + 3);
                } else {
                    // 未折叠，显示 - 号
                    painter.drawLine(foldRect.center().x() - 3, foldRect.center().y(),
                                   foldRect.center().x() + 3, foldRect.center().y());
                }
            }
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

int JSCodeEditor::lineNumberAreaWidth()
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int space = 20 + fontMetrics().width(QLatin1Char('9')) * digits;
    return space;
}

int JSCodeEditor::codeFoldingAreaWidth()
{
    return codeFoldingEnabled ? 20 : 0;
}

void JSCodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth() + codeFoldingAreaWidth(), 0, 0, 0);
}

void JSCodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy) {
        lineNumberArea->scroll(0, dy);
        codeFoldingArea->scroll(0, dy);
    } else {
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());
        codeFoldingArea->update(0, rect.y(), codeFoldingArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void JSCodeEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    int lineNumberWidth = lineNumberAreaWidth();
    int codeFoldingWidth = codeFoldingAreaWidth();
    
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberWidth, cr.height()));
    codeFoldingArea->setGeometry(QRect(cr.left() + lineNumberWidth, cr.top(), 
                                      codeFoldingWidth, cr.height()));
}

void JSCodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        QColor lineColor = QColor(50, 50, 50); // 深灰色高亮
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    // Add annotation selections
    QTextBlock block = document()->firstBlock();
    while (block.isValid()) {
        AnnotationUserData *data = dynamic_cast<AnnotationUserData *>(block.userData());
        if (data && data->isAnnotation()) {
            QTextEdit::ExtraSelection selection;
            selection.format.setBackground(QColor(60, 60, 60)); // Annotation background
            selection.format.setProperty(QTextFormat::FullWidthSelection, true);
            selection.cursor = QTextCursor(block);
            selection.cursor.clearSelection();
            extraSelections.append(selection);
        }
        block = block.next();
    }

    setExtraSelections(extraSelections);
}

void JSCodeEditor::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    
    // 基本编辑操作
    menu.addAction("撤销", this, &QPlainTextEdit::undo);
    menu.addAction("重做", this, &QPlainTextEdit::redo);
    menu.addSeparator();
    menu.addAction("剪切", this, &QPlainTextEdit::cut);
    menu.addAction("复制", this, &QPlainTextEdit::copy);
    menu.addAction("粘贴", this, &QPlainTextEdit::paste);
    menu.addSeparator();
    
    // 断点操作 - 改进行号获取方式
    int lineNumber = -1;
    
    // 方法1：优先使用当前光标位置
    if (textCursor().hasSelection() || textCursor().position() >= 0) {
        lineNumber = textCursor().blockNumber() + 1;
    }
    
    // 方法2：如果没有有效的光标位置，则使用鼠标位置计算
    if (lineNumber <= 0) {
        QTextCursor cursor = cursorForPosition(event->pos());
        if (!cursor.isNull() && cursor.position() >= 0) {
            lineNumber = cursor.blockNumber() + 1;
        }
    }
    
    // 方法3：如果还是无效，使用更精确的位置计算
    if (lineNumber <= 0) {
        QPoint localPos = event->pos();
        QTextBlock block = getFirstVisibleBlock();
        int top = qRound(getBlockBoundingGeometry(block).translated(getContentOffset()).top());
        int bottom = top + qRound(getBlockBoundingRect(block).height());
        int blockNumber = block.blockNumber();
        
        while (block.isValid()) {
            if (top <= localPos.y() && localPos.y() < bottom && block.isVisible()) {
                lineNumber = blockNumber + 1;
                break;
            }
            block = block.next();
            top = bottom;
            bottom = top + qRound(getBlockBoundingRect(block).height());
            ++blockNumber;
        }
    }
    
    // 如果获取到了有效的行号，添加断点菜单
    if (lineNumber > 0) {
        if (hasBreakpoint(lineNumber)) {
            menu.addAction(QString("删除第%1行的断点").arg(lineNumber), [this, lineNumber]() {
                toggleBreakpoint(lineNumber);
            });
        } else {
            menu.addAction(QString("在第%1行插入断点").arg(lineNumber), [this, lineNumber]() {
                toggleBreakpoint(lineNumber);
            });
        }
        
        menu.addSeparator();
    }
    
    menu.addAction("清除所有断点", this, &JSCodeEditor::clearBreakpoints);
    
    menu.exec(event->globalPos());
}

void JSCodeEditor::inputMethodEvent(QInputMethodEvent *event)
{
    AnnotationUserData *data = dynamic_cast<AnnotationUserData *>(textCursor().block().userData());
    if (data && data->isAnnotation()) {
        return;
    }
    QPlainTextEdit::inputMethodEvent(event);
}

void JSCodeEditor::keyPressEvent(QKeyEvent *event)
{
    // Annotation protection logic
    auto isAnnotation = [](const QTextBlock &block) {
        if (!block.isValid()) return false;
        AnnotationUserData *data = dynamic_cast<AnnotationUserData *>(block.userData());
        return data && data->isAnnotation();
    };

    QTextCursor cursor = textCursor();

    // 1. Check if selection overlaps with any annotation block
    if (cursor.hasSelection()) {
        bool isNavigation = 
            event->key() == Qt::Key_Up || event->key() == Qt::Key_Down ||
            event->key() == Qt::Key_Left || event->key() == Qt::Key_Right ||
            event->key() == Qt::Key_PageUp || event->key() == Qt::Key_PageDown ||
            event->key() == Qt::Key_Home || event->key() == Qt::Key_End;

        bool isEdit = !isNavigation && 
                      (!event->text().isEmpty() && (event->modifiers() & Qt::ControlModifier) == 0); 
        
        if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete || 
            event->matches(QKeySequence::Cut) || event->matches(QKeySequence::Paste)) {
            isEdit = true;
        }
        
        if (isEdit) {
            int start = cursor.selectionStart();
            int end = cursor.selectionEnd();
            QTextBlock b = document()->findBlock(start);
            while (b.isValid() && b.position() < end) {
                if (isAnnotation(b)) {
                    return; // Prevent editing if selection contains annotation
                }
                b = b.next();
            }
        }
    }

    // 2. Check if cursor is inside an annotation block
    if (isAnnotation(cursor.block())) {
        bool isNavigation = 
            event->key() == Qt::Key_Up || event->key() == Qt::Key_Down ||
            event->key() == Qt::Key_Left || event->key() == Qt::Key_Right ||
            event->key() == Qt::Key_PageUp || event->key() == Qt::Key_PageDown ||
            event->key() == Qt::Key_Home || event->key() == Qt::Key_End;
            
        if (isNavigation || event->matches(QKeySequence::Copy)) {
            QPlainTextEdit::keyPressEvent(event);
            return;
        }
        return; // Block everything else
    }

    // 3. Check boundary conditions (Backspace/Delete merging blocks)
    if (event->key() == Qt::Key_Backspace && cursor.atBlockStart() && !cursor.hasSelection()) {
        if (isAnnotation(cursor.block().previous())) {
            return;
        }
    }
    if (event->key() == Qt::Key_Delete && cursor.atBlockEnd() && !cursor.hasSelection()) {
        if (isAnnotation(cursor.block().next())) {
            return;
        }
    }

    // 自动补全处理
    if (completer && completer->popup()->isVisible()) {
        switch (event->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            event->ignore();
            return;
        default:
            break;
        }
    }

    // 自动缩进
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QTextCursor cursor = textCursor();
        QString currentLine = cursor.block().text();
        QString indent = currentLine.left(currentLine.indexOf(QRegularExpression("[^\\s]")));
        
        QPlainTextEdit::keyPressEvent(event);
        
        // 如果当前行以 { 结尾，增加缩进
        if (currentLine.trimmed().endsWith("{")) {
            indent += "    ";
        }
        
        insertPlainText(indent);
        return;
    }

    // 自动闭合大括号
    if (event->text() == "{") {
        QPlainTextEdit::keyPressEvent(event);
        QTextCursor cursor = textCursor();
        cursor.insertText("}");
        cursor.movePosition(QTextCursor::Left);
        setTextCursor(cursor);
        return;
    }

    QPlainTextEdit::keyPressEvent(event);

    // 触发自动补全
    const bool isShortcut = ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_E);
    if (!completer || !isShortcut)
        return;

    static QString eow("~!@#$%^&*()_+{}|:\"<>?,./;'[]\\-=");
    const bool hasModifier = (event->modifiers() != Qt::NoModifier) && !isShortcut;
    QString completionPrefix = textUnderCursor();

    if (!isShortcut && (hasModifier || event->text().isEmpty()|| completionPrefix.length() < 2
                      || eow.contains(event->text().right(1)))) {
        completer->popup()->hide();
        return;
    }

    if (completionPrefix != completer->completionPrefix()) {
        completer->setCompletionPrefix(completionPrefix);
        completer->popup()->setCurrentIndex(completer->completionModel()->index(0, 0));
    }
    QRect cr = cursorRect();
    cr.setWidth(completer->popup()->sizeHintForColumn(0)
                + completer->popup()->verticalScrollBar()->sizeHint().width());
    completer->complete(cr);
}

// 断点相关方法
void JSCodeEditor::toggleBreakpoint(int lineNumber)
{
    if (breakpoints.contains(lineNumber)) {
        breakpoints.remove(lineNumber);
    } else {
        breakpoints.insert(lineNumber);
    }
    lineNumberArea->update();

    emit breakPointsChanged();
}

bool JSCodeEditor::hasBreakpoint(int lineNumber) const
{
    return breakpoints.contains(lineNumber);
}

void JSCodeEditor::clearBreakpoints()
{
    breakpoints.clear();
    lineNumberArea->update();
}

// 代码折叠相关方法
void JSCodeEditor::toggleFold(int lineNumber)
{
    // 检查代码折叠功能是否启用
    if (!codeFoldingEnabled) {
        return;
    }
    
    // 检查是否已经折叠
    bool isCurrentlyFolded = foldedBlocks.contains(lineNumber);
    
    if (isCurrentlyFolded) {
        // 展开代码块
        unfoldBlock(lineNumber);
    } else {
        // 折叠代码块
        QTextBlock block = blockByNumber(lineNumber - 1);
        if (block.isValid() && isBlockStart(block.text().trimmed())) {
            QTextBlock endBlock = findBlockEnd(block);
            if (endBlock.isValid()) {
                int endLineNumber = endBlock.blockNumber() + 1;
                foldBlock(lineNumber, endLineNumber);
            }
        }
    }
}

// 代码折叠功能控制
void JSCodeEditor::setCodeFoldingEnabled(bool enabled)
{
    codeFoldingEnabled = enabled;
    
    if (!enabled) {
        // 禁用时清除所有折叠
        clearAllFolds();
    }
    
    // 更新代码折叠区域显示
    codeFoldingArea->setVisible(enabled);
    updateLineNumberAreaWidth(0);
}

void JSCodeEditor::clearAllFolds()
{
    // 展开所有折叠的代码块
    QSet<int> foldsToUnfold = foldedBlocks; // 创建副本避免迭代时修改
    for (int startLine : foldsToUnfold) {
        unfoldBlock(startLine);
    }
    
    // 确保所有状态都被清除
    foldedBlocks.clear();
    foldRanges.clear();
    
    // 显示所有行
    QTextBlock block = document()->firstBlock();
    while (block.isValid()) {
        block.setVisible(true);
        block = block.next();
    }
    
    // 更新界面
    lineNumberArea->update();
    codeFoldingArea->update();
    viewport()->update();
    updateLineNumberAreaWidth(0);
}

// 代码执行箭头控制
void JSCodeEditor::setExecutionArrowEnabled(bool enabled)
{
    executionArrowEnabled = enabled;
    
    if (!enabled) {
        // 禁用时清除当前执行行
        currentExecutionLine = -1;
    }
    
    // 更新行号区域显示
    lineNumberArea->update();
}

void JSCodeEditor::setCurrentExecutionLine(int lineNumber)
{
    // 只在启用执行箭头时才设置
    if (!executionArrowEnabled) {
        return;
    }
    
    int previousLine = currentExecutionLine;
    currentExecutionLine = lineNumber;
    
    // 更新行号区域显示
    lineNumberArea->update();
    
    // 如果设置了有效的执行行，滚动到该行
    if (lineNumber > 0) {
        QTextBlock block = blockByNumber(lineNumber - 1);
        if (block.isValid()) {
            QTextCursor cursor(block);
            setTextCursor(cursor);
            centerCursor(); // 将执行行居中显示
        }
    }
}

void JSCodeEditor::clearExecutionArrow()
{
    currentExecutionLine = -1;
    lineNumberArea->update();
}

bool JSCodeEditor::isLineVisible(int lineNumber) const
{
    // 检查该行是否在任何折叠的代码块中
    for (auto it = foldRanges.constBegin(); it != foldRanges.constEnd(); ++it) {
        if (lineNumber > it.key() && lineNumber <= it.value()) {
            return false;
        }
    }
    return true;
}

void JSCodeEditor::foldBlock(int startLine, int endLine)
{
    // 添加到折叠块集合
    foldedBlocks.insert(startLine);
    foldRanges[startLine] = endLine;
    // 隐藏折叠区域内的行
    for (int i = startLine + 1; i <= endLine; ++i) {
        QTextBlock block = blockByNumber(i - 1);
        if (block.isValid()) {
            block.setVisible(false);
        }
    }
    
    // 更新界面
    lineNumberArea->update();
    codeFoldingArea->update();
    viewport()->update();
    document()->markContentsDirty(0, document()->characterCount());
    
    // 强制重新布局
    updateLineNumberAreaWidth(0);
}

void JSCodeEditor::unfoldBlock(int startLine)
{
    // 从折叠块集合中移除
    foldedBlocks.remove(startLine);
    int endLine = foldRanges.value(startLine, startLine);
    foldRanges.remove(startLine);
    // 显示折叠区域内的行
    for (int i = startLine + 1; i <= endLine; ++i) {
        QTextBlock block = blockByNumber(i - 1);
        if (block.isValid()) {
            block.setVisible(true);
        }
    }
    
    // 更新界面
    lineNumberArea->update();
    codeFoldingArea->update();
    viewport()->update();
    document()->markContentsDirty(0, document()->characterCount());
    
    // 强制重新布局
    updateLineNumberAreaWidth(0);
}

// 工具方法
int JSCodeEditor::getIndentLevel(const QString &text)
{
    int level = 0;
    for (const QChar &ch : text) {
        if (ch == ' ') {
            level++;
        } else if (ch == '\t') {
            level += 4;
        } else {
            break;
        }
    }
    return level / 4;
}

bool JSCodeEditor::isBlockStart(const QString &text)
{
    QString trimmed = text.trimmed();
    return trimmed.endsWith("{") || trimmed.contains(QRegularExpression("^\\s*(if|for|while|function|class)\\b.*\\{\\s*$"));
}

bool JSCodeEditor::isBlockEnd(const QString &text)
{
    QString trimmed = text.trimmed();
    return trimmed.startsWith("}");
}

QTextBlock JSCodeEditor::findBlockEnd(QTextBlock startBlock)
{
    QString startText = startBlock.text();
    if (!isBlockStart(startText)) {
        return QTextBlock();
    }
    
    int braceCount = 1;
    QTextBlock block = startBlock.next();
    
    while (block.isValid() && braceCount > 0) {
        QString text = block.text();
        braceCount += text.count('{');
        braceCount -= text.count('}');
        
        if (braceCount == 0) {
            return block;
        }
        
        block = block.next();
    }
    
    return QTextBlock();
}

QTextBlock JSCodeEditor::findBlockStart(QTextBlock endBlock)
{
    QString endText = endBlock.text();
    if (!isBlockEnd(endText)) {
        return QTextBlock();
    }
    
    int braceCount = 1;
    QTextBlock block = endBlock.previous();
    
    while (block.isValid() && braceCount > 0) {
        QString text = block.text();
        braceCount += text.count('}');
        braceCount -= text.count('{');
        
        if (braceCount == 0) {
            return block;
        }
        
        block = block.previous();
    }
    
    return QTextBlock();
}

QTextBlock JSCodeEditor::blockByNumber(int number) const
{
    if (number < 0) return QTextBlock();
    QTextBlock block = document()->firstBlock();
    while (block.isValid()) {
        if (block.blockNumber() == number)
            return block;
        block = block.next();
    }
    return QTextBlock();
}

QString JSCodeEditor::textUnderCursor() const
{
    QTextCursor tc = textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    return tc.selectedText();
}

//==============================================================================
//  行号区域鼠标事件处理
//==============================================================================
void LineNumberArea::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QTextCursor cursor = codeEditor->cursorForPosition(
            QPoint(0, event->pos().y()));
        int lineNumber = cursor.blockNumber() + 1;
        codeEditor->toggleBreakpoint(lineNumber);
    }
    QWidget::mousePressEvent(event);
}

//==============================================================================
//  代码折叠区域鼠标事件处理
//==============================================================================
void CodeFoldingArea::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 更准确的计算行号
        int y = event->pos().y();
        QTextBlock block = codeEditor->getFirstVisibleBlock();
        int top = qRound(codeEditor->getBlockBoundingGeometry(block).translated(codeEditor->getContentOffset()).top());
        int bottom = top + qRound(codeEditor->getBlockBoundingRect(block).height());
        int blockNumber = block.blockNumber();
        
        // 找到被点击的行
        while (block.isValid()) {
            if (top <= y && y < bottom && block.isVisible()) {
                int lineNumber = blockNumber + 1;
                QString text = block.text().trimmed();
                
                // 检查是否是可折叠的代码块开始
                if (codeEditor->isBlockStart(text)) {
                    codeEditor->toggleFold(lineNumber);
                }
                break;
            }
            
            block = block.next();
            top = bottom;
            bottom = top + qRound(codeEditor->getBlockBoundingRect(block).height());
            ++blockNumber;
        }
    }
    QWidget::mousePressEvent(event);
}

void JSCodeEditor::addAnnotation(int lineNumber, const QString &text)
{
    QTextBlock block = document()->firstBlock();
    int logicalLine = 1;
    while (block.isValid()) {
        AnnotationUserData *data = dynamic_cast<AnnotationUserData *>(block.userData());
        if (!data || !data->isAnnotation()) {
            if (logicalLine == lineNumber) {
                break;
            }
            logicalLine++;
        }
        block = block.next();
    }
    
    if (!block.isValid()) return;
    
    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::EndOfBlock);
    cursor.insertBlock();
    cursor.insertText(text);
    
    QTextBlock annotationBlock = cursor.block();
    annotationBlock.setUserData(new AnnotationUserData(true));
    
    QTextCursor annotationCursor(annotationBlock);
    annotationCursor.select(QTextCursor::BlockUnderCursor);
    QTextCharFormat fmt;
    fmt.setForeground(QColor(200, 200, 200)); 
    fmt.setFontItalic(true);
    annotationCursor.setCharFormat(fmt);
    
    highlightCurrentLine(); // Update extra selections
}

void JSCodeEditor::removeAnnotation(int lineNumber)
{
    QTextBlock block = document()->firstBlock();
    int logicalLine = 1;
    while (block.isValid()) {
        AnnotationUserData *data = dynamic_cast<AnnotationUserData *>(block.userData());
        if (!data || !data->isAnnotation()) {
            if (logicalLine == lineNumber) {
                break;
            }
            logicalLine++;
        }
        block = block.next();
    }
    
    if (!block.isValid()) return;
    
    QTextBlock nextBlock = block.next();
    while (nextBlock.isValid()) {
        AnnotationUserData *data = dynamic_cast<AnnotationUserData *>(nextBlock.userData());
        if (data && data->isAnnotation()) {
            QTextCursor cursor(nextBlock);
            cursor.select(QTextCursor::BlockUnderCursor);
            cursor.removeSelectedText();
            cursor.deletePreviousChar(); 
            nextBlock = block.next(); 
        } else {
            break; 
        }
    }
    highlightCurrentLine();
}

void JSCodeEditor::clearAnnotations()
{
    QTextBlock block = document()->firstBlock();
    while (block.isValid()) {
        AnnotationUserData *data = dynamic_cast<AnnotationUserData *>(block.userData());
        if (data && data->isAnnotation()) {
            QTextBlock next = block.next();
            QTextCursor cursor(block);
            cursor.select(QTextCursor::BlockUnderCursor);
            cursor.removeSelectedText();
            cursor.deletePreviousChar();
            block = next;
        } else {
            block = block.next();
        }
    }
    highlightCurrentLine();
}

void JSCodeEditor::paintEvent(QPaintEvent *e)
{
    QPlainTextEdit::paintEvent(e);

    QPainter painter(viewport());
    QTextBlock block = firstVisibleBlock();
    QPointF offset = contentOffset();
    
    while (block.isValid()) {
        AnnotationUserData *data = dynamic_cast<AnnotationUserData *>(block.userData());
        if (data && data->isAnnotation()) {
            QRectF r = blockBoundingGeometry(block).translated(offset);
            if (r.bottom() >= e->rect().top() && r.top() <= e->rect().bottom()) {
                painter.setPen(QColor(100, 100, 255)); // Blue border
                painter.drawRect(r.adjusted(0, 0, -1, -1));
            }
        }
        block = block.next();
        if (blockBoundingGeometry(block).translated(offset).top() > e->rect().bottom())
            break;
    }
}

