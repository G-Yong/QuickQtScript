#include "linenumberarea.h"
#include "jscodeeditor.h"
#include <QMouseEvent>

//==============================================================================
//  行号区域实现
//==============================================================================
LineNumberArea::LineNumberArea(JSCodeEditor *editor)
    : QWidget(editor)
    , codeEditor(editor)
{
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(codeEditor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    codeEditor->lineNumberAreaPaintEvent(event);
}

void LineNumberArea::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 使用逻辑行号（排除 annotation blocks）
        int lineNumber = codeEditor->getLogicalLineNumberAtY(event->pos().y());
        if (lineNumber > 0) {
            codeEditor->toggleBreakpoint(lineNumber);
        }
    }
    QWidget::mousePressEvent(event);
}
