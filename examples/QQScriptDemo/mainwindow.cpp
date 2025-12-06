#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>

#include "myscriptengineagent.h"

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

    engine.globalObject().setProperty("print", engine.newFunction(funcPrint, this));

    QString scriptStr =
R"(var a = 10;
print(2)
print(3)
print(4)
var b = 100;
var c = a+b
/*function add(a, b){
return a + b;
}
add(a,b)*/
)";

    QScriptValue result;
    result = engine.evaluate(scriptStr, "main.js");

    qDebug() << "script result:" << result.toString();
}

MainWindow::~MainWindow()
{
    delete ui;
}
