#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QString>
#include <atomic>
#include <QElapsedTimer>
#include <curl/curl.h>

struct DownloadControlFlags {
    std::atomic<bool> paused{false};
    std::atomic<bool> stopped{false};
};

class DownloadManager : public QObject {
    Q_OBJECT
public:
    explicit DownloadManager(const QString &url, const QString &filePath, DownloadControlFlags* controlFlags, QObject *parent = nullptr);
    ~DownloadManager();

    void start();
    void pause();
    void resume();
    void cancel();

signals:
    void progress(qint64 downloaded, qint64 total, double speedMBps, int eta);
    void finished();
    void error(const QString &msg);

private:
    static size_t writeCallback(void *ptr, size_t size, size_t nmemb, void *userdata);
    static int progressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                                curl_off_t ultotal, curl_off_t ulnow);

    bool saveMetaFile(qint64 downloaded);
    bool loadMetaFile(qint64 &downloaded);
    void deleteMetaFile();
    qint64 getRemoteFileSize();

    QString m_url;
    QString m_filePath;
    QString m_metaPath;

    FILE *m_file;
    CURL *m_curl;

    DownloadControlFlags* m_controlFlags;

    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_isPausedState {false}; // in your class, initially false
};

#endif // DOWNLOADMANAGER_H
