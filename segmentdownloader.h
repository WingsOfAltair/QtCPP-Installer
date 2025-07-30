#pragma once
#include <QObject>
#include <QString>
#include <curl/curl.h>

class SegmentDownloader : public QObject {
    Q_OBJECT
public:
    SegmentDownloader(const QString &url, const QString &filePart, qint64 start, qint64 end);
    ~SegmentDownloader();

public slots:
    void start();
    void pause();
    void resume();
    void stop();

signals:
    void progress(qint64 downloaded);
    void finished();
    void error(const QString &msg);

private:
    QString m_url, m_filePart;
    qint64 m_start, m_end;
    CURL *m_curl;
    FILE *m_file;
    bool m_paused, m_stopRequested;
    qint64 m_downloaded;

    int m_retryCount;
    int m_retryDelay;

    static int progressCallback(void *clientp, curl_off_t, curl_off_t dlnow, curl_off_t, curl_off_t);
};
