#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QStandardPaths>
#include <bit7z/bit7zlibrary.hpp>
#include <bit7z/bitfileextractor.hpp>
#include <bit7z/bitexception.hpp>

void extractResourceArchive(const QString& resourcePath, const QString& outputDir, const QString& password = QString())
{
    QFile resourceFile(resourcePath);
    if (!resourceFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open resource:" << resourcePath;
        return;
    }

    QByteArray archiveData = resourceFile.readAll();
    resourceFile.close();

    // Write the resource data to a temp file
    QString tempPath = "temp_archive.7z";

    QFile tempFile(tempPath);
    if (!tempFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to create temp file:" << tempPath;
        return;
    }
    tempFile.write(archiveData);
    tempFile.close();

    try {
        // Initialize bit7z library with path to 7z.dll (adjust if needed)
        bit7z::Bit7zLibrary lib("7z.dll");

        // Create extractor for 7z format
        bit7z::BitFileExtractor extractor(lib, bit7z::BitFormat::SevenZip);

        // Make sure output directory exists
        QDir().mkpath(outputDir);

        if (password.isEmpty()) {
            extractor.extract(tempPath.toStdString(), outputDir.toStdString());
        } else {
            extractor.setPassword(password.toStdString());
            extractor.extract(tempPath.toStdString(), outputDir.toStdString());
        }

        qDebug() << "Extraction completed successfully to:" << outputDir;
    } catch (const bit7z::BitException& e) {
        qWarning() << "Extraction failed:" << QString::fromStdString(e.what());
    }

    // Optionally delete the temp file here if you want
    QFile::remove(tempPath);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QString outputDir = ("");

    extractResourceArchive(":/data/data.7z", outputDir, /*password=*/"bashar");
}

MainWindow::~MainWindow()
{
    delete ui;
}
