#ifndef SEGMENTDOWNLOADER_H
#define SEGMENTDOWNLOADER_H

#include <QObject>
#include <QFile>
#include <QNetworkReply>
#include <QNetworkAccessManager>

class SegmentDownloader : public QObject {
    Q_OBJECT
public:
    SegmentDownloader(const QString &url, const QString &outputFile, qint64 start, qint64 end, QObject *parent = nullptr);
    ~SegmentDownloader();

    void start();
    void pause();
    void resume();
    void stop();

    bool isComplete() const { return m_complete; }

    int index = -1; // optional segment index

    qint64 bytesReceived() const { return m_bytesReceived; }
    qint64 size() const { return m_endPos - m_startPos + 1; }

    qint64 startPos() const { return m_startPos; }
    qint64 endPos() const { return m_endPos; }

    QString outputFile() const { return m_outputFile; }

signals:
    void progress(qint64 bytesReceived, qint64 bytesTotal);
    void finished();
    void error(const QString &msg);

private slots:
    void onReadyRead();
    void onFinished();
    void onErrorOccurred(QNetworkReply::NetworkError code);

private:
    QString m_url;
    QString m_outputFile;
    qint64 m_startPos;
    qint64 m_endPos;

    QFile *m_file = nullptr;
    QNetworkReply *m_reply = nullptr;
    QNetworkAccessManager *m_manager = nullptr;

    bool m_paused = false;
    bool m_stopped = false;
    bool m_complete = false;

    qint64 m_bytesReceived = 0;
};

#endif // SEGMENTDOWNLOADER_H
