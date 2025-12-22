#include "codefoldingarea.h"
#include "jscodeeditor.h"
#include <QMouseEvent>

//==============================================================================
//  代码折叠区域实现
//==============================================================================
CodeFoldingArea::CodeFoldingArea(JSCodeEditor *editor)
    : QWidget(editor)
    , codeEditor(editor)
{
}

QSize CodeFoldingArea::sizeHint() const
{
    return QSize(codeEditor->codeFoldingAreaWidth(), 0);
}

void CodeFoldingArea::paintEvent(QPaintEvent *event)
{
    codeEditor->codeFoldingAreaPaintEvent(event);
}

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
