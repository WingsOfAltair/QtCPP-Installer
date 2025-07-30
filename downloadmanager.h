#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QString>
#include <atomic>
#include <QElapsedTimer>
#include <curl/curl.h>

class DownloadManager : public QObject {
    Q_OBJECT
public:
    explicit DownloadManager(const QString &url, const QString &outputFile, QObject *parent = nullptr);
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
    static size_t writeCallback(void *ptr, size_t size, size_t nmemb, void *stream);
    static int progressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                                curl_off_t, curl_off_t);

    qint64 getRemoteFileSize();

    QString m_url;
    QString m_outputFile;

    FILE *m_file;
    CURL *m_curl;

    std::atomic<bool> m_paused;
    std::atomic<bool> m_stopRequested;
};

#endif // DOWNLOADMANAGER_H
