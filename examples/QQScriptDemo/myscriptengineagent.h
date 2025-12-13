#ifndef MYSCRIPTENGINEAGENT_H
#define MYSCRIPTENGINEAGENT_H

#include <QScriptEngineAgent>
#include <QScriptContext>

struct PosInfo
{
    QString file;
    qint64  line   = -1;
    qint64  column = -1;
};

class MyScriptEngineAgent :public QObject,  public QScriptEngineAgent
{
    Q_OBJECT

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


    PosInfo currentPos(){
        return mCurPos;
    }

signals:
    void posChanged(PosInfo info);

private:
    PosInfo mCurPos;
    QMap<qint64, QString> mFileMap;

};

#endif // MYSCRIPTENGINEAGENT_H
