#include "mainwindow.h"

#include <QApplication>

#ifdef __TINYC__
#include "quickjsTest.h"
#endif

int main(int argc, char *argv[])
{
    // qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication a(argc, argv);

    MainWindow w;
    w.show();

    // // quickjsTest();
    // scriptEngineTest();

    return a.exec();
}
