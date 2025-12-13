#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include <QScriptEngine>
#include <QPointer>

#include "codeEditor/jscodeeditor.h"
#include "myscriptengineagent.h"


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void handleLog(QString info);

    int stopFlagValue();

private slots:
    void on_pushButton_start_clicked();

    void on_pushButton_stop_clicked();

private:
    QString defaultCode();

private:
    Ui::MainWindow *ui;
    JSCodeEditor *codeEditor{nullptr};

    std::atomic_int stop_flag = 0;
    QPointer<QScriptEngine>       mEngine{nullptr};
    QPointer<MyScriptEngineAgent> mEngineAgent{nullptr};
};
#endif // MAINWINDOW_H
