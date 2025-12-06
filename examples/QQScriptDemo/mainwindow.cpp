#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QScriptEngine engine;

    QString scriptStr =
R"(var a = 10;
var b = 100;
a + b
sd9
)";

    QScriptValue result;
    result = engine.evaluate(scriptStr, "main.js");

    qDebug() << "script result:" << result.toString();
}

MainWindow::~MainWindow()
{
    delete ui;
}
