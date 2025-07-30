#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QList>
#include <QElapsedTimer>
#include "segmentdownloader.h"

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
signals:
    void progress(qint64 downloaded, qint64 total, double speedMBps, int eta);
    void finished();
    void error(const QString &message);

private slots:
    void segmentProgress(qint64 bytesReceived, qint64 bytesTotal);
    void segmentFinished();
    void segmentError(const QString &msg);

private:
    QElapsedTimer m_timer;
    qint64 m_lastBytesReceived = 0;
    QString m_url;
    QString m_outputFile;
    qint64 m_totalSize = 0;

    QList<SegmentDownloader*> m_segments;
    qint64 m_downloaded = 0;

    bool m_paused = false;
    bool m_cancelled = false;

    void mergeSegments();

    void initializeSegments(int segmentCount = 4);  // split into 4 parts by default
    qint64 getRemoteFileSize();
};

#endif // DOWNLOADMANAGER_H
