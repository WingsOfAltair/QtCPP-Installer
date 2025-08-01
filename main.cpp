#include "mainwindow.h"
#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/appicon.png"));
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    MainWindow w;
    w.setWindowTitle("ScrutaNet Installer");
    w.show();

    int result = app.exec();
    return result;
}
