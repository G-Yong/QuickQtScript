#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QtConcurrentRun>
#include <QDateTime>
#include <QScriptValueIterator>
#include "codeEditor/jscodeeditor.h"

#include "myscriptengineagent.h"

#include "myqobject.h"

#ifdef Q_OS_WIN
#pragma execution_character_set("utf-8")
#endif

QScriptValue funcLog(QScriptContext *context, QScriptEngine *engine, void *data);
QScriptValue funcSleep(QScriptContext *context, QScriptEngine *engine, void *data);

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->pushButton_start->setVisible(true);
    ui->pushButton_stop->setVisible(false);
    // 调试功能还没实现，先隐藏按钮
    ui->pushButton_stepIn->setVisible(false);
    ui->pushButton_stepOut->setVisible(false);
    ui->pushButton_stepOver->setVisible(false);

    codeEditor = new JSCodeEditor();
    codeEditor->setCodeFoldingEnabled(false); // 代码折叠功能还有大问题，先禁用
    codeEditor->setExecutionArrowEnabled(true); // 显示当箭头
    QHBoxLayout *codeEditorLayout = new QHBoxLayout();
    codeEditorLayout->setMargin(0);
    codeEditorLayout->addWidget(codeEditor);
    ui->widget->setLayout(codeEditorLayout);

    codeEditor->setPlainText(defaultCode());
}

MainWindow::~MainWindow()
{
    if(mEngine)
    {
        mEngine->abortEvaluation();
    }
    delete ui;
}

void MainWindow::handleLog(QString info)
{
    auto posInfo = mEngineAgent->currentPos();
    QString finalStr = QString("[%1]%2 ")
                           .arg(QDateTime::currentDateTime().time().toString("hh:mm:ss.zzz"))
                           .arg(posInfo.file + ":" + QString::number(posInfo.line) + ":" + QString::number(posInfo.column));
    finalStr += info;

    QMetaObject::invokeMethod(this, [=](){
        ui->plainTextEdit->appendPlainText(finalStr);
    }, Qt::QueuedConnection);
}

int MainWindow::stopFlagValue()
{
   return std::atomic_load(&stop_flag);
}

void MainWindow::on_pushButton_start_clicked()
{
    // ▶️ ⏸️
    if(mEngine.isNull())
    {
        std::atomic_store(&stop_flag, 0);

        ui->pushButton_start->setVisible(false);
        ui->pushButton_stop->setVisible(true);
        ui->plainTextEdit->clear();
        codeEditor->clearExecutionArrow();

        // 启动
        auto functor = [&](){
            QScriptEngine engine;
            MyScriptEngineAgent engineAgent(&engine);
            engine.setAgent(&engineAgent);

            mEngine      = &engine;
            mEngineAgent = &engineAgent;
            connect(mEngineAgent, &MyScriptEngineAgent::posChanged, this, [=](PosInfo info){
                codeEditor->setCurrentExecutionLine(info.line);
            }, Qt::QueuedConnection);

            // // 测试QObject
            // // 存在重大问题，暂时不要使用此功能
            // MyQObject qObj;
            // qObj.setObjectName("this is qObj");
            // qDebug() << "obj name:" << qObj.objectName();
            // auto jsQObj = engine.newQObject(&qObj);
            // engine.globalObject().setProperty("qObj", jsQObj);

            // console
            QScriptValue console = engine.newObject();
            console.setProperty("log", engine.newFunction(funcLog, this));
            engine.globalObject().setProperty("console", console);

            // sleep
            engine.globalObject().setProperty("sleep", engine.newFunction(funcSleep, this));

            QString scriptStr = codeEditor->toPlainText();

            // auto chkRet = engine.checkSyntax(scriptStr);
            // qDebug() << "result:"
            //          << chkRet.errorLineNumber()
            //          << chkRet.errorColumnNumber()
            //          << chkRet.errorMessage();
            // return;

            QScriptValue result;
            result = engine.evaluate(scriptStr, "main.js");
            if(result.isError())
            {
                handleLog(result.toString());
            }

            qDebug() << "script result:" << result.toString();
        };
        QtConcurrent::run(functor);
    }
    else
    {
        // 暂停
    }
}


void MainWindow::on_pushButton_stop_clicked()
{
    ui->pushButton_start->setVisible(true);
    ui->pushButton_stop->setVisible(false);

    if(mEngine.isNull() == false)
    {
        std::atomic_store(&stop_flag, 1);
        mEngine->abortEvaluation();
    }
}

QString MainWindow::defaultCode()
{
    return
R"(function add(a, b){
    return a + b;
}

sleep(1000)
console.log('result', add(12, 3))
sleep(1000)
console.log('done')


var val = 0
while(1){
    sleep(1000)
    console.log(Date.now())

    switch(val){
    case 1:
         console.log('this is', val)
    break;
    case 2:
         console.log('this is', val)
    break;
    case 3:
         console.log('this is', val)
    break;
    default:break;
    }
    sleep(1000)

    val++
    if(val > 3)
    {
        val = 0;
    }
}
)";
}

QScriptValue funcLog(QScriptContext *context, QScriptEngine *engine, void *data)
{
    if(context->argumentCount() < 1)
    {
        return context->throwError(QScriptContext::TypeError, "need argument");
    }

    auto retVal = engine->undefinedValue();

    QString info;
    for (int i = 0; i < context->argumentCount(); ++i) {
        if(i != 0)
        {
            info += " ,";
        }
        info += context->argument(i).toString();
    }

    MainWindow *mainWindow = static_cast<MainWindow*>(data);
    if(mainWindow == nullptr)
    {
        return retVal;
    }

    mainWindow->handleLog(info);

    return retVal;
}

QScriptValue funcSleep(QScriptContext *context, QScriptEngine *engine, void *data)
{
    if(context->argumentCount() < 1)
    {
        return context->throwError(QScriptContext::TypeError, "need argument");
    }

    auto retVal = engine->undefinedValue();

    MainWindow *mainWindow = static_cast<MainWindow*>(data);
    if(mainWindow == nullptr)
    {
        return retVal;
    }

    QDeadlineTimer timer(context->argument(0).toNumber());
    while(timer.hasExpired() == false)
    {
        if(mainWindow->stopFlagValue() == 1)
        {
            break;
        }

        QThread::msleep(1);
    }

    return retVal;
}

