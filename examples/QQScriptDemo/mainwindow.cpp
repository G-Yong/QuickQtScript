#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QtConcurrentRun>
#include <QScriptValueIterator>

#include "myscriptengineagent.h"

#include "myqobject.h"

#ifdef Q_OS_WIN
#pragma execution_character_set("utf-8")
#endif

QScriptValue funcPrint(QScriptContext *contex, QScriptEngine *engine, void *data)
{
    if (contex->argumentCount() < 1)
    {
        return contex->throwError(QScriptContext::TypeError, "argument count is invalid!");
    }
    qDebug() << "print-->:" << contex->argument(0).toVariant();

    return QScriptValue();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 设置为static，是为了延长生存期，以免在后面调用 engine.abortEvaluation() 时，engine被析构对象
    /*static*/ QScriptEngine engine;
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
// R"(var a = 10; jkg%^
// qObj.objectName = '789abc'
// print(qObj.objectName)
// print(qObj.myFunc('tyhjk'))
// print(2)
// print(3)
// print(4)
// var b = 100;
// var c = a+b
// /*function add(a, b){
// return a + b;
// }
// add(a,b)*/
// )"

// R"(
// while(1)
// {
// var a = 0;
// a++;
// }
// )"

// R"(
// function add(a, b){
// return a + b;
// // return;
// }
// add(1, 2)
// add(3, 4)
// var a = 1 + 2
// var b = a + 1
// )"


R"(let GI001 = 2
    var b = 0;
    var c
    c = b + 1
    print(GI001);
    if(GI001 === 2)
    {
        print('222222222222')
    }
    switch(GI001){
    case 0:
        print('00000000000')
            break;
    case 1:
        print('11111111111')
            break;
    case 2:
        print('22222222222')
            break;
    }
)"

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
