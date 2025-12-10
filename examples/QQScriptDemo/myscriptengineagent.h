#ifndef MYSCRIPTENGINEAGENT_H
#define MYSCRIPTENGINEAGENT_H

#include <QScriptEngineAgent>
#include <QScriptContext>

class MyScriptEngineAgent : public QScriptEngineAgent
{
public:
    MyScriptEngineAgent(QScriptEngine *engine);

    virtual void scriptLoad(qint64 id, const QString &program,
                            const QString &fileName, int baseLineNumber);
    virtual void scriptUnload(qint64 id);

    virtual void contextPush();
    virtual void contextPop();

    virtual void functionEntry(qint64 scriptId);
    virtual void functionExit(qint64 scriptId,
                              const QScriptValue &returnValue);

    virtual void positionChange(qint64 scriptId,
                                int lineNumber, int columnNumber);

    virtual void exceptionThrow(qint64 scriptId,
                                const QScriptValue &exception,
                                bool hasHandler);
    virtual void exceptionCatch(qint64 scriptId,
                                const QScriptValue &exception);

    // virtual bool supportsExtension(Extension extension) const;
    // virtual QVariant extension(Extension extension,
    //                            const QVariant &argument = QVariant());
};

#endif // MYSCRIPTENGINEAGENT_H
