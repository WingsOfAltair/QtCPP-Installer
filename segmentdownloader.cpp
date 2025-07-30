#include "segmentdownloader.h"
#include <QFile>
#include <QThread>
#include <QDebug>

SegmentDownloader::SegmentDownloader(const QString &url, const QString &filePart, qint64 start, qint64 end)
    : m_url(url), m_filePart(filePart), m_start(start), m_end(end),
    m_curl(nullptr), m_file(nullptr), m_paused(false), m_stopRequested(false),
    m_downloaded(0), m_retryCount(3), m_retryDelay(1000) // 3 retries, 1s base delay
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

SegmentDownloader::~SegmentDownloader() {
    curl_global_cleanup();
}

void SegmentDownloader::start() {
    m_file = fopen(m_filePart.toStdString().c_str(), "ab+");
    if (!m_file) {
        emit error("Cannot open file: " + m_filePart);
        emit finished();
        return;
    }

    fseek(m_file, 0, SEEK_END);
    qint64 existing = ftell(m_file);
    qint64 resumeFrom = m_start + existing;

    if (resumeFrom >= m_end) {
        fclose(m_file);
        emit finished();
        return;
    }

    bool success = false;

    for (int attempt = 0; attempt < m_retryCount && !m_stopRequested; ++attempt) {
        m_curl = curl_easy_init();
        if (!m_curl) {
            emit error("Failed to init CURL");
            break;
        }

        curl_easy_setopt(m_curl, CURLOPT_URL, m_url.toStdString().c_str());
        curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, m_file);
        curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(m_curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(m_curl, CURLOPT_XFERINFODATA, this);

        QString range = QString("%1-%2").arg(resumeFrom).arg(m_end);
        curl_easy_setopt(m_curl, CURLOPT_RANGE, range.toStdString().c_str());

        CURLcode res = curl_easy_perform(m_curl);

        if (res == CURLE_OK) {
            success = true;
            curl_easy_cleanup(m_curl);
            break;
        } else {
            qWarning() << "Segment" << m_filePart << "retry" << (attempt + 1)
            << "failed:" << curl_easy_strerror(res);
            curl_easy_cleanup(m_curl);
            if (attempt < m_retryCount - 1) {
                int delay = m_retryDelay * (1 << attempt); // exponential backoff
                QThread::msleep(delay);
            }
        }
    }

    fclose(m_file);

    if (!success && !m_stopRequested) {
        emit error(QString("Segment %1 failed after %2 retries").arg(m_filePart).arg(m_retryCount));
    }

    emit finished();
}

void SegmentDownloader::pause() {
    if (m_curl) curl_easy_pause(m_curl, CURLPAUSE_ALL);
    m_paused = true;
}

void SegmentDownloader::resume() {
    if (m_curl) curl_easy_pause(m_curl, CURLPAUSE_CONT);
    m_paused = false;
}

void SegmentDownloader::stop() {
    m_stopRequested = true;
    if (m_curl) curl_easy_pause(m_curl, CURLPAUSE_ALL);
}

int SegmentDownloader::progressCallback(void *clientp, curl_off_t, curl_off_t dlnow, curl_off_t, curl_off_t) {
    auto *self = static_cast<SegmentDownloader *>(clientp);
    if (self->m_stopRequested) return 1; // abort
    emit self->progress(dlnow);
    return 0;
}
