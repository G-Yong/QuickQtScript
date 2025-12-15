#ifndef QUICKJSTEST_H
#define QUICKJSTEST_H

#include "quickjs.h"
#include <stdio.h>

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
        // printf("propName:%s \n", propName);
        fprintf(stderr, "propName:%s \n", propName);

        JSValue v = JS_GetProperty(ctx, this_val, props[i].atom);
        if(JS_IsNumber(v))
        {
            double oldVal = 0;
            JS_ToFloat64(ctx, &oldVal, v);

            JS_FreeValue(ctx, v);
            JS_FreeValue(ctx, v);
            oldVal += 10;
            v = JS_NewFloat64(ctx, oldVal);
        }
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
        fprintf(stderr, "create js runtime fail\n");
        return;
    }

    auto ctx = JS_NewContext(rt);
    if(!ctx)
    {
        fprintf(stderr, "create js context fail\n");
        JS_FreeRuntime(rt);
        rt = nullptr;
        return;
    }

    JSValue glbObj = JS_GetGlobalObject(ctx);
    JSValue obj    = JS_NewObject(ctx);
    JSValue func   = JS_NewCFunction(ctx, my_add, "whatEver", 2);

    JS_SetPropertyStr(ctx, glbObj, "point1", obj);

    JS_SetPropertyStr(ctx, obj, "plus", func);
    JS_SetPropertyStr(ctx, obj, "x",    JS_NewFloat64(ctx, 0.1));
    JS_SetPropertyStr(ctx, obj, "y",    JS_NewFloat64(ctx, 0.2));
    JS_SetPropertyStr(ctx, obj, "z",    JS_NewFloat64(ctx, 0.3));

    const char *scriptStr = "var a = point1.plus(1, 2); point1.plus(1, 2).x + ', ' + point1.x;";
    auto retVal = JS_Eval(ctx, scriptStr, strlen(scriptStr), "<eval>", JS_EVAL_TYPE_GLOBAL);

    if(JS_IsException(retVal))
    {
        auto exception = JS_GetException(ctx);

        auto str = JS_ToCString(ctx, exception);
        printf("exception:%s \n", str);
        JS_FreeCString(ctx, str);

        JS_Throw(ctx, exception);
    }
    auto str = JS_ToCString(ctx, retVal);
    fprintf(stderr, "result: %s\n", str);
    // JS_FreeCString(ctx, str);

    JS_FreeValue(ctx, glbObj);
    // JS_FreeValue(ctx, retVal);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    return;
}

#include <QScriptEngine>
#include <QScriptValueIterator>

QScriptValue plus(QScriptContext *context, QScriptEngine *engine, void *data)
{
    if(context->argumentCount() < 1)
    {
        return context->throwError(QObject::tr("请输入参数"));
    }

    QScriptValue retObj = engine->newObject();

    // 深拷贝
    QScriptValue obj = context->thisObject();
    QScriptValueIterator it(obj);
    while (it.hasNext()) {
        it.next();
        retObj.setProperty(it.name(), it.value());
    }

    auto arguments = context->argument(0).toVariant().toMap();
    QStringList propStrList ={
        "x", "y", "z", "u", "v", "w"
    };
    foreach (auto propStr, propStrList) {
        if(arguments.contains(propStr))
        {
            double val = retObj.property(propStr).toNumber();
            val += arguments.value(propStr).toDouble();

            retObj.setProperty(propStr, val);
        }
    }

    return retObj;
}


QScriptValue genPointObj(QScriptEngine *engine)
{
    QScriptValue obj = engine->newObject();

    obj.setProperty("type", "point");

    obj.setProperty("des", "124324");

    obj.setProperty("x", 0.1);
    obj.setProperty("y", 0.2);
    obj.setProperty("z", 0.3);
    obj.setProperty("u", 0.4);
    obj.setProperty("v", 0.5);
    obj.setProperty("w", 0.6);

    obj.setProperty("plus",
                    engine->newFunction(plus, nullptr));

    return obj;
}

void scriptEngineTest()
{
    QScriptEngine engine;

    QScriptValue pt1 = genPointObj(&engine);

    engine.globalObject().setProperty("pt1", pt1);
    auto val = engine.evaluate("pt1.plus({x:100}); pt1.plus({x:100}).x;");

    qDebug() << val.toString();
}

#endif // QUICKJSTEST_H
