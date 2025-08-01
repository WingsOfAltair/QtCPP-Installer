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
#include <QFileDialog>
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

std::atomic<bool> m_pauseExtraction{false};
std::mutex m_pauseMutex;
std::condition_variable m_pauseCv;

DownloadManager *manager;
DownloadControlFlags *m_controlFlags;
QThread *workerThread;

QLabel *nextButtonLabel;
QLabel *backButtonLabel;
QLabel *browseButtonLabel;
QLabel *startDownloadButtonLabel;
QLabel *resumeDownloadButtonLabel;
QLabel *cancelDownloadButtonLabel;
QLabel *resumeInstallationButtonLabel;
QLabel *cancelInstallationButtonLabel;

QString url = "http://192.168.1.29/Data.bin";
QString file;
QString fileNameStr = "/Data.bin";
QString filePath = "Data.bin";
QString dllPath;

bool darkMode = false;

QString getDefaultInstallPath() {
#ifdef Q_OS_WIN
    QString programFiles = qEnvironmentVariable("ProgramFiles");
    if (programFiles.isEmpty())
        programFiles = "C:/Program Files";
    return QDir::toNativeSeparators(programFiles + "/Plancksoft/ScrutaNet");
#elif defined(Q_OS_LINUX)
    return "/Plancksoft/ScrutaNet";  // Or use home: QDir::homePath() + "/Plancksoft/ScrutaNet"
#else
    return QDir::homePath() + "/Plancksoft/ScrutaNet";
#endif
}

void MainWindow::toggleTheme() {
    darkMode = !darkMode;
    if (darkMode) {
        loadStyleSheet(":/themes/dark.qss");
    } else {
        loadStyleSheet(":/themes/light.qss");
    }
}

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

QString MainWindow::extractEmbeddedDll() {
#ifdef Q_OS_WIN
    QString dllPath = getExeFolder() + "/7z.dll";
    QFile dll(":/dependencies/7z.dll");
    if (!dll.exists()) {
        qWarning() << "DLL resource does not exist!";
        return QString();
    }
#elif defined(Q_OS_LINUX)
    QString dllPath = getExeFolder() + "/7z.so";  // Adjust for your system
    QFile dll(":/dependencies/7z.so");
    if (!dll.exists()) {
        qWarning() << "DLL resource does not exist!";
        return QString();
    }
#endif

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

    dllPath = extractEmbeddedDll();
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

                std::unique_lock<std::mutex> lock(m_pauseMutex);
                m_pauseCv.wait(lock, [this]() { return !m_pauseExtraction.load(); });
                lock.unlock();

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
                ui->cancelInstallationButton->setDisabled(true);
                ui->resumeInstallationButton->setDisabled(true);
                if (QFile::exists(dllPath)) {
                    QFile::remove(dllPath);
                }
            }, Qt::QueuedConnection);

        } catch (const bit7z::BitException& e) {
            QMetaObject::invokeMethod(this, [this]() {
                if (m_cancelExtraction.load(std::memory_order_relaxed)) {
                    ui->lblInstallationStatus->setText("Extraction Canceled.");
                    ui->progressBar->setValue(0);
                    ui->nextButton->setDisabled(true);
                    ui->backButton->setDisabled(true);
                    ui->cancelInstallationButton->setDisabled(true);
                    ui->resumeInstallationButton->setDisabled(true);
                    if (QFile::exists(filePath)) {
                        QFile::remove(dllPath);
                    }
                    QApplication::quit();
                } else {
                    ui->lblInstallationStatus->setText("An error has occured during installation. Please run installer as Administrator.");
                    ui->progressBar->setValue(0);
                    ui->nextButton->setDisabled(true);
                    ui->backButton->setDisabled(true);
                    ui->cancelInstallationButton->setDisabled(true);
                    ui->resumeInstallationButton->setDisabled(true);
                    setWindowFlags(windowFlags() | Qt::WindowCloseButtonHint);
                    show();
                }
            }, Qt::QueuedConnection);
            QMetaObject::invokeMethod(this, [msg = QString::fromUtf8(e.what())]() {
                if (!m_cancelExtraction.load(std::memory_order_relaxed)) {
                    QMessageBox::critical(nullptr, "Error", msg);
                }
            }, Qt::QueuedConnection);
        }

        //(archivePath);
    });
    m_extractionWatcher.setFuture(m_extractionFuture);
}

void MainWindow::onPauseExtraction() {
    isPausedExtraction = !isPausedExtraction;
    if (isPausedExtraction)
    {
        resumeInstallationButtonLabel->setText("Resume Installation");
        std::lock_guard<std::mutex> lock(m_pauseMutex);
        m_pauseExtraction.store(true);
        m_pauseCv.notify_one();
    } else {
        resumeInstallationButtonLabel->setText("Pause Installation");
        std::lock_guard<std::mutex> lock(m_pauseMutex);
        m_pauseExtraction.store(false);
        m_pauseCv.notify_one();
    }
}

void MainWindow::onCancelExtraction() {
    m_cancelExtraction.store(true);
}

void MainWindow::NextStep()
{
    ui->tabWidget->setCurrentIndex(ui->tabWidget->currentIndex() + 1);

    backButtonLabel->setText("Back");

    if (ui->tabWidget->currentIndex() != 4)
    {
        quitApp = false;
    }
    if (ui->tabWidget->currentIndex() == 2) {
        ui->nextButton->setDisabled(true);
        ui->backButton->setDisabled(true);

        filePath = getExeFolder() + fileNameStr;
        if (QFile::exists(filePath)) {
            if (QFile::exists(file + ".meta")) {
                this->onStartClicked();
            } else {
                QMessageBox msgBox;
                msgBox.setIcon(QMessageBox::Question);
                msgBox.setWindowTitle("File Exists");
                msgBox.setText("The file already exists. Do you want to overwrite?");

                // Add standard Yes and No buttons
                msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

                // Access and style buttons individually
                QAbstractButton  *yesButton = msgBox.button(QMessageBox::Yes);
                QAbstractButton  *noButton = msgBox.button(QMessageBox::No);

                yesButton->setObjectName("yesBtn");
                noButton->setObjectName("noBtn");

                msgBox.setStyleSheet(
                    "#yesBtn { "
                    "   background-color: #0078D7; "   // Blue
                    "   color: white; "
                    "   border-radius: 6px; "
                    "   padding: 8px 16px; "
                    "   font-weight: bold; "
                    "} "
                    "#yesBtn:hover { background-color: #005A9E; } "  // Darker blue on hover

                    "#noBtn { "
                    "   background-color: #FF8C00; "   // Orange
                    "   color: white; "
                    "   border-radius: 6px; "
                    "   padding: 8px 16px; "
                    "   font-weight: bold; "
                    "} "
                    "#noBtn:hover { background-color: #E67300; } "   // Darker orange on hover
                    );

                // Execute the message box
                int reply = msgBox.exec();

                if (reply == QMessageBox::Yes) {
                    QFile::remove(filePath);
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
        extractResourceArchive(":/data/Data.bin", parentPath, "ah*&62I(FFqwrhg12r089YFDW(213r");
    }
    if (ui->tabWidget->currentIndex() == 4 && !quitApp)
    {
        nextButtonLabel->setText("Finish");
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
#ifdef Q_OS_WIN
            QString exePath = QDir::cleanPath(ui->txtInstallationPath->toPlainText() + "/ScrutaNet-Server-GUI.exe");

            QFile file(exePath);
            if (file.exists()) {
                bool started = QProcess::startDetached(exePath, {}, ui->txtInstallationPath->toPlainText());
                if (!started) {
                    qDebug() << "Failed to start:" << exePath;
                }
            } else {
                qDebug() << "File does not exist:" << exePath;
            }

#elif defined(Q_OS_LINUX)
            QString exePath = QDir::cleanPath(ui->txtInstallationPath->toPlainText() + "/ScrutaNet-Server-GUI");

            QFile file(exePath);
            if (file.exists()) {
                // Set permissions rwxr-xr-x = 755
                QFileDevice::Permissions perms = QFileDevice::ReadOwner
                                                 | QFileDevice::WriteOwner
                                                 | QFileDevice::ExeOwner
                                                 | QFileDevice::ReadGroup
                                                 | QFileDevice::ExeGroup
                                                 | QFileDevice::ReadOther
                                                 | QFileDevice::ExeOther;

                bool success = file.setPermissions(perms);
                if (!success) {
                    qDebug() << "Failed to set permissions on" << exePath;
                } else {
                    qDebug() << "Permissions set to 755 for" << exePath;
                }

                bool started = QProcess::startDetached(exePath, {}, ui->txtInstallationPath->toPlainText());
                if (!started) {
                    qDebug() << "Failed to start:" << exePath;
                }

            } else {
                qDebug() << "File does not exist:" << exePath;
            }
#endif
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
        backButtonLabel->setText("Exit");

        quitApp = true;
    } else {
        backButtonLabel->setText("Back");
    }
}

void MainWindow::onStartClicked() {
    if (url.isEmpty() || file.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Please provide both URL and output file path.");
        return;
    }

    if (QFile::exists(file + ".meta")) {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setWindowTitle("Resume?");
        msgBox.setText("Resume previous download?");

        // Add standard Yes and No buttons
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

        // Access and style buttons individually
        QAbstractButton  *yesButton = msgBox.button(QMessageBox::Yes);
        QAbstractButton  *noButton = msgBox.button(QMessageBox::No);

        yesButton->setObjectName("yesBtn");
        noButton->setObjectName("noBtn");

        msgBox.setStyleSheet(
            "#yesBtn { "
            "   background-color: #0078D7; "   // Blue
            "   color: white; "
            "   border-radius: 6px; "
            "   padding: 8px 16px; "
            "   font-weight: bold; "
            "} "
            "#yesBtn:hover { background-color: #005A9E; } "  // Darker blue on hover

            "#noBtn { "
            "   background-color: #FF8C00; "   // Orange
            "   color: white; "
            "   border-radius: 6px; "
            "   padding: 8px 16px; "
            "   font-weight: bold; "
            "} "
            "#noBtn:hover { background-color: #E67300; } "   // Darker orange on hover
            );
        int reply = msgBox.exec();

        if (reply == QMessageBox::No) {
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
        ui->startDownloadButton->setDisabled(true);
        ui->resumeDownloadButton->setDisabled(true);
        ui->cancelDownloadButton->setDisabled(true);
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

void MainWindow::loadStyleSheet(const QString &path) {
    QFile file(path);
    if (file.open(QFile::ReadOnly)) {
        QString qss = QString::fromUtf8(file.readAll());
        qApp->setStyleSheet(qss);
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    this->setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
    this->setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    this->setFixedSize(800, 658);

    loadStyleSheet(":/themes/light.qss");

    DownloadControlFlags *m_controlFlags = nullptr;
    DownloadManager *manager = nullptr;
    QThread *workerThread = nullptr;

    ui->tabWidget->tabBar()->hide();
    ui->progressBar->setRange(0, 100);
    ui->progressBarDownload->setRange(0, 100);
    connect(ui->nextButton, &QPushButton::clicked, this, &MainWindow::NextStep);
    connect(ui->backButton, &QPushButton::clicked, this, &MainWindow::BackStep);
    connect(ui->startDownloadButton, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(ui->resumeDownloadButton, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    connect(ui->cancelDownloadButton, &QPushButton::clicked, this, &MainWindow::onCancelClicked);
    connect(ui->browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(ui->resumeInstallationButton, &QPushButton::clicked, this, &MainWindow::onPauseExtraction);
    connect(ui->cancelInstallationButton, &QPushButton::clicked, this, &MainWindow::onCancelExtraction);
    connect(ui->darkModeCB, &QCheckBox::clicked, this, [=]() {
        this->toggleTheme();
    });

    quitApp = true;
    file = getExeFolder() + fileNameStr;

    isPaused = false;
    ui->resumeDownloadButton->setText("Pause Download");
    isPausedExtraction = false;
    m_pauseExtraction.store(false);
    ui->resumeInstallationButton->setText("Pause Installation");

    ui->startDownloadButton->hide();

    QString installPath = getDefaultInstallPath();
    QDir dir(installPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    qDebug() << "Installing to:" << installPath;
    ui->txtInstallationPath->setText(installPath);

    init_ui_assets();
}

void MainWindow::init_ui_assets() {
    ui->nextButton->setIcon(QIcon(":/icons/Blue-Button.png"));
    ui->nextButton->setIconSize(QSize(128, 48));
    ui->nextButton->setFixedSize(128, 48);
    ui->nextButton->setText("");

    nextButtonLabel = new QLabel("Next", ui->nextButton);
    nextButtonLabel->setAlignment(Qt::AlignCenter);
    nextButtonLabel->setStyleSheet("color: white; background: transparent; font-weight: bold;");
    nextButtonLabel->setGeometry(ui->nextButton->rect());

    ui->backButton->setIcon(QIcon(":/icons/Orange-Button.png"));
    ui->backButton->setIconSize(QSize(128, 48));
    ui->backButton->setFixedSize(128, 48);
    ui->backButton->setText("");

    backButtonLabel = new QLabel("Exit", ui->backButton);
    backButtonLabel->setAlignment(Qt::AlignCenter);
    backButtonLabel->setStyleSheet("color: white; background: transparent; font-weight: bold;");
    backButtonLabel->setGeometry(ui->backButton->rect());

    ui->browseButton->setIcon(QIcon(":/icons/Blue-Button.png"));
    ui->browseButton->setIconSize(QSize(128, 48));
    ui->browseButton->setFixedSize(128, 48);
    ui->browseButton->setText("");

    browseButtonLabel = new QLabel("Browse", ui->browseButton);
    browseButtonLabel->setAlignment(Qt::AlignCenter);
    browseButtonLabel->setStyleSheet("color: white; background: transparent; font-weight: bold;");
    browseButtonLabel->setGeometry(ui->browseButton->rect());

    ui->startDownloadButton->setIcon(QIcon(":/icons/Blue-Button.png"));
    ui->startDownloadButton->setIconSize(QSize(128, 48));
    ui->startDownloadButton->setFixedSize(128, 48);
    ui->startDownloadButton->setText("");

    startDownloadButtonLabel = new QLabel("Start Download", ui->startDownloadButton);
    startDownloadButtonLabel->setAlignment(Qt::AlignCenter);
    startDownloadButtonLabel->setStyleSheet("color: white; background: transparent; font-weight: bold;");
    startDownloadButtonLabel->setGeometry(ui->startDownloadButton->rect());

    ui->resumeDownloadButton->setIcon(QIcon(":/icons/Blue-Button.png"));
    ui->resumeDownloadButton->setIconSize(QSize(128, 48));
    ui->resumeDownloadButton->setFixedSize(128, 48);
    ui->resumeDownloadButton->setText("");

    resumeDownloadButtonLabel = new QLabel("Pause Download", ui->resumeDownloadButton);
    resumeDownloadButtonLabel->setAlignment(Qt::AlignCenter);
    resumeDownloadButtonLabel->setStyleSheet("color: white; background: transparent; font-weight: bold;");
    resumeDownloadButtonLabel->setGeometry(ui->resumeDownloadButton->rect());

    ui->cancelDownloadButton->setIcon(QIcon(":/icons/Orange-Button.png"));
    ui->cancelDownloadButton->setIconSize(QSize(128, 48));
    ui->cancelDownloadButton->setFixedSize(128, 48);
    ui->cancelDownloadButton->setText("");

    cancelDownloadButtonLabel = new QLabel("Cancel Download", ui->cancelDownloadButton);
    cancelDownloadButtonLabel->setAlignment(Qt::AlignCenter);
    cancelDownloadButtonLabel->setStyleSheet("color: white; background: transparent; font-weight: bold;");
    cancelDownloadButtonLabel->setGeometry(ui->cancelDownloadButton->rect());

    ui->resumeInstallationButton->setIcon(QIcon(":/icons/Blue-Button.png"));
    ui->resumeInstallationButton->setIconSize(QSize(128, 48));
    ui->resumeInstallationButton->setFixedSize(128, 48);
    ui->resumeInstallationButton->setText("");

    resumeInstallationButtonLabel = new QLabel("Pause Download", ui->resumeInstallationButton);
    resumeInstallationButtonLabel->setAlignment(Qt::AlignCenter);
    resumeInstallationButtonLabel->setStyleSheet("color: white; background: transparent; font-weight: bold;");
    resumeInstallationButtonLabel->setGeometry(ui->resumeInstallationButton->rect());

    ui->cancelInstallationButton->setIcon(QIcon(":/icons/Orange-Button.png"));
    ui->cancelInstallationButton->setIconSize(QSize(128, 48));
    ui->cancelInstallationButton->setFixedSize(128, 48);
    ui->cancelInstallationButton->setText("");

    cancelInstallationButtonLabel = new QLabel("Cancel Installation", ui->cancelInstallationButton);
    cancelInstallationButtonLabel->setAlignment(Qt::AlignCenter);
    cancelInstallationButtonLabel->setStyleSheet("color: white; background: transparent; font-weight: bold;");
    cancelInstallationButtonLabel->setGeometry(ui->cancelInstallationButton->rect());

    ui->imgScrutaNetDownload->setPixmap(QPixmap(":/icons/ScrutaNet.png"));
    ui->imgScrutaNetDownload->setScaledContents(true);

    ui->imgScrutaNetInstall->setPixmap(QPixmap(":/icons/ScrutaNet.png"));
    ui->imgScrutaNetInstall->setScaledContents(true);
}

void MainWindow::onPauseClicked() {
    isPaused = !isPaused;
    if (isPaused) {
        if (m_controlFlags)
            m_controlFlags->paused.store(true);
        resumeDownloadButtonLabel->setText("Resume Download");
    } else {
        if (m_controlFlags)
            m_controlFlags->paused.store(false);
        resumeDownloadButtonLabel->setText("Pause Download");
    }
}

void MainWindow::onCancelClicked() {
    if (m_controlFlags)
        m_controlFlags->stopped.store(true);

    ui->cancelDownloadButton->setDisabled(true);
    ui->resumeDownloadButton->setText("Download Canceled");
    ui->resumeDownloadButton->setDisabled(true);
    ui->etaLabel->setText("Download Canceled");
    ui->sizeLabel->clear();
    ui->speedLabel->clear();
    ui->progressBarDownload->setValue(0);

    QApplication::quit();
}

void MainWindow::onBrowseClicked() {
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select Installation Folder",
        ui->txtInstallationPath->toPlainText().isEmpty() ? QDir::homePath() : ui->txtInstallationPath->toPlainText(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );

    dir = dir + QDir::separator() + "Plancksoft" + QDir::separator() + "ScrutaNet";

    if (!dir.isEmpty()) {
        ui->txtInstallationPath->setText(dir);
    }
}

MainWindow::~MainWindow() {
    delete ui;
}
