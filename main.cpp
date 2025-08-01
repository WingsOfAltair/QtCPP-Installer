#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/appicon.png"));
    MainWindow w;
    w.setWindowTitle("ScrutaNet Installer");
    w.show();

    int result = app.exec();
    return result;
}
