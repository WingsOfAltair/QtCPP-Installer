#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QTabBar>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrent>
#include <QStandardPaths>
#include <QEventLoop>
#include <QUrl>
#include <QFileInfo>
#include <atomic>
#include <bit7z/bit7zlibrary.hpp>
#include <bit7z/bitfileextractor.hpp>
#include <bit7z/bitexception.hpp>
#include <bit7z/bitinputarchive.hpp>
#include <iostream>

bool quitApp = false;
std::atomic<bool> m_cancelExtraction {false};
QFuture<void> m_extractionFuture;
qint64 totalFileSize;

DownloadManager *manager;
DownloadControlFlags *m_controlFlags;
QThread *workerThread;

QString url = "http://localhost/Data.bin";
QString file;
QString fileNameStr = "/Data.bin";
QString filePath = "Data.bin";

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_extractionFuture.isRunning()) {
        qDebug() << "App closing: cancelling extraction...";
        m_cancelExtraction.store(true);

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);

        QObject::connect(&m_extractionWatcher, &QFutureWatcher<void>::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

        timer.start(5000);  // 5 seconds timeout
        loop.exec();

        if (m_extractionFuture.isRunning()) {
            qWarning() << "Extraction did not stop within timeout.";
        } else {
            qDebug() << "Extraction stopped cleanly.";
        }
    }

    event->accept();
}

QString MainWindow::getExeFolder() {
    return QApplication::applicationDirPath();
}

void MainWindow::cancelExtraction() {
    m_cancelExtraction.store(true, std::memory_order_relaxed);
}

QString MainWindow::extractEmbeddedDll() {
    QString dllPath = getExeFolder() + "/7z.dll";

    QFile dll(":/dependencies/7z.dll");
    if (!dll.exists()) {
        qWarning() << "DLL resource does not exist!";
        return QString();
    }
    if (!dll.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open DLL resource!";
        return QString();
    }

    QFile outFile(dllPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open output file:" << dllPath;
        dll.close();  // Close before returning!
        return QString();
    }

    QByteArray data = dll.readAll();
    dll.close();  // Close resource here immediately after reading

    qint64 written = outFile.write(data);
    if (written != data.size()) {
        qWarning() << "DLL write incomplete";
    }

    outFile.flush();
    outFile.close();

    qDebug() << "DLL extracted to" << dllPath;

    return dllPath;
}

void MainWindow::extractResourceArchive(const QString& resourcePath, const QString& outputDir, const QString& password) {
    QString archivePath = getExeFolder() + "/Data.bin";

    QString dllPath = extractEmbeddedDll();
    if (dllPath.isEmpty()) {
        qCritical() << "Failed to extract 7z.dll";
        return;
    }

    m_cancelExtraction.store(false);

    m_extractionFuture = QtConcurrent::run([=]() {
        if (ui->tabWidget->currentIndex() == 3)
        {
            ui->nextButton->setDisabled(true);
            ui->backButton->setDisabled(true);
        }

        /*QFile resourceFile(resourcePath);
        if (!resourceFile.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open resource:" << resourcePath;
            return;
        }

        QByteArray archiveData = resourceFile.readAll();
        resourceFile.close();

        QString tempPath = QDir::temp().filePath("temp_archive.7z");
        QFile tempFile(tempPath);
        if (!tempFile.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to create temp file:" << tempPath;
            return;
        }
        tempFile.write(archiveData);
        tempFile.close();*/

        try {
            bit7z::Bit7zLibrary lib(dllPath.toStdString());
            bit7z::BitFileExtractor extractor(lib, bit7z::BitFormat::SevenZip);

            if (!password.isEmpty()) {
                extractor.setPassword(password.toStdString());
            }

            //bit7z::BitInputArchive archive(extractor, tempPath.toStdString());
            bit7z::BitInputArchive archive(extractor, archivePath.toStdString());
            uint64_t totalSize = 0;
            for (const auto& item : archive) {
                totalSize += item.size();
            }

            QElapsedTimer timer;
            timer.start();

            extractor.setProgressCallback([this, totalSize, timer](uint64_t processedSize) -> bool {
                if (m_cancelExtraction.load(std::memory_order_relaxed)) {
                    qDebug() << "Extraction canceled by user.";
                    //(archivePath);
                    ("7z.dll");
                    return false; // Stops extraction
                }
                int percent = totalSize > 0 ? static_cast<int>((processedSize * 100) / totalSize) : 0;

                // Calculate time remaining
                qint64 elapsedMs = timer.elapsed();
                double elapsedSec = elapsedMs / 1000.0;
                QString remainingText = "Calculating...";
                if (elapsedSec > 0 && processedSize > 0) {
                    double speed = processedSize / elapsedSec; // bytes/sec
                    double remainingSec = (totalSize - processedSize) / speed;
                    int minutes = static_cast<int>(remainingSec) / 60;
                    int seconds = static_cast<int>(remainingSec) % 60;
                    remainingText = QString("Estimated Time Remaining: %1:%2")
                                        .arg(minutes, 2, 10, QLatin1Char('0'))
                                        .arg(seconds, 2, 10, QLatin1Char('0'));
                }

                QMetaObject::invokeMethod(this, [this, percent, remainingText]() {
                    ui->progressBar->setValue(percent);
                    ui->labelTime->setText(remainingText);
                }, Qt::QueuedConnection);
                return true; // continue extraction
            });

            QDir().mkpath(outputDir);
            extractor.extract(archivePath.toStdString(), outputDir.toStdString());

            QMetaObject::invokeMethod(this, [this]() {
                qDebug() << "Extraction Completed!";
                ui->lblInstallationStatus->setText("Installing Completed.");
                ui->nextButton->setDisabled(false);
                ui->backButton->setDisabled(true);
                ui->labelTime->setText("Installation Completed.");
            }, Qt::QueuedConnection);

        } catch (const bit7z::BitException& e) {
            qWarning() << "Extraction failed:" << QString::fromStdString(e.what());
        }

        //(archivePath);
        (dllPath);
    });
    m_extractionWatcher.setFuture(m_extractionFuture);
}

void MainWindow::NextStep()
{
    ui->tabWidget->setCurrentIndex(ui->tabWidget->currentIndex() + 1);
    ui->backButton->setText("Back");
    if (ui->tabWidget->currentIndex() != 4)
    {
        quitApp = false;
    }
    if (ui->tabWidget->currentIndex() == 2) {
        ui->nextButton->setDisabled(true);
        ui->backButton->setDisabled(true);

        if (QFile::exists(filePath)) {
            if (QFile::exists(file + ".meta")) {
                this->onStartClicked();
            } else {
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(nullptr, "File Exists",
                                              "The file already exists. Do you want to overwrite?",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    QFile::remove(filePath); // Delete old file
                    this->onStartClicked();
                } else {
                    qDebug() << "User chose not to overwrite.";
                    ui->tabWidget->setCurrentIndex(ui->tabWidget->currentIndex() + 1);
                }
            }
        } else {
            this->onStartClicked();
        }
    }
    if (ui->tabWidget->currentIndex() == 3) {
        ui->nextButton->setDisabled(true);
        ui->backButton->setDisabled(true);
        QString outputDir = ui->txtInstallationPath->toPlainText();
        QDir dir(outputDir);
        //dir.cdUp();  // Goes one level up (removes the last folder)

        QString parentPath = dir.absolutePath();  // This is the path without the last folder
        ui->lblInstallationStatus->setText("Installing...");
        extractResourceArchive(":/data/Data.bin", parentPath, "bashar");
    }
    if (ui->tabWidget->currentIndex() == 4 && !quitApp)
    {
        ui->nextButton->setText("Finish");
        ui->nextButton->setDisabled(false);
        ui->backButton->setDisabled(true);
        quitApp = true;
        return;
    }
    if (ui->tabWidget->currentIndex() == 4 && quitApp)
    {
        if (!ui->cbLaunch->isChecked())
        {
            QApplication::quit();
        }
        else {
            QString exePath = QDir::cleanPath(ui->txtInstallationPath->toPlainText() + "/ScrutaNet-Server-GUI.exe");

            bool started = QProcess::startDetached(exePath, {}, ui->txtInstallationPath->toPlainText());

            if (!started) {
                qDebug() << "Failed to start:" << exePath;
            }
            QApplication::quit();
        }
    }
}

void MainWindow::BackStep()
{
    ui->tabWidget->setCurrentIndex(ui->tabWidget->currentIndex() - 1);

    if (ui->tabWidget->currentIndex() == 0 && quitApp) {
        QApplication::quit();
    }

    if (ui->tabWidget->currentIndex() == 0) {
        ui->backButton->setText("Exit");
        quitApp = true;
    } else {
        ui->backButton->setText("Back");
    }
}

void MainWindow::onStartClicked() {
    if (url.isEmpty() || file.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please provide both URL and output file path.");
        return;
    }

    if (QFile::exists(file + ".meta")) {
        QMessageBox::StandardButton ans = QMessageBox::question(this, "Resume?", "Resume previous download?");
        if (ans == QMessageBox::No) {
            if (QFile::exists(filePath)) {
                QFile::remove(filePath);
            }
            if (QFile::exists(file + ".meta")) {
                QFile::remove(file + ".meta");
            }
        }
    }

    auto controlFlags = new DownloadControlFlags();
    if (m_controlFlags)
        delete m_controlFlags;

    m_controlFlags = new DownloadControlFlags();
    manager = new DownloadManager(url, file, m_controlFlags);

    workerThread = new QThread;

    manager->moveToThread(workerThread);
    connect(workerThread, &QThread::started, manager, &DownloadManager::start);
    connect(workerThread, &QThread::finished, manager, &QObject::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);
    connect(manager, &DownloadManager::finished, this, [=]() {
        ui->progressBarDownload->setValue(100);
        ui->retryLabel->setText("");
        ui->sizeLabel->setText("");
        ui->etaLabel->setText("Download Complete.");
        ui->speedLabel->setText("");
        ui->startButton->setDisabled(true);
        ui->resumeButton->setDisabled(true);
        ui->cancelButton->setDisabled(true);
        ui->nextButton->setDisabled(false);
        workerThread->quit();
        workerThread->wait();
    });
    connect(manager, &DownloadManager::error, this, [=](const QString &msg) {
        if (m_controlFlags) {
            delete m_controlFlags;
            m_controlFlags = nullptr;
        }
        workerThread->quit();
        workerThread->wait();

        manager = nullptr;
        workerThread = nullptr;

        QApplication::quit();
    });

    connect(manager, &DownloadManager::progress, this, [this](qint64 downloaded, qint64 total, double speed, int eta) {
                int percent = (total > 0) ? static_cast<int>((downloaded * 100) / total) : 0;
                ui->progressBarDownload->setValue(percent);
                ui->sizeLabel->setText(QString("%1 / %2")
                                           .arg(humanSize(downloaded))
                                           .arg(humanSize(total)));
                ui->speedLabel->setText(QString("Speed: %1 MB/s").arg(speed, 0, 'f', 2));
                ui->etaLabel->setText(QString("ETA: %1 sec").arg(eta >= 0 ? eta : -1));
            });

    workerThread->start();
}

QString MainWindow::humanSize(qint64 bytes) {
    double size = bytes;
    QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    while (size >= 1024.0 && i < units.size() - 1) {
        size /= 1024.0;
        i++;
    }
    return QString("%1 %2").arg(size, 0, 'f', 2).arg(units[i]);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);

    DownloadControlFlags *m_controlFlags = nullptr;
    DownloadManager *manager = nullptr;
    QThread *workerThread = nullptr;

    ui->tabWidget->tabBar()->hide();
    ui->progressBar->setRange(0, 100);
    ui->progressBarDownload->setRange(0, 100);
    connect(ui->nextButton, &QPushButton::clicked, this, &MainWindow::NextStep);
    connect(ui->backButton, &QPushButton::clicked, this, &MainWindow::BackStep);
    connect(ui->startButton, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(ui->resumeButton, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &MainWindow::onCancelClicked);

    ui->backButton->setText("Exit");
    quitApp = true;
    file = getExeFolder() + fileNameStr;

    ui->startButton->hide();

    ui->txtInstallationPath->setText("C:\\Plancksoft\\ScrutaNet\\");
}

void MainWindow::onPauseClicked() {
    isPaused = !isPaused;
    if (isPaused) {
        if (m_controlFlags)
            m_controlFlags->paused.store(false);
        ui->resumeButton->setText("Pause Download");
    } else {
        if (m_controlFlags)
            m_controlFlags->paused.store(true);
        ui->resumeButton->setText("Resume Download");
    }
}

void MainWindow::onCancelClicked() {
    if (m_controlFlags)
        m_controlFlags->stopped.store(true);

    ui->cancelButton->setDisabled(true);
    ui->resumeButton->setText("Download Canceled");
    ui->resumeButton->setDisabled(true);
    ui->etaLabel->setText("Download Canceled");
    ui->sizeLabel->clear();
    ui->speedLabel->clear();
    ui->progressBarDownload->setValue(0);

    QApplication::quit();
}

MainWindow::~MainWindow() {
    delete ui;
}
