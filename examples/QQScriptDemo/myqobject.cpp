#include "myqobject.h"

MyQObject::MyQObject(QObject *parent)
    : QObject{parent}
{

}

QString MyQObject::myFunc(QString info)
{
    return "MYQObject:++" + info;
}
