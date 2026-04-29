#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QtConcurrentRun>
#include <QDateTime>
#include <QScriptValueIterator>
#include <QInputDialog>
#include <QMessageBox>

#include "codeEditor/jscodeeditor.h"
#include "myscriptengineagent.h"
#include "myscriptcode.h"
// #include "myqobject.h"

#include <QMetaType>
#include "barprototype.h"

// Simple C++ type for testing defaultPrototype
struct Bar {
    // QString name;
    // int value{0};
};
Q_DECLARE_METATYPE(Bar)

#define JS_FILE_NAME "main.js"

#ifdef Q_OS_WIN
#pragma execution_character_set("utf-8")
#endif

QScriptValue funcLog(QScriptContext *context, QScriptEngine *engine, void *data);
QScriptValue funcSleep(QScriptContext *context, QScriptEngine *engine, void *data);
QScriptValue funcWithoutData(QScriptContext *context, QScriptEngine *engine);
QScriptValue Foo(QScriptContext *context, QScriptEngine *engine);
QScriptValue constructBar(QScriptContext *context, QScriptEngine *engine);

// 使用宽字符串
static QString runToLineDialogTitle()
{
    return QString::fromWCharArray(L"运行到指定行");
}

static QString runToLineDialogLabel()
{
    return QString::fromWCharArray(L"请输入目标行号");
}

static QString runToLineHintTitle()
{
    return QString::fromWCharArray(L"提示");
}

static QString runToLineWarningText(int requestedLine)
{
    return QString::fromWCharArray(L"无法运行到当前指定行号：%1").arg(requestedLine);
}

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
    codeEditor->setCodeFoldingEnabled(true);    // 代码折叠功能
    codeEditor->setExecutionArrowEnabled(true); // 显示箭头
    codeEditor->setReadOnly(false);             // 允许实时编辑脚本内容
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

    // 当用户编辑停止一定时间后，则重映射断点和执行行
    connect(codeEditor, &JSCodeEditor::contentEditedDebounced, this, [=]() {
        // if(mEngine.isNull()) {
        codeEditor->remapBreakpointsAfterEdit();
        // 将编辑后的断点同步到保存的断点集合
        mBreakPoints[JS_FILE_NAME] = codeEditor->getBreakpoints();
        // }
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

        codeEditor->addAnnotation(posInfo.line, info);
    }, Qt::QueuedConnection);
}

int MainWindow::stopFlagValue()
{
   return std::atomic_load(&stop_flag);
}

QScriptEngine::RunToLineInfo MainWindow::resolveRunToLineRequest(int requestedLine)
{
    QScriptEngine engine;

#ifdef QUICKJS_NG
    QList<QScriptEngine::ModuleExport> exports;
    exports << QScriptEngine::ModuleExport("int32", 42, QScriptEngine::ModuleExport::Int32);
    exports << QScriptEngine::ModuleExport("int64", (int64_t)666, QScriptEngine::ModuleExport::Int64);
    exports << QScriptEngine::ModuleExport("double", 1.234, QScriptEngine::ModuleExport::Double);
    exports << QScriptEngine::ModuleExport("str", QString("这是模块字符串属性"), QScriptEngine::ModuleExport::String);

    QScriptValue obj = engine.newObject();
    obj.setProperty("str", "这是对象字符串属性");
    exports << QScriptEngine::ModuleExport("obj", QVariant::fromValue(obj), QScriptEngine::ModuleExport::Object);
    exports << QScriptEngine::ModuleExport("Print",
                                           QScriptEngine::ModuleExport::Function,
                                           engine.newFunction(funcLog, this), 1);
    engine.registerModule("m", exports);
#endif

    return engine.resolveRunToLine(codeEditor->getSourceCode(), JS_FILE_NAME, requestedLine);
}

void MainWindow::on_pushButton_start_clicked()
{
    if(!mStartUsesExistingRunToLineRequest)
    {
        mRequestedRunToLine = 0;
    }
    mStartUsesExistingRunToLineRequest = false;

    // ▶️ ⏸️
    if(mEngine.isNull())
    {
        std::atomic_store(&stop_flag, 0);
        mScriptThreadRunning = true;

        ui->pushButton_start->setVisible(false);
        ui->pushButton_stop->setVisible(true);
        ui->pushButton_stepIn->setVisible(true);
        ui->pushButton_stepOut->setVisible(true);
        ui->pushButton_stepOver->setVisible(true);
        ui->pushButton_continue->setVisible(true);
        ui->plainTextEdit->clear();
        codeEditor->clearExecutionArrow();
        codeEditor->clearAnnotations();

        // 先把本次执行需要的输入固化到成员里，后台线程再按快照读取
        mActiveRequestedRunToLine = mRequestedRunToLine;
        mActiveScriptSource = codeEditor->getSourceCode();

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

            // console
            QScriptValue console = engine.newObject();
            console.setProperty("log", engine.newFunction(funcLog, this));
            engine.globalObject().setProperty("console", console);

            // print
            engine.globalObject().setProperty("print", engine.newFunction(funcLog, this));

            // sleep
            engine.globalObject().setProperty("sleep", engine.newFunction(funcSleep, this));

            // 测试带 prototype 的构造器/工厂函数
            QScriptValue fooProto = engine.newObject();
            fooProto.setProperty("whatever", engine.newVariant(QString("protoVal")));
            engine.globalObject().setProperty("Foo", engine.newFunction(Foo, fooProto));

            // 测试自定义类型 Bar 的 defaultPrototype 与构造器（使用 QObject 原型）
            BarPrototype *barPrototypeObject = new BarPrototype();
            QScriptValue barProto = engine.newQObject(barPrototypeObject);
            engine.setDefaultPrototype(qMetaTypeId<Bar>(), barProto);
            QScriptValue barCtor = engine.newFunction(constructBar, barProto);
            // set constructor.prototype to the prototype object
            // barCtor.setProperty("prototype", barProto);
            engine.globalObject().setProperty("Bar", barCtor);

            // 测试无带参函数签名注册功能
            engine.globalObject().setProperty("callPure", engine.newFunction(funcWithoutData));


            // 测试QObject
            QObject qObj;
            qObj.setObjectName("this is qObj");
            qDebug() << "obj name:" << qObj.objectName();
            QObject::connect(&qObj, &QObject::destroyed, [](QObject* obj) {
                qDebug() << "对象已被销毁! 指针地址:" << obj;
                qDebug() << "对象名:" << (obj ? obj->objectName() : "nullptr");
            });
            auto jsQObj = engine.newQObject(&qObj);
            engine.globalObject().setProperty("qObj", jsQObj);


#ifdef QUICKJS_NG
            // 配置模块属性
            QList<QScriptEngine::ModuleExport> exports;
            exports << QScriptEngine::ModuleExport("int32", 42, QScriptEngine::ModuleExport::Int32);
            exports << QScriptEngine::ModuleExport("int64", (int64_t)666, QScriptEngine::ModuleExport::Int64);
            exports << QScriptEngine::ModuleExport("double", 1.234, QScriptEngine::ModuleExport::Double);
            exports << QScriptEngine::ModuleExport("str", QString("这是模块字符串属性"), QScriptEngine::ModuleExport::String);

            // 嵌套对象需要特殊处理
            QScriptValue obj = engine.newObject();
            obj.setProperty("str", "这是对象字符串属性");
            exports << QScriptEngine::ModuleExport("obj", QVariant::fromValue(obj), QScriptEngine::ModuleExport::Object);


            // 配置模块方法
            exports << QScriptEngine::ModuleExport("Print",
                                                   QScriptEngine::ModuleExport::Function,
                                                   engine.newFunction(funcLog, this), 1);

            engine.registerModule("m", exports);
#endif

            auto chkRet = engine.checkSyntax(mActiveScriptSource);
            qDebug() << "check result:"
                     << chkRet.isValid()
                     << chkRet.errorLineNumber()
                     << chkRet.errorColumnNumber()
                     << chkRet.errorMessage();
            if(chkRet.isValid() == false)
            {
                // 当有语法错误时，假装其是一个运行时错误，方便显示
                mEngineAgent->positionChange(0, chkRet.errorLineNumber(), chkRet.errorColumnNumber());
                handleLog(chkRet.errorMessage());
            }
            else
            {
                bool shouldEvaluate = true;
                if(mActiveRequestedRunToLine > 0)
                {
                    // 启动前先解析一次目标行，避免在线程里直接跑到无效位置
                    auto runToLineInfo = engine.resolveRunToLine(mActiveScriptSource,
                                                                 JS_FILE_NAME,
                                                                 mActiveRequestedRunToLine);
                    if(runToLineInfo.enabled == false)
                    {
                        shouldEvaluate = false;
                        engine.clearRunToLineInfo();
                        engineAgent.clearRunToLine();

                        const QString warningText = runToLineInfo.warningText.isEmpty()
                                                        ? runToLineWarningText(mActiveRequestedRunToLine)
                                                        : runToLineInfo.warningText;
                        QMetaObject::invokeMethod(this, [=](){
                            QMessageBox::warning(this, runToLineHintTitle(), warningText);
                        }, Qt::QueuedConnection);
                    }
                    else
                    {
                        if(runToLineInfo.warningText.isEmpty() == false)
                        {
                            const QString warningText = runToLineInfo.warningText;
                            QMetaObject::invokeMethod(this, [=](){
                                QMessageBox::warning(this, runToLineHintTitle(), warningText);
                            }, Qt::QueuedConnection);
                        }

                        // 运行到指定行的初始暂停点由 engine/agent 协同控制
                        engine.setRunToLineInfo(runToLineInfo);
                        engineAgent.configureRunToLine(true,
                                                       runToLineInfo.requestedLine,
                                                       runToLineInfo.resolvedLine,
                                                       runToLineInfo.warningText);
                    }
                }
                else
                {
                    engine.clearRunToLineInfo();
                    engineAgent.clearRunToLine();
                }

                QScriptValue result;
                if(shouldEvaluate)
                {
                    result = engine.evaluate(mActiveScriptSource, JS_FILE_NAME, 0);
                    qDebug() << "script result:" << result.toString();
                    if(result.isError())
                    {
                        handleLog(result.toString());
                    }
                }
            }

            // 检查异步错误 // 也可以放在evaluate里面进行判断
            if (engine.hasUncaughtPromiseRejections()) {
                auto rejections = engine.uncaughtPromiseRejections();
                for (const auto &rej : rejections) {
                    qDebug() << "Async error:" << rej.toString();
                }
            }

            // 标记该对象然后调用对应线程下的释放操作
            barPrototypeObject->deleteLater();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

            // 或者由于变量在之后没有继续被使用到，直接delete也行
            // delete barPrototypeObject;
            // barPrototypeObject = nullptr;

            QMetaObject::invokeMethod(this, [this](){
                // on_pushButton_stop_clicked();    // 脚本暂停后再点击停止，似乎会重复调用
                mScriptThreadRunning = false;
                mActiveRequestedRunToLine = 0;
                mActiveScriptSource.clear();
                ui->pushButton_start->setVisible(true);
                ui->pushButton_stop->setVisible(false);
                ui->pushButton_stepIn->setVisible(false);
                ui->pushButton_stepOut->setVisible(false);
                ui->pushButton_stepOver->setVisible(false);
                ui->pushButton_continue->setVisible(false);

                if(mPendingRestartRunToLine >= 0)
                {
                    mRequestedRunToLine = mPendingRestartRunToLine;
                    mPendingRestartRunToLine = -1;
                    mStartUsesExistingRunToLineRequest = true;
                    on_pushButton_start_clicked();
                }
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

QScriptValue funcWithoutData(QScriptContext *context, QScriptEngine *engine)
{
    // // 故意泄漏：创建但不释放
    // for(int i = 0; i < 5; i++) {
    //     JSValue leaking = JS_NewString(engine->ctx(), "This string is leaked");
    //     // 没有 JS_FreeValue(engine->m_ctx, leaking);
    // }
    // return QScriptValue(QString("leaked 100 strings"));
    return QScriptValue(QString("hello from funcWithoutData"));
}

QScriptValue constructBar(QScriptContext *context, QScriptEngine *engine)
{
    if (!context || !engine)
        return QScriptValue();
    // create C++ Bar value (could initialize from context arguments)
    Bar bar;

    return engine->toScriptValue(bar);
}

QScriptValue Foo(QScriptContext *context, QScriptEngine *engine)
{
    if (!context || !engine)
        return QScriptValue();

    if (context->isCalledAsConstructor()) {
        // initialize the new object (thisObject refers to the new instance)
        context->thisObject().setProperty("bar", engine->newVariant(QString("from ctor")));
        return engine->undefinedValue();
    } else {
        // not called as constructor: create and return our own object
        QScriptValue object = engine->newObject();
        QScriptValue callee = context->callee();
        QScriptValue proto = callee.property("prototype");
        if (proto.isValid()) {
            // set prototype using engine API
            object.setPrototype(proto);
        }
        object.setProperty("baz", engine->newVariant(QString("from call")));
        return object;
    }
}

QString MainWindow::defaultCode()
{
    return defultcode;
}

QString MainWindow::asnycCode()
{
    return asnyccode;
}

QString MainWindow::debugCode()
{
    return debugcode;
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

void MainWindow::on_pushButton_runToLine_clicked()
{
    bool ok = false;
    // 输入 0 时直接退化为普通启动，其余值按 run-to-line 处理
    int requestedLine = QInputDialog::getInt(this,
                                             runToLineDialogTitle(),
                                             runToLineDialogLabel(),
                                             0,
                                             0,
                                             INT_MAX,
                                             1,
                                             &ok);
    if(!ok)
    {
        return;
    }

    if(requestedLine > 0)
    {
        auto runToLineInfo = resolveRunToLineRequest(requestedLine);
        if(runToLineInfo.enabled == false)
        {
            const QString warningText = runToLineInfo.warningText.isEmpty()
                                            ? runToLineWarningText(requestedLine)
                                            : runToLineInfo.warningText;
            QMessageBox::warning(this, runToLineHintTitle(), warningText);
            return;
        }
    }

    if(mScriptThreadRunning || mEngine.isNull() == false)
    {
        // 暂停态或运行态下重新发起时，先停掉旧引擎，再自动重启
        mPendingRestartRunToLine = requestedLine;
        on_pushButton_stop_clicked();
        return;
    }

    mRequestedRunToLine = requestedLine;
    mStartUsesExistingRunToLineRequest = true;
    on_pushButton_start_clicked();
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

void MainWindow::on_pushButton_loadAsnyc_clicked()
{
    // 将 asnycCode 加载到编辑器；用户可在加载后继续编辑，后续重置不会覆盖当前编辑内容
    codeEditor->setPlainText(asnycCode());
}
