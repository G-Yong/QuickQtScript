#ifndef QUICKJSTEST_H
#define QUICKJSTEST_H

#include "quickjs.h"
#include <QDebug>

// C 函数实现
static JSValue my_add(JSContext *ctx,
                      JSValueConst this_val,
                      int argc, JSValueConst *argv)
{
    auto obj = JS_NewObject(ctx);

    JSPropertyEnum *props{nullptr};
    uint32_t plen;
    JS_GetOwnPropertyNames(ctx,
                           &props,
                           &plen,
                           this_val,
                           JS_GPN_ENUM_ONLY | JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK);
    for(int i = 0; i < plen; i++)
    {
        const char *propName = JS_AtomToCString(ctx, props[i].atom);
        qDebug() << "propName:" << propName;

        JSValue v = JS_GetProperty(ctx, this_val, props[i].atom);
        JS_SetPropertyStr(ctx, obj, propName, v);

        JS_FreeCString(ctx, propName);
    }
    JS_FreePropertyEnum(ctx, props, plen);

    return obj;
}

void quickjsTest()
{
    auto rt = JS_NewRuntime();
    if(!rt)
    {
        qCritical() << "create js runtime fail";
        return;
    }

    auto ctx = JS_NewContext(rt);
    if(!ctx)
    {
        qCritical() << "create js context fail";
        JS_FreeRuntime(rt);
        rt = nullptr;
        return;
    }

    JSValue glbObj = JS_GetGlobalObject(ctx);
    JSValue obj    = JS_NewObject(ctx);
    JSValue func   = JS_NewCFunction(ctx, my_add, "plus131", 2);

    JS_SetPropertyStr(ctx, glbObj, "point1", obj);

    JS_SetPropertyStr(ctx, obj, "plus",   func);
    JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat64(ctx, 0.1));
    JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat64(ctx, 0.2));
    JS_SetPropertyStr(ctx, obj, "z", JS_NewFloat64(ctx, 0.3));

    QString scriptStr = "var a = point1.plus(1, 2); var b = point1.plus(1, 2);";
    QByteArray ba = scriptStr.toUtf8();

    auto retVal = JS_Eval(ctx, ba.data(), ba.length(), "<eval>", JS_EVAL_TYPE_GLOBAL);

    if(JS_IsException(retVal))
    {
        auto exception = JS_GetException(ctx);

        auto str = JS_ToCString(ctx, exception);
        qDebug() << "exception:" << str;
        JS_FreeCString(ctx, str);

        JS_Throw(ctx, exception);
    }
    auto str = JS_ToCString(ctx, retVal);
    qDebug() << "result:" << str;
    JS_FreeCString(ctx, str);

    JS_FreeValue(ctx, glbObj);
    JS_FreeValue(ctx, retVal);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    return;
}

#endif // QUICKJSTEST_H
