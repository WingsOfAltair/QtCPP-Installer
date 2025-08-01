#include "downloadmanager.h"
#include <QFileInfo>
#include <QThread>
#include <QDebug>

DownloadManager::DownloadManager(const QString &url, const QString &filePath, DownloadControlFlags* controlFlags, QObject *parent)
    : QObject(parent),
    m_url(url),
    m_filePath(filePath),
    m_metaPath(filePath + ".meta"),
    m_curl(nullptr),
    m_file(nullptr),
    m_controlFlags(controlFlags) {
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

#ifdef _WIN32
#define fseeko _fseeki64
#endif

#ifdef _WIN32
#include <io.h>    // for _chsize_s
#else
#include <unistd.h> // for ftruncate
#endif

void DownloadManager::start() {
    // 1. Get remote file size for accurate ETA and progress
    m_expectedTotal = getRemoteFileSize();
    if (m_expectedTotal <= 0) {
        emit error("Failed to get remote file size");
        emit finished();
        return;
    }

    // 2. Determine resume point: min(meta info, actual file size)
    qint64 metaResume = 0;
    loadMetaFile(metaResume);
    QFileInfo fi(m_filePath);
    qint64 actualSize = fi.exists() ? fi.size() : 0;
    qint64 resumePos = qMin(metaResume, actualSize);
    m_resumeBase = resumePos; // For correct progress calculation

    qDebug() << "Resuming from" << resumePos << "of" << m_expectedTotal;

    // 3. Open file for resume or fresh download
    if (resumePos > 0) {
        m_file = fopen(m_filePath.toStdString().c_str(), "r+b");
        if (!m_file) {
            emit error("Cannot open file for resuming");
            emit finished();
            return;
        }

        // Seek to resume point
#ifdef _WIN32
        if (_fseeki64(m_file, resumePos, SEEK_SET) != 0)
#else
        if (fseeko(m_file, resumePos, SEEK_SET) != 0)
#endif
        {
            emit error("Failed to seek in file");
            fclose(m_file);
            emit finished();
            return;
        }

        // Truncate any extra bytes after resume position
#ifdef _WIN32
        _chsize_s(_fileno(m_file), resumePos);
#else
        ftruncate(fileno(m_file), resumePos);
#endif

    } else {
        // Fresh download: overwrite file
        m_file = fopen(m_filePath.toStdString().c_str(), "wb");
        if (!m_file) {
            emit error("Cannot open file for writing");
            emit finished();
            return;
        }
    }

    // 4. Initialize CURL
    m_curl = curl_easy_init();
    if (!m_curl) {
        emit error("Failed to initialize curl");
        fclose(m_file);
        emit finished();
        return;
    }

    curl_easy_setopt(m_curl, CURLOPT_URL, m_url.toStdString().c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, m_file);
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);

    if (resumePos > 0) {
        curl_easy_setopt(m_curl, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)resumePos);
    }

    // 5. Perform download
    CURLcode res = curl_easy_perform(m_curl);

    // 6. Cleanup
    curl_easy_cleanup(m_curl);
    fclose(m_file);

    // 7. Finalize
    if (res == CURLE_OK && !m_stopRequested.load()) {
        deleteMetaFile(); // Remove resume data if success
        emit finished();
    } else if (m_stopRequested.load()) {
        QThread::msleep(100); // Cancel requested
    } else {
        emit error(QString("Download failed: %1").arg(curl_easy_strerror(res)));
        emit finished();
    }
}

void DownloadManager::pause() {
    if (m_controlFlags)
        m_controlFlags->paused.store(true, std::memory_order_relaxed);
}

void DownloadManager::resume() {
    if (m_controlFlags)
        m_controlFlags->paused.store(false, std::memory_order_relaxed);
}

void DownloadManager::cancel() {
    if (m_controlFlags)
        m_controlFlags->stopped.store(true, std::memory_order_relaxed);
}

size_t DownloadManager::writeCallback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    FILE *stream = (FILE *)userdata;
    return fwrite(ptr, size, nmemb, stream);
}

int DownloadManager::progressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                                      curl_off_t, curl_off_t) {
    DownloadManager *self = static_cast<DownloadManager *>(clientp);

    // Correct absolute progress when resuming
    qint64 totalDownloaded = self->m_resumeBase + dlnow;
    self->saveMetaFile(totalDownloaded);

    // Stop if cancel requested
    if (self->m_controlFlags && self->m_controlFlags->stopped.load(std::memory_order_relaxed)) {
        return 1; // abort download
    }

    // Pause logic: block thread without eating CPU
    while (self->m_controlFlags && self->m_controlFlags->paused.load(std::memory_order_relaxed)) {
        QThread::msleep(200);
    }

    // Progress reporting every second
    static QElapsedTimer timer;
    static curl_off_t lastBytes = 0;

    if (!timer.isValid())
        timer.start();

    qint64 elapsedMs = timer.elapsed();
    if (elapsedMs > 1000) {
        curl_off_t diff = dlnow - lastBytes;
        double speedMBps = diff / (1024.0 * 1024.0) / (elapsedMs / 1000.0);

        // ETA based on total file size
        curl_off_t remaining = (dltotal > 0) ? (dltotal - dlnow) : 0;
        int eta = (diff > 0) ? static_cast<int>(remaining / (diff / (elapsedMs / 1000.0))) : -1;

        emit self->progress(totalDownloaded, self->m_expectedTotal, speedMBps, eta);

        timer.restart();
        lastBytes = dlnow;
    }
    return 0; // continue download
}

bool DownloadManager::saveMetaFile(qint64 downloaded) {
    QFile file(m_metaPath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QTextStream out(&file);
    out << m_url << "\n" << downloaded;
    return true;
}

bool DownloadManager::loadMetaFile(qint64 &downloaded) {
    QFile file(m_metaPath);
    if (!file.exists()) return false;
    if (!file.open(QIODevice::ReadOnly)) return false;
    QTextStream in(&file);
    QString urlLine;
    in >> urlLine >> downloaded;
    return !urlLine.isEmpty();
}

void DownloadManager::deleteMetaFile() {
    QFile::remove(m_metaPath);
}
