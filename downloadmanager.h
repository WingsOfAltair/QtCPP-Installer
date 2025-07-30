#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QThread>
#include <QMap>
#include <QJsonObject>
#include "segmentdownloader.h"

class DownloadManager : public QObject {
    Q_OBJECT
public:
    DownloadManager(const QString &url, const QString &output);
    void setSegmentCount(int count) { m_segmentCount = count; }
    void setMaxRetries(int maxRetries) { m_maxRetries = maxRetries; }
    void setRetryBaseDelay(int ms) { m_retryBaseDelay = ms; }

public slots:
    void start();
    void pause();
    void resume();
    void cancel();

signals:
    void progress(qint64 downloaded, qint64 total, double speedMBps, int eta);
    void error(const QString &msg);
    void finished();
    void retryStatus(int segmentIndex, int attempt);

private:
    QString m_url, m_output, m_metaFile;
    int m_segmentCount;
    qint64 m_totalSize;
    bool m_supportsRange;
    QVector<SegmentDownloader*> m_segments;
    QVector<QThread*> m_threads;
    qint64 m_downloaded;
    std::chrono::steady_clock::time_point m_start;

    QMap<int, int> m_retryCounts; // segment index → retry attempts
    int m_maxRetries;
    int m_retryBaseDelay;
    bool m_isPaused;

    bool checkServerSupport();
    void mergeParts();
    void saveMetadata();
    bool loadMetadata();
    void deleteMetadata();

    void startSegment(int index, qint64 start, qint64 end);
};
