#ifndef BARPROTOTYPE_H
#define BARPROTOTYPE_H

#include <QObject>
#include <QString>

class BarPrototype : public QObject
{
    Q_OBJECT
public:
    explicit BarPrototype(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QString greet() const { return QString("hello from BarProto (QObject)"); }
};

#endif // BARPROTOTYPE_H
