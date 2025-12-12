#include <QScriptEngineAgent.h>
#include <QScriptEngine.h>

#include <QDebug>
#include <QScriptContext>

// 常用 opcode（按功能分组，摘选自 quickjs-opcode.h）：

// 调用函数的主要 opcode：call（同时还有专用的优化变体 call0 / call1 / call2 / call3）。
// call：通用调用（可处理任意参数个数，表里标注为 npop，表示参数数量不在固定 n_pop 中）。
// call0..call3：对 0–3 个参数的快速路径（字节码更短、执行更快）。
// 其它相关调用类 opcode：
// call_constructor：用于 new Foo(...) 的构造器调用。
// tail_call / tail_call_method：尾调用优化。
// call_method：方法调用（this/方法分开处理的情形）。
// apply / apply_eval：对应 Function.prototype.apply / apply 类的特殊调用；eval / apply_eval 用于 eval 场景。

// 返回/结束
// return：函数有返回值的正常返回（你提到的 OP_return）。
// return_undef：无返回值时的返回。
// return_async：async 函数的返回（与 promise 相关）。

// 抛出/异常
// throw：抛出当前栈顶异常值。
// throw_error：带 message/atom 的抛错快捷码。
// catch / nip_catch：异常处理相关的控制码。

// 栈/临时值操作（经常见）
// push_i32, push_const, push_this, push_true / push_false：把常量/立即数/this 等压栈。
// drop, dup, dup1, dup2：弹栈/复制栈顶值等。
// swap, perm3 等：重排栈元素。

// 局部/参数/变量访问
// get_loc, put_loc, set_loc：对局部变量操作。
// get_arg, put_arg：对参数访问。
// get_var, put_var, get_var_undef：对作用域/全局变量的读写（含不存在时的行为）。

// 对象 / 字段 / 数组
// get_field, put_field：属性读取/写入。
// get_array_el, put_array_el：数组元素读取/写入。
// get_private_field, put_private_field：私有字段访问（class private）。

// 控制流 / 跳转
// if_false, if_true, goto：条件跳转与无条件跳转。
// gosub, ret：用于 finally 等子例程控制。
// label / source_loc：编译阶段或调试位置信息（source_loc 在字节码里保存源码位置）。

// 迭代 / 异步
// for_in_start, for_of_start, for_of_next：for-in / for-of 相关。
// yield, await, initial_yield, yield_star：生成器 / async 操作。

// 算术 / 逻辑（常用）
// add, sub, mul, div, mod，比较运算 lt/gt/eq 等。
// not, lnot, typeof 等。

// 其它重要 opcode
// eval：执行字符串 eval 的字节码路径。
// apply：将数组作为参数调用（Function.apply）。
// nop：空操作（占位）。


// QuickJs执行函数时，是按需执行的，没有用到的代码会直接跳过(也有可能是目前获取行号的逻辑有问题)
// 而Qt原生的，好像每一行代码都会执行
int scriptOPChanged(uint8_t op,
                    const char *fileName,
                    const char *funcName,
                    int line,
                    int col,
                    void *userData
                    )
{
    QScriptEngineAgent *agent = static_cast<QScriptEngineAgent*>(userData);
    if (!agent)
        return -1;

    // qDebug() << OP_call << OP_return;

    qDebug() << "op changed:"
             << (QJDefines::OPCodeEnum)op
             << fileName
             << funcName
             << line
             << col;

    int scriptId = agent->scriptId(fileName);


    if(true
    // && op != QJDefines::OP_check_define_var
    // && op != QJDefines::OP_put_var
    // && op != QJDefines::OP_leave_scope
    // && op != QJDefines::OP_push_7
        )
    {
        // 不能每次op变动都调用一次，要行列号变化才调用
        if(agent->isPosChanged(line, col))
        {
            // qDebug() << "script id:" << scriptId << fileName << line << col;
            agent->positionChange(scriptId, line, col);
        }
    }

    switch(op)
    {
    case QJDefines::OP_set_arg:
    case QJDefines::OP_set_arg0:
    case QJDefines::OP_set_arg1:
    case QJDefines::OP_set_arg2:
    case QJDefines::OP_set_arg3:
    case QJDefines::OP_fclosure:
    case QJDefines::OP_fclosure8:
    case QJDefines::OP_tail_call_method:
    case QJDefines::OP_call_method:
    case QJDefines::OP_tail_call:
    case QJDefines::OP_call_constructor:
    case QJDefines::OP_call0:
    case QJDefines::OP_call1:
    case QJDefines::OP_call2:
    case QJDefines::OP_call3:
    case QJDefines::OP_call:{
        // // functionEntry 和 functionExit 一定要成对
        // qDebug() << "functon entry:"
        //          << (QJDefines::OPCodeEnum)op
        //          << fileName
        //          << funcName
        //          << line
        //          << col;
        agent->functionEntry(scriptId);
    };break;
    case QJDefines::OP_return_undef:
    case QJDefines::OP_return_async:
    case QJDefines::OP_return:{
        // // 这个有问题，会有一次额外的return,应该是脚本eval本身的这一次
        // // 想办法去掉最后这一次？或者想办法在前面补充一次？
        // // 已在engin中的eval中虚构了一次functionEntry
        // qDebug() << "function exit:"
        //          << (QJDefines::OPCodeEnum)op
        //          << fileName
        //          << funcName
        //          << line
        //          << col;
        agent->functionExit(scriptId, QScriptValue());
    };break;
    }

    return 0;
}

QScriptEngineAgent::QScriptEngineAgent(QScriptEngine *engine)
    : m_engine(engine)
{
    mLastLine = -1;
    mLastCol = -1;

    JS_SetOPChangedHandler(engine->ctx(), scriptOPChanged, this);
    engine->setAgent(this);
}

QScriptEngineAgent::~QScriptEngineAgent()
{
}

void QScriptEngineAgent::contextPop()
{

}

void QScriptEngineAgent::contextPush()
{

}

QScriptEngine *QScriptEngineAgent::engine() const
{
    return m_engine;
}

void QScriptEngineAgent::exceptionCatch(qint64 scriptId, const QScriptValue &exception)
{

}

void QScriptEngineAgent::exceptionThrow(qint64 scriptId, const QScriptValue &exception, bool hasHandler)
{

}

// QVariant QScriptEngineAgent::extension(QScriptEngineAgent::Extension extension, const QVariant &argument)
// {

// }

void QScriptEngineAgent::functionEntry(qint64 scriptId)
{
    qDebug() << "function entry";
}

void QScriptEngineAgent::functionExit(qint64 scriptId, const QScriptValue &returnValue)
{
    qDebug() << "function exit";
}

void QScriptEngineAgent::positionChange(qint64 scriptId, int lineNumber, int columnNumber)
{
    // qDebug() << "position changed:" << scriptId << lineNumber << columnNumber;
}

void QScriptEngineAgent::scriptLoad(qint64 id, const QString &program, const QString &fileName, int baseLineNumber)
{
    // qDebug() << "script load:" << id << fileName;
}

void QScriptEngineAgent::scriptUnload(qint64 id)
{
    // qDebug() << "script unload" << id;
}

bool QScriptEngineAgent::isPosChanged(qint64 line, qint64 col)
{
    bool flag = false;
    if(mLastLine != line || mLastCol != col)
    {
        flag = true;
    }

    mLastLine = line;
    mLastCol = col;

    return flag;
}

qint64 QScriptEngineAgent::scriptId(QString fileName)
{
    if(engine() == nullptr)
    {
        return -1;
    }

    return engine()->fileNameBuffer().indexOf(fileName);
}

// bool QScriptEngineAgent::supportsExtension(QScriptEngineAgent::Extension extension) const
// {

// }

