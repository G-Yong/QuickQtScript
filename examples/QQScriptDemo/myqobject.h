#ifndef MYQOBJECT_H
#define MYQOBJECT_H

#include <QObject>

class MyQObject : public QObject
{
    Q_OBJECT

public:
    explicit MyQObject(QObject *parent = nullptr);

    Q_INVOKABLE QString myFunc(QString info);

signals:
};

#endif // MYQOBJECT_H
