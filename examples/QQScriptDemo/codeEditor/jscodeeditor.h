#ifndef JSCODEEDITOR_H
#define JSCODEEDITOR_H

#include <QPlainTextEdit>
#include <QTextDocument>
#include <QRegularExpression>
#include <QPainter>
#include <QTextBlock>
#include <QScrollBar>
#include <QCompleter>
#include <QAbstractItemModel>
#include <QStringListModel>
#include <QMenu>
#include <QContextMenuEvent>
#include <QSet>

#include "jssyntaxhighlighter.h"
#include "linenumberarea.h"
#include "codefoldingarea.h"

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
    
    // 注释框相关
    void addAnnotation(int lineNumber, const QString &text);
    void removeAnnotation(int lineNumber);
    void clearAnnotations();
    
    // 获取源代码（不包含annotation）
    QString getSourceCode() const;

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
    
    // 根据y坐标获取逻辑行号（排除annotation blocks）
    int getLogicalLineNumberAtY(int y) const;

signals:
    void breakPointsChanged();

    // 防抖后的内容变更信号
    void contentEditedDebounced();

protected:
    void paintEvent(QPaintEvent *event) override;

    // 在此函数中，对代码行页面、折叠控件页面进行整理，将其嵌入到当前画面中
    // 此页面的左侧已经通过 setViewportMargins的方式空了出来
    void resizeEvent(QResizeEvent *event) override;

    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;

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
    QTextBlock findBlockEnd(QTextBlock startBlock);
    QTextBlock findBlockStart(QTextBlock endBlock);
    // 根据绝对块号查找 QTextBlock（忽略可见性）
    QTextBlock blockByNumber(int number) const;
    
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

#endif // JSCODEEDITOR_H
