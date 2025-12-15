#ifndef JSCODEEDITOR_H
#define JSCODEEDITOR_H

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QPainter>
#include <QTextBlock>
#include <QScrollBar>
#include <QCompleter>
#include <QAbstractItemModel>
#include <QStringListModel>
#include <QMenu>
#include <QContextMenuEvent>
#include <QSet>

#ifdef Q_OS_WIN
#pragma execution_character_set("utf-8")
#endif

QT_BEGIN_NAMESPACE
class QPaintEvent;
class QResizeEvent;
class QSize;
class QWidget;
class QTimer;
QT_END_NAMESPACE

class LineNumberArea;
class CodeFoldingArea;

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
};

//==============================================================================
//  代码编辑器主类
//==============================================================================
class JSCodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    JSCodeEditor(QWidget *parent = nullptr);

    // 在编辑停止后重映射断点和执行行（外部可访问）
    void remapBreakpointsAfterEdit();

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    void codeFoldingAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();
    int codeFoldingAreaWidth();

    // 断点相关
    void toggleBreakpoint(int lineNumber);
    bool hasBreakpoint(int lineNumber) const;
    QSet<int> getBreakpoints() const { return breakpoints; }
    void clearBreakpoints();

    // 代码折叠相关
    void toggleFold(int lineNumber);
    bool isLineVisible(int lineNumber) const;
    void foldBlock(int startLine, int endLine);
    void unfoldBlock(int startLine);
    
    // 代码折叠功能控制
    void setCodeFoldingEnabled(bool enabled);
    bool isCodeFoldingEnabled() const { return codeFoldingEnabled; }
    void clearAllFolds();  // 清除所有折叠
    
    // 代码执行箭头控制
    void setExecutionArrowEnabled(bool enabled);
    bool isExecutionArrowEnabled() const { return executionArrowEnabled; }
    void setCurrentExecutionLine(int lineNumber);
    int getCurrentExecutionLine() const { return currentExecutionLine; }
    void clearExecutionArrow();  // 清除执行箭头
    
    // 代码块检测方法（供外部组件使用）
    bool isBlockStart(const QString &text);
    bool isBlockEnd(const QString &text);
    
    // 访问protected成员的public方法
    QTextBlock getFirstVisibleBlock() const { return firstVisibleBlock(); }
    QRectF getBlockBoundingGeometry(const QTextBlock &block) const { return blockBoundingGeometry(block); }
    QRectF getBlockBoundingRect(const QTextBlock &block) const { return blockBoundingRect(block); }
    QPointF getContentOffset() const { return contentOffset(); }

signals:
    void breakPointsChanged();
    // 防抖后的内容变更信号
    void contentEditedDebounced();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();

private:
    void setupEditor();
    void setupSyntaxHighlighter();
    void setupCompleter();
    void setupContextMenu();
    
    // 查找匹配的大括号
    int findMatchingBrace(int position, bool forward = true);
    QTextBlock findBlockEnd(QTextBlock startBlock);
    QTextBlock findBlockStart(QTextBlock endBlock);
    
    // 计算行的缩进级别
    int getIndentLevel(const QString &text);
    
    // 获取光标下的单词
    QString textUnderCursor() const;

    LineNumberArea *lineNumberArea;
    CodeFoldingArea *codeFoldingArea;
    JSSyntaxHighlighter *syntaxHighlighter;
    QCompleter *completer;
    
    QSet<int> breakpoints;  // 断点集合
    QSet<int> foldedBlocks; // 折叠的代码块
    QMap<int, int> foldRanges; // 折叠范围映射 (startLine -> endLine)
    
    bool codeFoldingEnabled; // 代码折叠功能启用状态
    
    // 代码执行箭头相关
    bool executionArrowEnabled; // 执行箭头显示状态
    int currentExecutionLine;   // 当前执行行号（-1表示无）

    // 防抖和文本差异记录，用于在编辑后重映射断点和执行行
    QStringList prevTextLines;
    QTimer *editDebounceTimer{nullptr};

    
};

//==============================================================================
//  行号区域类
//==============================================================================
class LineNumberArea : public QWidget
{
public:
    LineNumberArea(JSCodeEditor *editor) : QWidget(editor), codeEditor(editor)
    {}

    QSize sizeHint() const override
    {
        return QSize(codeEditor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        codeEditor->lineNumberAreaPaintEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override;

private:
    JSCodeEditor *codeEditor;
};

//==============================================================================
//  代码折叠区域类
//==============================================================================
class CodeFoldingArea : public QWidget
{
public:
    CodeFoldingArea(JSCodeEditor *editor) : QWidget(editor), codeEditor(editor)
    {}

    QSize sizeHint() const override
    {
        return QSize(codeEditor->codeFoldingAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        codeEditor->codeFoldingAreaPaintEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override;

private:
    JSCodeEditor *codeEditor;
};

#endif // JSCODEEDITOR_H
