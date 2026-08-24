#include "qmlavplayer.h"

QmlAVPlayer::QmlAVPlayer(QObject *parent)
    : QObject(parent)
    , m_complete(false)
    , m_demuxer(nullptr)
    , m_videoSurface(nullptr)
    , m_presentedFramesCount(0)
    , m_currentBackoffMs(2000)
    , m_audioOutput(nullptr)
{
    qRegisterMetaType<QList<QVideoFrame::PixelFormat>>();

    m_playTimer.setSingleShot(true);
    connect(&m_playTimer, &QTimer::timeout, this, &QmlAVPlayer::play);

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &QmlAVPlayer::onReconnectTimer);

    m_metricsTimer.setInterval(1000);
    connect(&m_metricsTimer, &QTimer::timeout, this, &QmlAVPlayer::updateMetrics);

    m_fpsTimer.start();
}

QmlAVPlayer::~QmlAVPlayer()
{
    m_reconnectTimer.stop();
    m_metricsTimer.stop();
    stop();
}

void QmlAVPlayer::componentComplete()
{
    if (m_autoPlay) {
        play();
    } else if (m_autoLoad) {
        load();
    }

    m_complete = true;
}

void QmlAVPlayer::play()
{
    logDebug() << "play()";

    cancelReconnect();

    if (load()) {
        m_demuxer->start();
        m_metricsTimer.start();
    }
}

void QmlAVPlayer::retry()
{
    logDebug() << "retry()";
    stop();
    play();
}

void QmlAVPlayer::stop()
{
    logDebug() << "stop()";

    m_metricsTimer.stop();

    if (m_demuxer) {
        disconnect(m_demuxer, nullptr, this, nullptr);
        delete m_demuxer;
        m_demuxer = nullptr;
    }

    if (m_videoSurface && m_videoSurface->isActive()) {
        m_videoSurface->stop();
    }

    if (m_audioOutput) {
        m_audioOutput->stop();
        delete m_audioOutput;
        m_audioOutput = nullptr;
    }

    m_audioIODevice.clear();

    setPlaybackState(QMediaPlayer::StoppedState);
    setHasVideo(false);
    setHasAudio(false);
    setFps(0.0);
}

void QmlAVPlayer::setVideoSurface(QAbstractVideoSurface *surface)
{
    if (m_videoSurface != surface) {
        stop();
    }

    m_videoSurface = surface;
}

void QmlAVPlayer::frameHandler(const std::shared_ptr<QmlAVFrame> frame)
{
    if (m_playbackState == QMediaPlayer::PlayingState) {
        if (frame->type() == QmlAVFrame::TypeVideo) {
            auto vf = std::static_pointer_cast<QmlAVVideoFrame>(frame);
            QVideoFrame qvf = *vf;

            if (m_videoSurface) {
                if (!m_videoSurface->isActive()) {
                    QVideoSurfaceFormat f(qvf.size(), qvf.pixelFormat(), qvf.handleType());

                    AVRational sar = {1, 1};
                    auto dar = QmlAVOptions(m_avOptions).aspectRatio();
                    if (!dar.has_value()) {
                        sar = vf->sampleAspectRatio();
                    } else {
                        // Just divide the DAR by the Frame size and reduce the fraction
                        av_reduce(&sar.num, &sar.den,
                                  dar->num * vf->size().height(),
                                  dar->den * vf->size().width(),
                                  1024 * 1024);
                        logDebug() << "Force Aspect Ratio "
                                  << vf->size().width() << "x" << vf->size().height()
                                  << " [SAR " << sar.num << ":" << sar.den << " DAR " << dar->num << ":" << dar->den << "]";
                    }
                    f.setPixelAspectRatio(sar.num, sar.den);

                    f.setYCbCrColorSpace(vf->colorSpace());
                    logDebug() << "Starting with: "
                               << "QVideoSurfaceFormat(" << f.pixelFormat() << ", " << f.frameSize()
                               << ", viewport=" << f.viewport() << ", pixelAspectRatio=" << f.pixelAspectRatio()
                               << ", handleType=" << f.handleType() <<  ", yCbCrColorSpace=" << f.yCbCrColorSpace()
                               << ')';
                    if (!m_videoSurface->start(f)) {
                        logCritical() << "Error starting the video surface presenting frames.";
                        return;
                    }

                    setHasVideo(true);
                }

                if (m_videoSurface->isActive()) {
                    if (!m_videoSurface->present(qvf)) {
                        stop();
                    } else {
                        m_presentedFramesCount++;
                        if (m_reconnecting) {
                            cancelReconnect();
                        }
                        emit videoFramePresented();
                    }
                }
            }
        } else if (frame->type() == QmlAVFrame::TypeAudio) {
            auto af = std::static_pointer_cast<QmlAVAudioFrame>(frame);

            m_audioIODevice.enqueue(af);

            if (!m_audioOutput) {
                if (af->audioFormat().isValid()) {
                    auto f = af->audioFormat();
                    logDebug() << "Starting with: " << f;
                    auto outputDevice = QAudioDeviceInfo::defaultOutputDevice();
                    m_audioOutput = new QAudioOutput(outputDevice, f);
                    m_audioOutput->setVolume(QAudio::convertVolume(m_volume,
                                                                   QAudio::LogarithmicVolumeScale,
                                                                   QAudio::LinearVolumeScale));
                    m_audioOutput->start(&m_audioIODevice);
                    setHasAudio(true);
                }
            }
        }
    }
}

void QmlAVPlayer::setAVOptions(QVariantMap avOptions)
{
    if (m_avOptions == avOptions) {
        return;
    }

    m_avOptions = avOptions;

    reset();

    emit avOptionsChanged(avOptions);
}

void QmlAVPlayer::setAutoLoad(QmlAVPropertyType<bool> autoLoad)
{
    if (m_autoLoad == autoLoad) {
        return;
    }

    m_autoLoad = autoLoad;

    if (m_complete && autoLoad) {
        load();
    }

    emit autoLoadChanged(autoLoad);
}

void QmlAVPlayer::setAutoPlay(QmlAVPropertyType<bool> autoPlay)
{
    if (m_autoPlay == autoPlay) {
        return;
    }

    m_autoPlay = autoPlay;

    if (m_complete && autoPlay) {
        play();
    }

    emit autoPlayChanged(autoPlay);
}

void QmlAVPlayer::setSource(QmlAVPropertyType<QUrl> source)
{
    if (m_source == source) {
        return;
    }

    logDebug() << QString("setSource(source=%1)").arg(source.toDisplayString());

    m_source = source;

    cancelReconnect();
    reset();

    emit sourceChanged(source);
}

void QmlAVPlayer::setVolume(QmlAVPropertyType<double> volume)
{
    if (qFuzzyCompare(m_volume, volume)) {
        return;
    }

    m_volume = volume;

    if (m_audioOutput) {
        m_audioOutput->setVolume(QAudio::convertVolume(volume,
                                                       QAudio::LogarithmicVolumeScale,
                                                       QAudio::LinearVolumeScale));
    }

    emit volumeChanged(volume);
}

bool QmlAVPlayer::load()
{
    if (!m_demuxer && m_source.isValid()) {
        m_demuxer = new QmlAVDemuxer();

        connect(m_demuxer, &QmlAVDemuxer::frameFinished, this, &QmlAVPlayer::frameHandler);
        connect(m_demuxer, &QmlAVDemuxer::playbackStateChanged, this, &QmlAVPlayer::setPlaybackState);
        connect(m_demuxer, &QmlAVDemuxer::mediaStatusChanged, this, &QmlAVPlayer::setStatus);

        m_demuxer->load(m_source, m_avOptions);

        return true;
    }

    return false;
}

void QmlAVPlayer::scheduleReconnect()
{
    if (!m_autoReconnect || !m_source.isValid() || m_source.isEmpty()) {
        return;
    }

    setReconnecting(true);
    setReconnectAttempt(m_reconnectAttempt + 1);

    if (m_reconnectAttempt == 1) {
        m_currentBackoffMs = m_reconnectInterval;
    } else {
        m_currentBackoffMs = std::min(m_maxReconnectInterval, static_cast<int>(m_currentBackoffMs * 1.5));
    }

    logInfo() << QString("Scheduling reconnect attempt %1 in %2 ms for %3")
                 .arg(m_reconnectAttempt)
                 .arg(m_currentBackoffMs)
                 .arg(m_source.toString());

    m_reconnectTimer.start(m_currentBackoffMs);
}

void QmlAVPlayer::cancelReconnect()
{
    m_reconnectTimer.stop();
    setReconnecting(false);
    setReconnectAttempt(0);
    m_currentBackoffMs = m_reconnectInterval;
}

void QmlAVPlayer::onReconnectTimer()
{
    if (m_reconnecting && m_source.isValid()) {
        logInfo() << QString("Executing reconnect attempt %1 for %2")
                     .arg(m_reconnectAttempt)
                     .arg(m_source.toString());
        stop();
        if (load()) {
            m_demuxer->start();
            m_metricsTimer.start();
        }
    }
}

void QmlAVPlayer::stateMachine()
{
    logDebug() << QString("stateMachine[m_status=%1; m_playbackState=%2]()").arg(m_status).arg(m_playbackState);

    if (m_playbackState == QMediaPlayer::PausedState) {
        logInfo() << QString("%1:%2 Not implemented!").arg(__FILE__).arg(__LINE__);
    } else if (m_playbackState == QMediaPlayer::StoppedState) {
        switch (m_status) {
        case QMediaPlayer::NoMedia:
        case QMediaPlayer::EndOfMedia:
        case QMediaPlayer::InvalidMedia: {
            // Internal demuxer interrupt or error
            if (m_demuxer) {
                stop();

                if (m_loops == -1 /*MediaPlayer.Infinite*/ || m_autoReconnect) {
                    scheduleReconnect();
                }
            }
            break;
        }
        default:
            break;
        }
    }
}

void QmlAVPlayer::reset()
{
    if (m_complete) {
        stop();

        if (m_autoPlay) {
            play();
        } else if (m_autoLoad) {
            load();
        }
    }
}

void QmlAVPlayer::updateMetrics()
{
    qint64 elapsed = m_fpsTimer.restart();
    if (elapsed > 0) {
        double currentFps = (m_presentedFramesCount * 1000.0) / elapsed;
        setFps(std::round(currentFps * 10.0) / 10.0);
    }
    m_presentedFramesCount = 0;

    if (m_demuxer) {
        auto stat = m_demuxer->stat();
        setFramesDecoded(stat.value("videoFramesDecoded").toInt());
        setFramesDiscarded(stat.value("videoFramesDiscarded").toInt());

        QString codec = m_demuxer->videoCodecName();
        if (!codec.isEmpty()) {
            setVideoCodec(codec);
        }

        QSize res = m_demuxer->videoResolution();
        if (res.isValid()) {
            setVideoResolution(QString("%1x%2").arg(res.width()).arg(res.height()));
        }

        int64_t br = m_demuxer->bitrate();
        if (br > 0) {
            setBitrate(br / 1000); // in kbps
        }

        setIsHWAccelerated(m_demuxer->isHWAccelerated());
    }
}

void QmlAVPlayer::setPlaybackState(const QMediaPlayer::State state)
{
    if (m_playbackState == state) {
        return;
    }

    logDebug() << QString("setPlaybackState(state=%1)").arg(state);

    m_playbackState = state;

    if (sender()) {
        stateMachine();
    }

    emit playbackStateChanged(state);
}

void QmlAVPlayer::setStatus(const QMediaPlayer::MediaStatus status)
{
    if (m_status == status) {
        return;
    }

    logDebug() << QString("setStatus(status=%1)").arg(status);

    m_status = status;

    stateMachine();

    emit statusChanged(status);
}

void QmlAVPlayer::setHasVideo(bool hasVideo)
{
    if (m_hasVideo == hasVideo) {
        return;
    }

    logDebug() << QString("setHasVideo(hasVideo=%1)").arg(hasVideo);

    m_hasVideo = hasVideo;

    emit hasVideoChanged(hasVideo);
}

void QmlAVPlayer::setHasAudio(bool hasAudio)
{
    if (m_hasAudio == hasAudio) {
        return;
    }

    logDebug() << QString("setHasAudio(hasAudio=%1)").arg(hasAudio);

    m_hasAudio = hasAudio;

    emit hasAudioChanged(hasAudio);
}

void QmlAVPlayer::setFps(double fps)
{
    if (qFuzzyCompare(m_fps, fps)) return;
    m_fps = fps;
    emit fpsChanged(fps);
}

void QmlAVPlayer::setBitrate(qint64 bitrate)
{
    if (m_bitrate == bitrate) return;
    m_bitrate = bitrate;
    emit bitrateChanged(bitrate);
}

void QmlAVPlayer::setVideoCodec(const QString &codec)
{
    if (m_videoCodec == codec) return;
    m_videoCodec = codec;
    emit videoCodecChanged(codec);
}

void QmlAVPlayer::setVideoResolution(const QString &res)
{
    if (m_videoResolution == res) return;
    m_videoResolution = res;
    emit videoResolutionChanged(res);
}

void QmlAVPlayer::setIsHWAccelerated(bool hw)
{
    if (m_isHWAccelerated == hw) return;
    m_isHWAccelerated = hw;
    emit isHWAcceleratedChanged(hw);
}

void QmlAVPlayer::setFramesDecoded(int count)
{
    if (m_framesDecoded == count) return;
    m_framesDecoded = count;
    emit framesDecodedChanged(count);
}

void QmlAVPlayer::setFramesDiscarded(int count)
{
    if (m_framesDiscarded == count) return;
    m_framesDiscarded = count;
    emit framesDiscardedChanged(count);
}

void QmlAVPlayer::setReconnecting(bool reconnecting)
{
    if (m_reconnecting == reconnecting) return;
    m_reconnecting = reconnecting;
    emit reconnectingChanged(reconnecting);
}

void QmlAVPlayer::setReconnectAttempt(int attempt)
{
    if (m_reconnectAttempt == attempt) return;
    m_reconnectAttempt = attempt;
    emit reconnectAttemptChanged(attempt);
}
