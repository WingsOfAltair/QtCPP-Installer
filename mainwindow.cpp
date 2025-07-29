#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QTabBar>
#include <QtConcurrent/QtConcurrent>
#include <QStandardPaths>
#include <bit7z/bit7zlibrary.hpp>
#include <bit7z/bitfileextractor.hpp>
#include <bit7z/bitexception.hpp>
#include <bit7z/bitinputarchive.hpp>

bool quitApp = false;

void MainWindow::extractResourceArchive(const QString& resourcePath, const QString& outputDir, const QString& password) {
    QString archivePath = "Qt6CPP-App.7z";
    QtConcurrent::run([=]() {
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
            bit7z::Bit7zLibrary lib("7z.dll");
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
    });
}

void MainWindow::NextStep()
{
    ui->tabWidget->setCurrentIndex(ui->tabWidget->currentIndex() + 1);
    if (ui->tabWidget->currentIndex() == 2) {
        ui->nextButton->setDisabled(true);
        ui->backButton->setDisabled(true);
        QString outputDir = ui->txtInstallationPath->toPlainText();
        QDir dir(outputDir);
        dir.cdUp();  // Goes one level up (removes the last folder)

        QString parentPath = dir.absolutePath();  // This is the path without the last folder
        extractResourceArchive(":/data/Qt6CPP-App.7z", parentPath, "bashar");
    }
    if (ui->tabWidget->currentIndex() == 3 && !quitApp)
    {
        ui->nextButton->setText("Finish");
        ui->nextButton->setDisabled(false);
        ui->backButton->setDisabled(true);
        quitApp = true;
        return;
    }
    if (ui->tabWidget->currentIndex() == 3 && quitApp)
    {
        QApplication::quit();
    }
}


void MainWindow::BackStep()
{
    ui->tabWidget->setCurrentIndex(ui->tabWidget->currentIndex() - 1);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    ui->tabWidget->tabBar()->hide();
    ui->progressBar->setRange(0, 100);
    connect(ui->nextButton, &QPushButton::clicked, this, &MainWindow::NextStep);
    connect(ui->backButton, &QPushButton::clicked, this, &MainWindow::BackStep);
    ui->txtInstallationPath->setText("C:\\Qt6CPP-App\\");
}

MainWindow::~MainWindow() {
    delete ui;
}
