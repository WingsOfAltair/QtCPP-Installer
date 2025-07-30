#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProgressBar>
#include <QFutureWatcher>
#include <QCloseEvent>

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
    QString extractEmbeddedDll();
    void cancelExtraction();
    void closeEvent(QCloseEvent* event);

private:
    Ui::MainWindow *ui;
    void extractResourceArchive(const QString& resourcePath, const QString& outputDir, const QString& password = QString());
    QFutureWatcher<void> m_extractionWatcher;
};
#endif // MAINWINDOW_H
