#ifndef QMLAVPLAYER_H
#define QMLAVPLAYER_H

#include <QQmlParserStatus>
#include <QMediaPlayer>
#include <QAbstractVideoSurface>
#include <QVideoSurfaceFormat>
#include <QAudioOutput>
#include <QTimer>
#include <QElapsedTimer>

#include "qmlavframe.h"
#include "qmlavdemuxer.h"
#include "qmlavaudioiodevice.h"
#include "qmlavpropertyhelpers.h"

class QmlAVPlayer : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    Q_PROPERTY(QAbstractVideoSurface *videoSurface READ videoSurface WRITE setVideoSurface)

    QMLAV_PROPERTY_DECL(QVariantMap, avOptions, setAVOptions, avOptionsChanged);
    QMLAV_PROPERTY_DECL(bool, autoLoad, setAutoLoad, autoLoadChanged) = true;
    QMLAV_PROPERTY_DECL(bool, autoPlay, setAutoPlay, autoPlayChanged) = false;
    QMLAV_PROPERTY(int, loops, setLoops, loopsChanged) = 1; // NOTE: Implemented partially (Once playing and infinite loop behavior)
    QMLAV_PROPERTY_DECL(QUrl, source, setSource, sourceChanged);
    QMLAV_PROPERTY_READONLY(QMediaPlayer::State, playbackState, playbackStateChanged) = QMediaPlayer::StoppedState;
    QMLAV_PROPERTY_READONLY(QMediaPlayer::MediaStatus, status, statusChanged) = QMediaPlayer::NoMedia;
    QMLAV_PROPERTY_READONLY(QVariant, bufferProgress, bufferProgressChanged) = 1.0;
    QMLAV_PROPERTY(bool, muted, setMuted, mutedChanged) = false;
    QMLAV_PROPERTY_DECL(double, volume, setVolume, volumeChanged) = 0.0;
    QMLAV_PROPERTY_READONLY(bool, hasVideo, hasVideoChanged) = false;
    QMLAV_PROPERTY_READONLY(bool, hasAudio, hasAudioChanged) = false;

    // Stream Health & Diagnostics properties
    QMLAV_PROPERTY_READONLY(double, fps, fpsChanged) = 0.0;
    QMLAV_PROPERTY_READONLY(qint64, bitrate, bitrateChanged) = 0;
    QMLAV_PROPERTY_READONLY(QString, videoCodec, videoCodecChanged) = QString();
    QMLAV_PROPERTY_READONLY(QString, videoResolution, videoResolutionChanged) = QString();
    QMLAV_PROPERTY_READONLY(bool, isHWAccelerated, isHWAcceleratedChanged) = false;
    QMLAV_PROPERTY_READONLY(int, framesDecoded, framesDecodedChanged) = 0;
    QMLAV_PROPERTY_READONLY(int, framesDiscarded, framesDiscardedChanged) = 0;

    // Auto-Reconnect watchdog properties
    QMLAV_PROPERTY(bool, autoReconnect, setAutoReconnect, autoReconnectChanged) = true;
    QMLAV_PROPERTY_READONLY(bool, reconnecting, reconnectingChanged) = false;
    QMLAV_PROPERTY_READONLY(int, reconnectAttempt, reconnectAttemptChanged) = 0;
    QMLAV_PROPERTY(int, reconnectInterval, setReconnectInterval, reconnectIntervalChanged) = 2000;
    QMLAV_PROPERTY(int, maxReconnectInterval, setMaxReconnectInterval, maxReconnectIntervalChanged) = 15000;

public:
    QmlAVPlayer(QObject *parent = nullptr);
    ~QmlAVPlayer() override;

    QAbstractVideoSurface *videoSurface() const { return m_videoSurface; }
    virtual void classBegin() override {}
    virtual void componentComplete() override;

signals:
    void videoFramePresented();

public slots:
    void play();
    void stop();
    void retry();
    void setVideoSurface(QAbstractVideoSurface *surface);
    void frameHandler(const std::shared_ptr<QmlAVFrame> frame);

protected:
    bool load();
    void stateMachine();
    void reset();
    void scheduleReconnect();
    void cancelReconnect();

    void setPlaybackState(const QMediaPlayer::State state);
    void setStatus(const QMediaPlayer::MediaStatus status);
    void setHasVideo(bool hasVideo);
    void setHasAudio(bool hasAudio);

    void setFps(double fps);
    void setBitrate(qint64 bitrate);
    void setVideoCodec(const QString &codec);
    void setVideoResolution(const QString &res);
    void setIsHWAccelerated(bool hw);
    void setFramesDecoded(int count);
    void setFramesDiscarded(int count);
    void setReconnecting(bool reconnecting);
    void setReconnectAttempt(int attempt);

private slots:
    void updateMetrics();
    void onReconnectTimer();

private:
    bool m_complete;
    QmlAVDemuxer *m_demuxer;
    QAbstractVideoSurface *m_videoSurface;
    QTimer m_playTimer;
    QTimer m_reconnectTimer;
    QTimer m_metricsTimer;

    QElapsedTimer m_fpsTimer;
    int m_presentedFramesCount;
    int m_currentBackoffMs;

    QmlAVAudioIODevice m_audioIODevice;
    QAudioOutput *m_audioOutput;
};

#endif // QMLAVPLAYER_H
