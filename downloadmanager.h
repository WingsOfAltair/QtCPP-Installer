#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QFile>
#include <atomic>
#include <QElapsedTimer>
#include <QString>
#include <curl/curl.h>

class DownloadManager : public QObject {
    Q_OBJECT

public:
    explicit DownloadManager(const QString &url, const QString &outputFile, QObject *parent = nullptr);
    ~DownloadManager();

public slots:
    void start();       // Start download
    void pause();       // Pause download
    void cancel();      // Cancel download

signals:
    void progress(qint64 downloaded, qint64 total, double speedMBps, int etaSeconds);
    void finished();
    void error(const QString &message);

private:
    static size_t writeCallback(void *ptr, size_t size, size_t nmemb, void *userdata);
    static int progressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t);

    QString m_url;
    QString m_outputFile;
    std::atomic<bool> m_stopRequested;
    std::atomic<bool> m_pauseRequested;

    CURL *m_curl;
    QFile m_file;
    QElapsedTimer m_timer;
    qint64 m_lastBytes;

    int m_retryCount;
    int m_retryDelay;

    bool performDownload();
};

#endif // DOWNLOADMANAGER_H
