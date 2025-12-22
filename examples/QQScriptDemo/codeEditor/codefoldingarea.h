#ifndef CODEFOLDINGAREA_H
#define CODEFOLDINGAREA_H

#include <QWidget>

#ifdef Q_OS_WIN
#pragma execution_character_set("utf-8")
#endif

class JSCodeEditor;

//==============================================================================
//  代码折叠区域类
//==============================================================================
class CodeFoldingArea : public QWidget
{
public:
    CodeFoldingArea(JSCodeEditor *editor);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    JSCodeEditor *codeEditor;
};

#endif // CODEFOLDINGAREA_H
