#include "downloadmanager.h"
#include "utils.h"
#include <QFile>
#include <QDir>
#include <QThread>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <curl/curl.h>
#include <chrono>
#include <QPointer>

DownloadManager::DownloadManager(const QString &url, const QString &output)
    : m_url(url), m_output(output), m_segmentCount(4), m_totalSize(0),
    m_supportsRange(false), m_downloaded(0), m_maxRetries(5),
    m_retryBaseDelay(1000), m_isPaused(false) {
    m_metaFile = m_output + ".meta";
}

bool DownloadManager::checkServerSupport() {
    CURLcode globalInit = curl_global_init(CURL_GLOBAL_ALL);
    if (globalInit != CURLE_OK) {
        return 1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        curl_global_cleanup();
        return 1;
    }

    struct HeaderData {
        bool acceptRanges = false;
    } headerData;

    // Plain C++ header callback - no Qt types here!
    auto headerCallback = [](char* buffer, size_t size, size_t nitems, void* userdata) -> size_t {
        size_t totalSize = size * nitems;
        std::string headerLine(buffer, totalSize);

        // Convert header to lowercase for case-insensitive comparison
        std::string headerLower;
        headerLower.resize(headerLine.size());
        std::transform(headerLine.begin(), headerLine.end(), headerLower.begin(), ::tolower);

        if (headerLower.compare(0, 14, "accept-ranges:") == 0) {
            if (headerLower.find("bytes") != std::string::npos) {
                static_cast<HeaderData*>(userdata)->acceptRanges = true;
            }
        }
        return totalSize;
    };

    std::string urlStr = m_url.toStdString();
    curl_easy_setopt(curl, CURLOPT_URL, urlStr.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);  // HEAD request
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Uncomment for debugging

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        emit error(QStringLiteral("HEAD request failed: %1").arg(curl_easy_strerror(res)));
        curl_easy_cleanup(curl);
        return false;
    }

    double contentLength = 0;
    if(curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentLength) != CURLE_OK || contentLength < 0) {
        contentLength = 0; // fallback if content length missing
    }
    m_totalSize = static_cast<qint64>(contentLength);

    m_supportsRange = headerData.acceptRanges;

    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return true;
}

void DownloadManager::start() {
    if (!checkServerSupport()) {
        emit error("Cannot get file info from server");
        emit finished();
        return;
    }

    m_start = std::chrono::steady_clock::now();
    if (!m_supportsRange || m_totalSize <= 0) {
        m_segmentCount = 1; // fallback to single segment
    }

    qint64 segmentSize = m_totalSize / m_segmentCount;

    m_retryCounts.clear();
    for (int i = 0; i < m_segmentCount; i++) {
        m_retryCounts[i] = 0;

        qint64 start = i * segmentSize;
        qint64 end = (i == m_segmentCount - 1) ? m_totalSize - 1 : (start + segmentSize - 1);

        startSegment(i, start, end);
    }
}

void DownloadManager::startSegment(int index, qint64 start, qint64 end) {
    if (m_output.isEmpty()) {
        qCritical() << "ERROR: m_output is empty! Cannot create part file names.";
        return;
    }
    QString partFile = QString("%1.part%2").arg(m_output).arg(index);
    auto *seg = new SegmentDownloader(m_url, partFile, start, end);
    auto *thread = new QThread();
    seg->moveToThread(thread);

    connect(thread, &QThread::started, seg, &SegmentDownloader::start);

    QPointer<DownloadManager> safeThis(this);
    connect(seg, &SegmentDownloader::progress, this, [safeThis]() {
        if (!safeThis) return;

        safeThis->m_downloaded = 0;
        for (int j = 0; j < safeThis->m_segmentCount; j++) {
            QString partFileName = QString("%1.part%2").arg(safeThis->m_output).arg(j);
            QFile f(partFileName);
            if (f.exists()) {
                qint64 size = f.size();
                if (size >= 0) {
                    safeThis->m_downloaded += size;
                } else {
                    qWarning() << "Failed to get size of" << partFileName;
                }
            } else {
                qWarning() << "File not found:" << partFileName;
            }
        }

        auto elapsed = std::chrono::steady_clock::now() - safeThis->m_start;
        double elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() + 1;
        double speed = safeThis->m_downloaded / elapsedSec;
        int eta = (speed > 0) ? (int)((safeThis->m_totalSize - safeThis->m_downloaded) / speed) : 0;

        emit safeThis->progress(safeThis->m_downloaded, safeThis->m_totalSize, speed / (1024 * 1024), eta);

        safeThis->saveMetadata();
    });


    connect(seg, &SegmentDownloader::error, this, [=](const QString &msg) {
        emit error(msg);
        if (m_retryCounts[index] < m_maxRetries) {
            m_retryCounts[index]++;
            emit retryStatus(index, m_retryCounts[index]);
            qWarning() << "Retrying segment" << index << "(attempt" << m_retryCounts[index] << ")";
            QThread::msleep(m_retryBaseDelay * m_retryCounts[index]);
            // Restart segment
            startSegment(index, start, end);
        } else {
            emit error(QString("Segment %1 failed after %2 retries").arg(index).arg(m_maxRetries));
        }
    });

    connect(seg, &SegmentDownloader::finished, this, [=]() {
        thread->quit();
        thread->deleteLater();
        seg->deleteLater();
    });

    m_segments.append(seg);
    m_threads.append(thread);

    thread->start();

    if (m_isPaused) {
        QMetaObject::invokeMethod(seg, "pause", Qt::QueuedConnection);
    }
}

void DownloadManager::pause() {
    m_isPaused = true;
    for (auto *seg : m_segments)
        seg->pause();
}

void DownloadManager::resume() {
    m_isPaused = false;
    for (auto *seg : m_segments)
        seg->resume();
}

void DownloadManager::cancel() {
    for (auto *seg : m_segments)
        seg->stop();
    deleteMetadata();
}

void DownloadManager::mergeParts() {
    QFile out(m_output);
    if (!out.open(QIODevice::WriteOnly)) {
        emit error("Cannot open final file");
        return;
    }
    for (int i = 0; i < m_segmentCount; i++) {
        QFile part(QString("%1.part%2").arg(m_output).arg(i));
        if (part.open(QIODevice::ReadOnly)) {
            out.write(part.readAll());
            part.close();
            part.remove();
        }
    }
    out.close();
    deleteMetadata();
    emit finished();
}

void DownloadManager::saveMetadata() {
    QJsonObject root;
    root["url"] = m_url;
    root["totalSize"] = QString::number(m_totalSize);
    QJsonArray segs;
    for (int i = 0; i < m_segmentCount; i++) {
        QFile part(QString("%1.part%2").arg(m_output).arg(i));
        QJsonObject seg;
        seg["start"] = QString::number(i * (m_totalSize / m_segmentCount));
        seg["end"] = QString::number((i + 1) * (m_totalSize / m_segmentCount) - 1);
        seg["downloaded"] = QString::number(part.exists() ? part.size() : 0);
        seg["file"] = part.fileName();
        segs.append(seg);
    }
    root["segments"] = segs;

    QFile f(m_metaFile);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson());
        f.close();
    }
}

bool DownloadManager::loadMetadata() {
    QFile f(m_metaFile);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return false;
    auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return false;
    auto obj = doc.object();
    if (obj["url"].toString() != m_url) return false;
    return true;
}

void DownloadManager::deleteMetadata() {
    QFile::remove(m_metaFile);
}
