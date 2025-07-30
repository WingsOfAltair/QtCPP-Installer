#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    curl_global_init(CURL_GLOBAL_ALL);
    QApplication app(argc, argv);
    MainWindow w;
    w.show();

    int result = app.exec();
    curl_global_cleanup();
    return result;
}
