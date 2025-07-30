#include "downloadmanager.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QDebug>
#include <QEventLoop>

DownloadManager::DownloadManager(const QString &url, const QString &outputFile, QObject *parent)
    : QObject(parent), m_url(url), m_outputFile(outputFile)
{
    m_totalSize = getRemoteFileSize();
    if (m_totalSize <= 0) {
        emit error("Failed to get remote file size or file is empty.");
    }
}

DownloadManager::~DownloadManager()
{
    qDeleteAll(m_segments);
    m_segments.clear();
}

qint64 DownloadManager::getRemoteFileSize()
{
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(m_url)};
    QNetworkReply *reply = manager.head(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return -1;
    }

    QVariant length = reply->header(QNetworkRequest::ContentLengthHeader);
    reply->deleteLater();

    return length.isValid() ? length.toLongLong() : -1;
}

void DownloadManager::initializeSegments(int segmentCount)
{
    qint64 segmentSize = m_totalSize / segmentCount;
    for (int i = 0; i < segmentCount; ++i) {
        qint64 start = i * segmentSize;
        qint64 end = (i == segmentCount - 1) ? m_totalSize - 1 : (start + segmentSize - 1);
        QString partFile = QString("%1.part%2").arg(m_outputFile).arg(i);

        SegmentDownloader *seg = new SegmentDownloader(m_url, partFile, start, end, this);
        seg->index = i;

        connect(seg, &SegmentDownloader::progress, this, &DownloadManager::segmentProgress);
        connect(seg, &SegmentDownloader::finished, this, &DownloadManager::segmentFinished);
        connect(seg, &SegmentDownloader::error, this, &DownloadManager::segmentError);

        m_segments.append(seg);
    }
}

void DownloadManager::start()
{
    if (m_segments.size() > 0) return; // Already started

    if (m_totalSize <= 0) {
        emit error("Cannot start download, invalid file size.");
        return;
    }

    initializeSegments();

    m_timer.start();
    m_lastBytesReceived = 0;

    for (SegmentDownloader *seg : qAsConst(m_segments)) {
        seg->start();
    }

    m_paused = false;
    m_cancelled = false;
}

void DownloadManager::pause()
{
    if (m_paused) return;
    m_paused = true;
    for (SegmentDownloader *seg : qAsConst(m_segments)) {
        seg->pause();
    }
}

void DownloadManager::resume()
{
    if (!m_paused) return;
    m_paused = false;
    for (SegmentDownloader *seg : qAsConst(m_segments)) {
        seg->resume();
    }
}

void DownloadManager::cancel()
{
    if (m_cancelled) return;
    m_cancelled = true;
    for (SegmentDownloader *seg : qAsConst(m_segments)) {
        seg->stop();
    }
    m_segments.clear();
}

void DownloadManager::segmentProgress(qint64 /*bytesReceived*/, qint64 /*bytesTotal*/) {
    qint64 totalDownloaded = 0;
    qint64 totalSize = 0;

    // Sum bytes received and total size of all segments
    for (SegmentDownloader *seg : m_segments) {
        totalDownloaded += seg->bytesReceived();
        totalSize += seg->size();  // e.g. segment size = endPos - startPos + 1
    }

    qint64 elapsedMs = m_timer.elapsed();
    if (elapsedMs == 0) elapsedMs = 1; // avoid div by zero

    // Calculate speed as average bytes per second
    double speedBytesPerSecond = (totalDownloaded * 1000.0) / elapsedMs;
    double speedMBps = speedBytesPerSecond / (1024 * 1024);

    qint64 bytesLeft = totalSize - totalDownloaded;
    int etaSeconds = speedBytesPerSecond > 0 ? static_cast<int>(bytesLeft / speedBytesPerSecond) : -1;

    emit progress(totalDownloaded, totalSize, speedMBps, etaSeconds);
}

void DownloadManager::segmentFinished()
{
    bool allDone = std::all_of(m_segments.begin(), m_segments.end(),
                               [](SegmentDownloader *seg) { return seg->isComplete(); });
    if (allDone) {
        mergeSegments();
        emit finished();
    }
}

void DownloadManager::segmentError(const QString &msg)
{
    emit error(msg);
}

void DownloadManager::mergeSegments()
{
    QFile outputFile(m_outputFile);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        emit error(QString("Failed to open output file for merge: %1").arg(m_outputFile));
        return;
    }

    for (SegmentDownloader *seg : qAsConst(m_segments)) {
        QFile partFile(seg->outputFile());
        if (!partFile.open(QIODevice::ReadOnly)) {
            emit error(QString("Failed to open part file: %1").arg(seg->outputFile()));
            outputFile.close();
            return;
        }
        outputFile.write(partFile.readAll());
        partFile.close();
    }
    outputFile.close();

    // Clean up part files after merge
    for (SegmentDownloader *seg : qAsConst(m_segments)) {
        QFile::remove(seg->outputFile());
    }
}
