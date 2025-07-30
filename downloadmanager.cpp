#include "downloadmanager.h"
#include <QFileInfo>
#include <QThread>
#include <QDebug>

DownloadManager::DownloadManager(const QString &url, const QString &outputFile, QObject *parent)
    : QObject(parent), m_url(url), m_outputFile(outputFile), m_file(nullptr), m_curl(nullptr) {
    m_paused.store(0);
    m_stopRequested.store(0);
    curl_global_init(CURL_GLOBAL_ALL);
}

DownloadManager::~DownloadManager() {
    if (m_curl) {
        curl_easy_cleanup(m_curl);
    }
    curl_global_cleanup();
}

qint64 DownloadManager::getRemoteFileSize() {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    curl_easy_setopt(curl, CURLOPT_URL, m_url.toStdString().c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    CURLcode res = curl_easy_perform(curl);
    double fileSize = -1;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &fileSize);
    }
    curl_easy_cleanup(curl);
    return static_cast<qint64>(fileSize);
}

void DownloadManager::start() {
    qint64 remoteSize = getRemoteFileSize();
    QFileInfo fi(m_outputFile);
    qint64 localSize = fi.exists() ? fi.size() : 0;

    // Already complete
    if (remoteSize > 0 && localSize >= remoteSize) {
        emit finished();
        return;
    }

    m_file = fopen(m_outputFile.toStdString().c_str(), "ab+");
    if (!m_file) {
        emit error("Cannot open file: " + m_outputFile);
        emit finished();
        return;
    }

    m_curl = curl_easy_init();
    if (!m_curl) {
        emit error("Failed to initialize curl");
        fclose(m_file);
        emit finished();
        return;
    }

    curl_easy_setopt(m_curl, CURLOPT_URL, m_url.toStdString().c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, &DownloadManager::writeCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, m_file);
    curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFOFUNCTION, &DownloadManager::progressCallback);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(m_curl, CURLOPT_FAILONERROR, 1L);

    // Resume support
    if (localSize > 0 && remoteSize > localSize) {
        curl_easy_setopt(m_curl, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)localSize);
    }

    CURLcode res = curl_easy_perform(m_curl);

    fclose(m_file);
    if (res == CURLE_OK && !m_stopRequested.load()) {
        emit finished();
    } else if (!m_stopRequested.load()) {
        emit error(QString("Download failed: %1").arg(curl_easy_strerror(res)));
    }
}

void DownloadManager::pause() {
    m_paused.store(1);
}

void DownloadManager::resume() {
    m_paused.store(0);
}

void DownloadManager::cancel() {
    m_stopRequested.store(1);
}

size_t DownloadManager::writeCallback(void *ptr, size_t size, size_t nmemb, void *stream) {
    return fwrite(ptr, size, nmemb, (FILE *)stream);
}

int DownloadManager::progressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                                      curl_off_t, curl_off_t) {
    DownloadManager *self = static_cast<DownloadManager *>(clientp);
    if (self->m_stopRequested.load()) return 1; // Abort

    // Pause handling
    while (self->m_paused.load()) {
        QThread::msleep(200);
    }

    static QElapsedTimer timer;
    static qint64 lastBytes = 0;

    if (!timer.isValid())
        timer.start();

    qint64 elapsedMs = timer.elapsed();
    if (elapsedMs > 1000) {
        qint64 diff = dlnow - lastBytes;
        double speedMBps = diff / (1024.0 * 1024.0) / (elapsedMs / 1000.0);
        int eta = (dltotal > 0 && diff > 0)
                      ? static_cast<int>((dltotal - dlnow) / (diff / (elapsedMs / 1000.0)))
                      : -1;
        emit self->progress(dlnow, dltotal, speedMBps, eta);
        timer.restart();
        lastBytes = dlnow;
    }
    return 0;
}
