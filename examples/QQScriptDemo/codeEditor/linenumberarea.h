#ifndef LINENUMBERAREA_H
#define LINENUMBERAREA_H

#include <QWidget>

#ifdef Q_OS_WIN
#pragma execution_character_set("utf-8")
#endif

class JSCodeEditor;

//==============================================================================
//  行号区域类
//==============================================================================
class LineNumberArea : public QWidget
{
public:
    LineNumberArea(JSCodeEditor *editor);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    JSCodeEditor *codeEditor;
};

#endif // LINENUMBERAREA_H
