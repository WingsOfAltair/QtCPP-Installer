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
    void closeEvent(QCloseEvent* event);
    void onBrowseClicked();
    void onPauseExtraction();
    void onCancelExtraction();
    void onLogMessage(const QString &msg);
    void loadStyleSheet(const QString &path);
    void init_ui_assets();
    void toggleTheme();
    void toggleInstallationDetails();

private:
    Ui::MainWindow *ui;
    QString humanSize(qint64 bytes);
    void extractResourceArchive(const QString& resourcePath, const QString& outputDir, const QString& password = QString());
    QFutureWatcher<void> m_extractionWatcher;
    DownloadManager *manager;
    QThread *workerThread;
    bool isPaused;
    bool isPausedExtraction;
    QString getExeFolder();
};
#endif // MAINWINDOW_H
