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

    void on_pushButton_stepOver_clicked();

    void on_pushButton_stepIn_clicked();

    void on_pushButton_stepOut_clicked();

    void on_pushButton_continue_clicked();

    void on_pushButton_runToLine_clicked();

    // reset removed

    void on_pushButton_loadDefault_clicked();
    void on_pushButton_loadDebug_clicked();
    void on_pushButton_loadAsnyc_clicked();

private:
    QString defaultCode();
    QString debugCode();
    QString asnycCode();
    // 启动前先用一个临时引擎解析目标行，避免污染当前运行态
    QScriptEngine::RunToLineInfo resolveRunToLineRequest(int requestedLine);

private:
    Ui::MainWindow *ui;
    JSCodeEditor *codeEditor{nullptr};

    std::atomic_int stop_flag = 0;
    QPointer<QScriptEngine>       mEngine{nullptr};
    QPointer<MyScriptEngineAgent> mEngineAgent{nullptr};
    QMap<QString, QSet<int>> mBreakPoints;
    // 当前这次启动要使用的目标行，0 表示普通启动
    int mRequestedRunToLine{0};
    // 工作线程读取这次启动的脚本快照，避免 [&] 捕获局部变量后悬空
    QString mActiveScriptSource;
    int mActiveRequestedRunToLine{0};
    // 运行中再次点击 run-to-line 时，先记住请求，等 stop 完成后自动重启
    int mPendingRestartRunToLine{-1};
    bool mScriptThreadRunning{false};
    // 复用 start 槽时，避免入口处把外部刚写入的目标行清掉
    bool mStartUsesExistingRunToLineRequest{false};
};
#endif // MAINWINDOW_H
