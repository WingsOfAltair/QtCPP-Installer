#include "segmentdownloader.h"
#include <QNetworkRequest>

SegmentDownloader::SegmentDownloader(const QString &url, const QString &outputFile, qint64 start, qint64 end, QObject *parent)
    : QObject(parent),
    m_url(url),
    m_outputFile(outputFile),
    m_startPos(start),
    m_endPos(end),
    m_manager(new QNetworkAccessManager(this))
{
    m_file = new QFile(m_outputFile);
    if (!m_file->open(QIODevice::ReadWrite | QIODevice::Unbuffered)) {
        emit error(QString("Failed to open file: %1").arg(m_outputFile));
        return;
    }
    if (!m_file->seek(m_startPos)) {
        emit error(QString("Failed to seek in file: %1").arg(m_outputFile));
        return;
    }
}

SegmentDownloader::~SegmentDownloader()
{
    if (m_reply) {
        m_reply->deleteLater();
    }
    if (m_file) {
        m_file->close();
        delete m_file;
    }
}

void SegmentDownloader::start()
{
    if (m_complete || m_stopped) return;

    QNetworkRequest request{QUrl(m_url)};
    QByteArray rangeHeader = "bytes=" + QByteArray::number(m_startPos + m_bytesReceived) + "-" + QByteArray::number(m_endPos);
    request.setRawHeader("Range", rangeHeader);

    m_reply = m_manager->get(request);

    connect(m_reply, &QNetworkReply::readyRead, this, &SegmentDownloader::onReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        Q_UNUSED(total);
        m_bytesReceived = received;
        emit progress(m_bytesReceived, m_endPos - m_startPos + 1);
    });
    connect(m_reply, &QNetworkReply::finished, this, &SegmentDownloader::onFinished);
    connect(m_reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &SegmentDownloader::onErrorOccurred);
}

void SegmentDownloader::pause()
{
    if (m_reply && !m_paused) {
        m_paused = true;
        disconnect(m_reply, &QNetworkReply::readyRead, this, &SegmentDownloader::onReadyRead);
        disconnect(m_reply, &QNetworkReply::finished, this, &SegmentDownloader::onFinished);
        disconnect(m_reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
                   this, &SegmentDownloader::onErrorOccurred);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

void SegmentDownloader::resume()
{
    if (m_paused) {
        m_paused = false;
        start();
    }
}

void SegmentDownloader::stop()
{
    m_stopped = true;
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_file) {
        m_file->close();
    }
}

void SegmentDownloader::onReadyRead() {
    if (!m_file) return;

    QByteArray data = m_reply->readAll();
    qint64 written = m_file->write(data);
    if (written != data.size()) {
        emit error(QString("Failed to write to file segment: %1").arg(m_outputFile));
        m_reply->abort();
        return;
    }

    m_bytesReceived += written;

    qDebug() << "Segment" << index << "received" << m_bytesReceived << "bytes";

    emit progress(m_bytesReceived, m_endPos - m_startPos + 1);
}

void SegmentDownloader::onFinished()
{
    if (!m_reply) return;

    if (m_reply->error() == QNetworkReply::NoError) {
        m_complete = true;
        m_file->flush();
        m_file->close();
        emit finished();
    } else {
        emit error(QString("Download failed: %1").arg(m_reply->errorString()));
    }
    m_reply->deleteLater();
    m_reply = nullptr;
}

void SegmentDownloader::onErrorOccurred(QNetworkReply::NetworkError code) {
    Q_UNUSED(code);
    qDebug() << "Segment" << index << "network error:" << m_reply->errorString();
    emit error(QString("Network error: %1").arg(m_reply->errorString()));
}
