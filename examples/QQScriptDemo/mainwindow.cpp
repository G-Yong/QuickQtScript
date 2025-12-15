#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QtConcurrentRun>
#include <QDateTime>
#include <QScriptValueIterator>
#include "codeEditor/jscodeeditor.h"

#include "myscriptengineagent.h"

// #include "myqobject.h"

#define JS_FILE_NAME "main.js"

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
    ui->pushButton_stepIn->setVisible(false);
    ui->pushButton_stepOut->setVisible(false);
    ui->pushButton_stepOver->setVisible(false);
    ui->pushButton_continue->setVisible(false);


    codeEditor = new JSCodeEditor();
    codeEditor->setCodeFoldingEnabled(false); // 代码折叠功能还有大问题，先禁用
    codeEditor->setExecutionArrowEnabled(true); // 显示当箭头
    codeEditor->setReadOnly(false); // 允许实时编辑脚本内容
    QHBoxLayout *codeEditorLayout = new QHBoxLayout();
    codeEditorLayout->setMargin(0);
    codeEditorLayout->addWidget(codeEditor);
    ui->widget->setLayout(codeEditorLayout);

    // 默认加载 debugCode，用户可以实时修改编辑器内容
    codeEditor->setPlainText(debugCode());

    connect(codeEditor, &JSCodeEditor::breakPointsChanged, this, [=](){
        auto breakPoints = codeEditor->getBreakpoints();
        mBreakPoints[JS_FILE_NAME] = breakPoints;
        if(mEngineAgent.isNull() == false)
        {
            mEngineAgent->clearBreakpoints();
            foreach (auto line, breakPoints) {
                mEngineAgent->addBreakpoint(JS_FILE_NAME, line);
            }
        }
    });

    // 当用户编辑停止一定时间后，如果脚本未在执行，则重映射断点和执行行
    connect(codeEditor, &JSCodeEditor::contentEditedDebounced, this, [=]() {
        if(mEngine.isNull()) {
            codeEditor->remapBreakpointsAfterEdit();
            // 将编辑后的断点同步到保存的断点集合
            mBreakPoints[JS_FILE_NAME] = codeEditor->getBreakpoints();
        }
    });
}

MainWindow::~MainWindow()
{
    if(mEngine)
    {
        mEngineAgent->stopDebugging();
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
        ui->pushButton_stepIn->setVisible(true);
        ui->pushButton_stepOut->setVisible(true);
        ui->pushButton_stepOver->setVisible(true);
        ui->pushButton_continue->setVisible(true);
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

            // 添加断点
            auto bps = mBreakPoints[JS_FILE_NAME];
            engineAgent.clearBreakpoints();
            foreach (auto line, bps) {
                engineAgent.addBreakpoint(JS_FILE_NAME, line);
            }
            engineAgent.setDebugMode(MyScriptEngineAgent::Continue);

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
            result = engine.evaluate(scriptStr, JS_FILE_NAME);
            if(result.isError())
            {
                handleLog(result.toString());
            }

            qDebug() << "script result:" << result.toString();

            QMetaObject::invokeMethod(this, [=](){
                on_pushButton_stop_clicked();
            }, Qt::QueuedConnection);
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
    ui->pushButton_stepIn->setVisible(false);
    ui->pushButton_stepOut->setVisible(false);
    ui->pushButton_stepOver->setVisible(false);
    ui->pushButton_continue->setVisible(false);

    if(mEngine.isNull() == false)
    {
        mEngineAgent->stopDebugging();
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

var val = add(12, 3)
console.log(val)

var c
var d = 1
var e

if(true){
 console.log(true)
}
if(false){
 console.log(false)
}

switch(d){
case 0:
     console.log('this is', val)
break;
case 1:
     console.log('this is', val)
break;
case 2:
     console.log('this is', val)
break;
default:break;
}

var offset = 0
for(var i=0; i<50; i++)  {
   offset++
   sleep(1000)
   console.log('offset', offset)
   sleep(1000)
}
/*
sleep(1000)
console.log('result', add(12, 3))
sleep(1000)
console.log('done')

var val = 0
//while(1)
{
    sleep(1000)
    console.log(Date.now())

    sleep(1000)

    val++
    if(val >= 2)
    {
        val = 0;
    }
}
*/
)";
}

QString MainWindow::debugCode()
{
    return
R"(
// ==================== 变量测试 ====================
// 基本变量声明和赋值
var intVar = 42;
var floatVar = 3.14159;
var strVar = "Hello, QuickJS!";
var boolVar = true;
var nullVar = null;
var undefinedVar;

console.log("===== 变量测试 =====");
console.log("整数:", intVar);
console.log("浮点数:", floatVar);
console.log("字符串:", strVar);
console.log("布尔值:", boolVar);
console.log("null值:", nullVar);
console.log("undefined值:", undefinedVar);

// 变量类型检查
console.log("intVar的类型:", typeof intVar);
console.log("floatVar的类型:", typeof floatVar);
console.log("strVar的类型:", typeof strVar);

// ==================== 数组测试 ====================
console.log("\n===== 数组测试 =====");

// 数组创建和访问
var arr = [1, 2, 3, 4, 5];
console.log("数组:", arr);
console.log("数组长度:", arr.length);
console.log("访问数组[0]:", arr[0]);
console.log("访问数组[4]:", arr[4]);

// 数组方法
var arr2 = [10, 20, 30];
var joined = arr2.join("-");
console.log("join方法结果:", joined);

var arr3 = [1, 2, 3];
arr3.push(4);
console.log("push(4)后的数组:", arr3);

var popped = arr3.pop();
console.log("pop返回值:", popped);
console.log("pop后的数组:", arr3);

// 数组遍历
console.log("遍历数组:");
var sum = 0;
for (var i = 0; i < arr.length; i++) {
    console.log("  arr[" + i + "] = " + arr[i]);
    sum += arr[i];
}
console.log("数组求和:", sum);

// ==================== 对象测试 ====================
console.log("\n===== 对象测试 =====");

// 对象创建和访问
var person = {
    name: "John Doe",
    age: 30,
    city: "New York",
    email: "john@example.com"
};

console.log("对象:", person.name, person.age, person.city);
console.log("访问对象属性.name:", person.name);
console.log("访问对象属性['age']:", person["age"]);

// 修改对象属性
person.age = 31;
console.log("修改age后:", person.age);

// 添加新属性
person.phone = "123-456-7890";
console.log("添加phone属性:", person.phone);

// 对象属性遍历
console.log("遍历对象属性:");
var keys = [];
for (var key in person) {
    console.log("  " + key + ": " + person[key]);
    keys.push(key);
}
console.log("对象的所有键:", keys.length, "个");

// ==================== 函数测试 ====================
console.log("\n===== 函数测试 =====");

// 基本函数定义和调用
function greet(name) {
    return "Hello, " + name + "!";
}
console.log("调用greet函数:", greet("Alice"));

// 多参数函数
function multiply(a, b) {
    return a * b;
}
console.log("3 * 7 =", multiply(3, 7));

// 递归函数 - 计算阶乘
function factorial(n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}
console.log("5的阶乘:", factorial(5));

// 带有默认值的函数模拟
function sayHello(greeting, name) {
    if (greeting === undefined) greeting = "Hi";
    if (name === undefined) name = "Friend";
    return greeting + ", " + name + "!";
}
console.log(sayHello());
console.log(sayHello("Hello"));
console.log(sayHello("Hey", "Bob"));

// 函数作为参数
function applyOperation(a, b, operation) {
    return operation(a, b);
}

var result = applyOperation(5, 3, function(x, y) {
    return x + y;
});
console.log("applyOperation(5, 3, add):", result);

// 返回函数的函数
function makeAdder(x) {
    return function(y) {
        return x + y;
    };
}

var addFive = makeAdder(5);
console.log("makeAdder(5)(3):", addFive(3));

// ==================== 条件控制测试 ====================
console.log("\n===== 条件控制测试 =====");

// if-else 语句
var testNum = 10;
if (testNum > 0) {
    console.log(testNum, "是正数");
} else if (testNum < 0) {
    console.log(testNum, "是负数");
} else {
    console.log(testNum, "是零");
}

// 三元运算符
var age = 25;
var ageGroup = age >= 18 ? "成人" : "未成年";
console.log("年龄分组:", ageGroup);

// switch-case 语句
var day = 3;
var dayName = "";
switch (day) {
    case 1:
        dayName = "Monday";
        break;
    case 2:
        dayName = "Tuesday";
        break;
    case 3:
        dayName = "Wednesday";
        break;
    case 4:
        dayName = "Thursday";
        break;
    case 5:
        dayName = "Friday";
        break;
    default:
        dayName = "Weekend";
}
console.log("第" + day + "天是:", dayName);

// ==================== 循环测试 ====================
console.log("\n===== 循环测试 =====");

// for 循环
console.log("for循环 (1到5):");
for (var i = 1; i <= 5; i++) {
    console.log("  i =", i);
}

// while 循环
var counter = 0;
var whileResult = "";
while (counter < 3) {
    whileResult += counter + " ";
    counter++;
}
console.log("while循环结果:", whileResult);

// do-while 循环
var num = 0;
var doWhileCount = 0;
do {
    doWhileCount++;
    num++;
} while (num < 2);
console.log("do-while循环执行", doWhileCount, "次");

// 嵌套循环
console.log("嵌套循环 (3x3矩阵):");
for (var row = 1; row <= 3; row++) {
    var rowStr = "";
    for (var col = 1; col <= 3; col++) {
        rowStr += (row * col) + " ";
    }
    console.log("  " + rowStr);
}

// ==================== 字符串操作测试 ====================
console.log("\n===== 字符串操作测试 =====");

var str = "JavaScript";
console.log("原字符串:", str);
console.log("字符串长度:", str.length);
console.log("转大写:", str.toUpperCase());
console.log("转小写:", str.toLowerCase());

var str2 = "Hello World";
var index = str2.indexOf("World");
console.log("'World'在字符串中的位置:", index);

var substring = str2.substring(0, 5);
console.log("substring(0, 5):", substring);

var replaced = str2.replace("World", "JavaScript");
console.log("replace后:", replaced);

var splits = str2.split(" ");
console.log("split结果:", splits);

// ==================== 数学运算测试 ====================
console.log("\n===== 数学运算测试 =====");

var a = 10;
var b = 3;
console.log(a, "+", b, "=", a + b);
console.log(a, "-", b, "=", a - b);
console.log(a, "*", b, "=", a * b);
console.log(a, "/", b, "=", a / b);
console.log(a, "%", b, "=", a % b);
console.log(a, "**", b, "=", a ** b);

// 逻辑运算
console.log("true && false =", true && false);
console.log("true || false =", true || false);
console.log("!true =", !true);

// ==================== 对象方法测试 ====================
console.log("\n===== 对象方法测试 =====");

var car = {
    brand: "Toyota",
    model: "Camry",
    year: 2023,
    getInfo: function() {
        return this.year + " " + this.brand + " " + this.model;
    },
    honk: function() {
        return "Beep! Beep!";
    }
};

console.log("车辆信息:", car.getInfo());
console.log("鸣笛声:", car.honk());

// ==================== 异常处理测试 ====================
console.log("\n===== 异常处理测试 =====");

try {
    var testVal = undefined.property; // 这会抛出错误
} catch (e) {
    console.log("捕获到异常:", "访问undefined的属性");
}

try {
    console.log("尝试执行...");
    // 正常执行
    var normalOp = 5 + 3;
    console.log("正常执行结果:", normalOp);
} catch (e) {
    console.log("异常");
} finally {
    console.log("finally块总是执行");
}

// ==================== 综合功能测试 ====================
console.log("\n===== 综合功能测试 =====");

// 定义一个复杂的类似对象的结构
function Student(name, grade, scores) {
    this.name = name;
    this.grade = grade;
    this.scores = scores;
}

// 为原型添加方法
Student.prototype.getAverage = function() {
    var sum = 0;
    for (var i = 0; i < this.scores.length; i++) {
        sum += this.scores[i];
    }
    return sum / this.scores.length;
};

Student.prototype.getGrade = function() {
    var avg = this.getAverage();
    if (avg >= 90) return "A";
    if (avg >= 80) return "B";
    if (avg >= 70) return "C";
    return "F";
};

var student1 = new Student("Alice", 10, [85, 90, 88, 92]);
var student2 = new Student("Bob", 10, [78, 82, 75, 80]);

console.log("学生1:", student1.name, "平均分:", student1.getAverage().toFixed(2), "等级:", student1.getGrade());
console.log("学生2:", student2.name, "平均分:", student2.getAverage().toFixed(2), "等级:", student2.getGrade());

// ==================== 完成测试 ====================
console.log("\n===== 所有测试完成 =====");
console.log("测试已全部执行完毕!");
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
            info += " ";
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


void MainWindow::on_pushButton_stepOver_clicked()
{
    if(mEngineAgent.isNull() == false)
    {
        mEngineAgent->stepOver();
    }
}


void MainWindow::on_pushButton_stepIn_clicked()
{
    if(mEngineAgent.isNull() == false)
    {
        mEngineAgent->stepInto();
    }
}


void MainWindow::on_pushButton_stepOut_clicked()
{
    if(mEngineAgent.isNull() == false)
    {
        mEngineAgent->stepOut();
    }
}


void MainWindow::on_pushButton_continue_clicked()
{
    if(mEngineAgent.isNull() == false)
    {
        mEngineAgent->continueExecution();
    }
}

void MainWindow::on_pushButton_loadDefault_clicked()
{
    // 将 defaultCode 加载到编辑器；用户可在加载后继续编辑，后续重置不会覆盖当前编辑内容
    codeEditor->setPlainText(defaultCode());
}

void MainWindow::on_pushButton_loadDebug_clicked()
{
    // 将 debugCode 加载到编辑器；用户可在加载后继续编辑，后续重置不会覆盖当前编辑内容
    codeEditor->setPlainText(debugCode());
}

void MainWindow::on_pushButton_reset_clicked()
{
    // 停止当前执行
    if(mEngine.isNull() == false)
    {
        mEngineAgent->stopDebugging();
        std::atomic_store(&stop_flag, 1);
        mEngine->abortEvaluation();
    }

    // 重置 UI 状态
    ui->pushButton_start->setVisible(true);
    ui->pushButton_stop->setVisible(false);
    ui->pushButton_stepIn->setVisible(false);
    ui->pushButton_stepOut->setVisible(false);
    ui->pushButton_stepOver->setVisible(false);
    ui->pushButton_continue->setVisible(false);

    // 清空日志
    ui->plainTextEdit->clear();

    // 清除执行箭头
    codeEditor->clearExecutionArrow();

    // 清除断点视觉效果但保留断点数据
    // 这样用户可以再次运行时使用之前的断点

    // 清空引擎指针
    mEngine = nullptr;
    mEngineAgent = nullptr;
}

