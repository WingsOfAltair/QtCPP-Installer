#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProgressBar>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void NextStep();
    void BackStep();

private:
    Ui::MainWindow *ui;
    void extractResourceArchive(const QString& resourcePath, const QString& outputDir, const QString& password = QString());
};
#endif // MAINWINDOW_H
