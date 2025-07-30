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
#include <atomic>
#include <bit7z/bit7zlibrary.hpp>
#include <bit7z/bitfileextractor.hpp>
#include <bit7z/bitexception.hpp>
#include <bit7z/bitinputarchive.hpp>
#include "utils.h"

bool quitApp = false;
std::atomic<bool> m_cancelExtraction {false};
QFuture<void> m_extractionFuture;

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

QString getExeFolder() {
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
                    //QFile::remove(archivePath);
                    QFile::remove("7z.dll");
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
                qDebug() << "Extraction completed!";
                ui->nextButton->setDisabled(false);
                ui->backButton->setDisabled(true);
                ui->labelTime->setText("Installation completed.");
            }, Qt::QueuedConnection);

        } catch (const bit7z::BitException& e) {
            qWarning() << "Extraction failed:" << QString::fromStdString(e.what());
        }

        //QFile::remove(archivePath);
        QFile::remove(dllPath);
    });
    m_extractionWatcher.setFuture(m_extractionFuture);
}

void MainWindow::NextStep()
{
    ui->tabWidget->setCurrentIndex(ui->tabWidget->currentIndex() + 1);
    if (ui->tabWidget->currentIndex() == 2) {

    }
    if (ui->tabWidget->currentIndex() == 3) {
        ui->nextButton->setDisabled(true);
        ui->backButton->setDisabled(true);
        QString outputDir = ui->txtInstallationPath->toPlainText();
        QDir dir(outputDir);
        //dir.cdUp();  // Goes one level up (removes the last folder)

        QString parentPath = dir.absolutePath();  // This is the path without the last folder
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
}

void MainWindow::onStartClicked() {
    if (manager) return;

    QString url = "http://localhost/Data.bin";
    QString file = "Data.bin";

    if (file.isEmpty()) {
        QUrl qurl(url);
        file = qurl.fileName().isEmpty() ? "Data.bin" : qurl.fileName();
    }

    // Resume prompt
    if (QFile::exists(file + ".meta")) {
        QMessageBox::StandardButton ans = QMessageBox::question(
            this,
            "Resume Download",
            "A partial download was found.\nDo you want to resume? Choose No if you want to start fresh.",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes
            );
        if (ans == QMessageBox::No) {
            QFile::remove(file + ".meta");
            for (int i = 0; i < 10; i++) {
                QFile::remove(file + QString(".part%1").arg(i));
            }
        }
    }

    manager = new DownloadManager(url, file);
    managerThread = new QThread(this);  // parented to MainWindow

    manager->moveToThread(managerThread);

    connect(managerThread, &QThread::started, manager, &DownloadManager::start);

    connect(manager, &DownloadManager::finished, this, [=]() {
        QMessageBox::information(this, "Done", "Download complete!");
        ui->retryLabel->clear();
        managerThread->quit();
    });

    connect(manager, &DownloadManager::error, this, [=](const QString &msg) {
        QMessageBox::critical(this, "Error", msg);
    });

    /*connect(manager, &DownloadManager::retryStatus, this, [=](int segmentIndex, int attempt) {
        ui->retryLabel->setText(QString("Retrying segment %1 (Attempt %2)...").arg(segmentIndex).arg(attempt));
    });*/

    connect(manager, &DownloadManager::progress, this, [=](qint64 downloaded, qint64 total, double speedMBps, int eta) {
        ui->progressBarDownload->setValue(total > 0 ? static_cast<int>((downloaded * 100) / total) : 0);
        ui->etaLabel->setText(QString("ETA: %1 sec").arg(eta));
        ui->speedLabel->setText(QString("Speed: %1 MB/s").arg(speedMBps, 0, 'f', 2));
        ui->sizeLabel->setText(QString("%1 / %2").arg(humanSize(downloaded)).arg(humanSize(total)));
    });

    connect(managerThread, &QThread::finished, this, [=]() {
        manager->deleteLater();
        managerThread->deleteLater();
        manager = nullptr;
        managerThread = nullptr;
    });

    managerThread->start();
}

void MainWindow::onPauseClicked() {
    if (!manager) return;
    if (isPaused) {
        manager->resume();
        ui->pauseButton->setText("Pause");
        isPaused = false;
    } else {
        manager->pause();
        ui->pauseButton->setText("Resume");
        isPaused = true;
    }
}

void MainWindow::onCancelClicked() {
    if (!manager) return;
    manager->cancel();

    ui->retryLabel->clear();
    ui->progressBarDownload->setValue(0);
    ui->etaLabel->setText("ETA: 0 sec");
    ui->speedLabel->setText("Speed: 0 MB/s");
    ui->sizeLabel->setText("0 / 0");
    isPaused = false;
    ui->pauseButton->setText("Pause");
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    ui->tabWidget->tabBar()->hide();
    ui->progressBar->setRange(0, 100);
    ui->progressBarDownload->setRange(0, 100);
    connect(ui->nextButton, &QPushButton::clicked, this, &MainWindow::NextStep);
    connect(ui->backButton, &QPushButton::clicked, this, &MainWindow::BackStep);
    connect(ui->startButton, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(ui->pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &MainWindow::onCancelClicked);
    ui->txtInstallationPath->setText("C:\\Qt6CPP-App\\");
}

MainWindow::~MainWindow() {
    delete ui;
}
