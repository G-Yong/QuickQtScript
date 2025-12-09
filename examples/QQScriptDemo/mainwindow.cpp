#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QtConcurrentRun>

#include "myscriptengineagent.h"

#include "myqobject.h"

QScriptValue funcPrint(QScriptContext *contex, QScriptEngine *engine, void *data)
{
    qDebug() << "print-->:" << contex->argument(0).toVariant();

    return QScriptValue();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QScriptEngine engine;
    MyScriptEngineAgent engineAgent(&engine);
    engine.setAgent(&engineAgent);

    // // 测试 停止脚本
    // QtConcurrent::run([&](){
    //     QThread::msleep(1000);
    //     engine.abortEvaluation();
    // });

    // 测试QObject
    // 存在重大问题，暂时不要使用此功能
    MyQObject qObj;
    qObj.setObjectName("this is qObj");
    qDebug() << "obj name:" << qObj.objectName();
    auto jsQObj = engine.newQObject(&qObj);
    engine.globalObject().setProperty("qObj", jsQObj);


    engine.globalObject().setProperty("print", engine.newFunction(funcPrint, this));

    QString scriptStr =
R"(var a = 10; jkg%^
qObj.objectName = '789abc'
print(qObj.objectName)
print(qObj.myFunc('tyhjk'))
print(2)
print(3)
print(4)
var b = 100;
var c = a+b
/*function add(a, b){
return a + b;
}
add(a,b)*/
)"

// R"(
// while(1)
// {
// var a = 0;
// a++;
// }
// )"

        ;

    // auto chkRet = engine.checkSyntax(scriptStr);
    // qDebug() << "result:"
    //          << chkRet.errorLineNumber()
    //          << chkRet.errorColumnNumber()
    //          << chkRet.errorMessage();
    // return;

    QScriptValue result;
    result = engine.evaluate(scriptStr, "main.js");

    qDebug() << "script result:" << result.toString();
}

MainWindow::~MainWindow()
{
    delete ui;
}
