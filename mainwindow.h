#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProgressBar>
#include <QFutureWatcher>
#include <QCloseEvent>
#include "downloadmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QThread;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void NextStep();
    void BackStep();
    void onStartClicked();
    void onPauseClicked();
    void onCancelClicked();
    QString extractEmbeddedDll();
    void cancelExtraction();
    void closeEvent(QCloseEvent* event);

private:
    Ui::MainWindow *ui;
    QString humanSize(qint64 bytes);
    void extractResourceArchive(const QString& resourcePath, const QString& outputDir, const QString& password = QString());
    QFutureWatcher<void> m_extractionWatcher;
    DownloadManager *manager;
    QThread *workerThread;
    bool isPaused;
};
#endif // MAINWINDOW_H
