#include "downloadmanager.h"
#include <QThread>
#include <QDebug>

DownloadManager::DownloadManager(const QString &url, const QString &outputFile, QObject *parent)
    : QObject(parent),
    m_url(url),
    m_outputFile(outputFile),
    m_stopRequested(false),
    m_pauseRequested(false),
    m_curl(nullptr),
    m_lastBytes(0),
    m_retryCount(3),
    m_retryDelay(1000) // 1 sec base delay
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

DownloadManager::~DownloadManager() {
    curl_global_cleanup();
}

void DownloadManager::start() {
    int attempt = 0;
    bool success = false;

    while (attempt < m_retryCount && !m_stopRequested) {
        if (performDownload()) {
            success = true;
            break;
        } else {
            ++attempt;
            if (attempt < m_retryCount) {
                int delay = m_retryDelay * (1 << (attempt - 1)); // exponential backoff
                qWarning() << "Retry" << attempt << "in" << delay << "ms";
                QThread::msleep(delay);
            }
        }
    }

    if (!success && !m_stopRequested) {
        emit error(QString("Download failed after %1 attempts").arg(m_retryCount));
    }

    emit finished();
}

bool DownloadManager::performDownload() {
    m_file.setFileName(m_outputFile);
    if (!m_file.open(QIODevice::WriteOnly)) {
        emit error("Cannot open file for writing: " + m_outputFile);
        return false;
    }

    m_curl = curl_easy_init();
    if (!m_curl) {
        emit error("Failed to initialize CURL");
        return false;
    }

    QByteArray urlData = m_url.toUtf8();
    curl_easy_setopt(m_curl, CURLOPT_URL, urlData.constData());
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, &DownloadManager::writeCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);

    curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFOFUNCTION, &DownloadManager::progressCallback);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFODATA, this);

    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(m_curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 0L); // no timeout
    curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, 10L);

    m_timer.restart();
    m_lastBytes = 0;

    CURLcode res = curl_easy_perform(m_curl);
    curl_easy_cleanup(m_curl);
    m_curl = nullptr;
    m_file.close();

    if (res == CURLE_OK) {
        return true;
    } else {
        emit error(QString("CURL error: %1").arg(curl_easy_strerror(res)));
        return false;
    }
}

void DownloadManager::pause() {
    m_pauseRequested.store(true);
}

void DownloadManager::cancel() {
    m_stopRequested.store(true);
}

size_t DownloadManager::writeCallback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    DownloadManager *self = static_cast<DownloadManager *>(userdata);

    // Pause handling
    while (self->m_pauseRequested.load() && !self->m_stopRequested.load()) {
        QThread::msleep(200);
    }

    if (self->m_stopRequested.load()) {
        return 0; // Abort download
    }

    qint64 written = self->m_file.write(static_cast<char *>(ptr), size * nmemb);
    return written > 0 ? written : 0;
}

int DownloadManager::progressCallback(void *clientp,
                                      curl_off_t dltotal,
                                      curl_off_t dlnow,
                                      curl_off_t,
                                      curl_off_t) {
    DownloadManager *self = static_cast<DownloadManager *>(clientp);

    if (self->m_stopRequested.load()) {
        return 1; // abort
    }

    qint64 elapsedMs = self->m_timer.elapsed();
    if (elapsedMs > 1000) {
        qint64 diff = dlnow - self->m_lastBytes;
        double speedMBps = (diff / 1024.0 / 1024.0) / (elapsedMs / 1000.0);
        int eta = (dltotal > 0 && diff > 0)
                      ? static_cast<int>((dltotal - dlnow) / (diff / (elapsedMs / 1000.0)))
                      : -1;
        emit self->progress(dlnow, dltotal, speedMBps, eta);
        self->m_timer.restart();
        self->m_lastBytes = dlnow;
    }

    return 0;
}
