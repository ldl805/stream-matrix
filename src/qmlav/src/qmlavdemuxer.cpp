#include "qmlavdemuxer.h"

#include <QFileInfo>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
}

QmlAVDemuxer::QmlAVDemuxer(QObject *parent)
    : QObject(parent)
    , m_context(std::make_shared<QmlAVMediaContextHolder>(this))
{
}

QmlAVDemuxer::~QmlAVDemuxer()
{
    m_interruptCallback.requestAVInterrupt();

    // 1. First interrupt decoders without waiting so their packet queues (which might be full
    // and blocking m_demuxerThread in enqueue()) wake up waiting producer threads immediately.
    if (m_context) {
        if (m_context->videoDecoder) {
            m_context->videoDecoder->requestInterrupt(false);
        }
        if (m_context->audioDecoder) {
            m_context->audioDecoder->requestInterrupt(false);
        }
    }

    // 2. Interrupt and wait for loader and demuxer threads
    m_loaderThread.requestInterrupt(true);
    m_demuxerThread.requestInterrupt(true);

    // 3. Immediately close RTSP input context and socket to send TEARDOWN to camera
    if (m_context && m_context->avFormatCtx) {
        avformat_close_input(&m_context->avFormatCtx);
    }

    // 4. Important! Serialization point for decoders dtor's
    if (m_context) {
        if (m_context->videoDecoder) {
            m_context->videoDecoder->requestInterrupt(true);
        }
        if (m_context->audioDecoder) {
            m_context->audioDecoder->requestInterrupt(true);
        }
    }
}

static QString formatAvError(int ret, const QmlAVInterruptCallback &cb)
{
    if (cb.hasTimedOut() || (ret == AVERROR_EXIT && !cb.isAVInterruptRequested())) {
        return QObject::tr("Connection timed out");
    }
    char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    if (av_strerror(ret, errbuf, sizeof(errbuf)) == 0) {
        return QString::fromUtf8(errbuf);
    }
    return QObject::tr("Error %1").arg(ret);
}

void QmlAVDemuxer::load(const QUrl &url, const QmlAVOptions &avOptions)
{
    int ret = AVERROR_UNKNOWN;
    QString source(url.toString());

    if (m_context->avFormatCtx->iformat) {
        return;
    }

    if (url.isLocalFile()) {
        avdevice_register_all();
        source = url.toLocalFile();
    } else if (QFileInfo::exists(source)) {
        avdevice_register_all();
    } else if (url.scheme().startsWith(QLatin1String("rtsp"), Qt::CaseInsensitive) &&
               url.path().length() > 1 && source.endsWith(QLatin1Char('/'))) {
        source.chop(1);
    }

    if (source.isEmpty()) {
        logInfo() << "Source is emty!";
        emit errorOccurred(tr("No source specified"));
        emit mediaStatusChanged(QMediaPlayer::NoMedia);
        return;
    }

    #if (LIBAVFORMAT_VERSION_MAJOR < 58)
    av_register_all();
#endif
    avformat_network_init();

    m_context->clock.realTime = avOptions.realTime().value_or(isRealTime(url));

    m_interruptCallback.setTimeout(avOptions.demuxerTimeout());
    m_context->avFormatCtx->interrupt_callback = m_interruptCallback;

    emit mediaStatusChanged(QMediaPlayer::LoadingMedia);

    m_loaderThread = QmlAVThread::run([=]() mutable {
        AVDictionaryPtr dict = static_cast<AVDictionaryPtr>(avOptions);

        if (m_context->clock.realTime) {
            if (url.scheme() == "rtsp") {
                if (!dict.get("rtsp_transport").has_value()) {
                    dict.set("rtsp_transport", "tcp");
                }
                if (!dict.get("timeout").has_value() && !dict.get("stimeout").has_value()) {
                    dict.set("timeout", "20000000"); // 20s socket I/O timeout (microseconds)
                }
            }
            if (!dict.get("analyzeduration").has_value()) {
                dict.set("analyzeduration", "5000000"); // 5 second analysis window for longer camera GOPs
            }
            if (!dict.get("probesize").has_value()) {
                dict.set("probesize", "5000000"); // 5 MB probe size for high-bitrate CBR bursts
            }
        }

        ret = avformat_open_input(&m_context->avFormatCtx,
                                  source.toUtf8(),
                                  avOptions.avInputFormat(),
                                  dict);
        if (ret < 0) {
            QString errStr = formatAvError(ret, m_interruptCallback);
            logWarning() << QString("Unable to open input file: \"%1\" (%2)").arg(errStr).arg(ret);
            emit errorOccurred(errStr);
            emit mediaStatusChanged(QMediaPlayer::InvalidMedia);
            return;
        }

        if (m_context->clock.realTime) {
            m_context->avFormatCtx->flags |= AVFMT_FLAG_DISCARD_CORRUPT;
        }

        logDebug() << "avformat_open_input() options ignored: " << QmlAV::Quote << dict.toString();

        ret = avformat_find_stream_info(m_context->avFormatCtx, nullptr);
        if (ret < 0) {
            QString errStr = formatAvError(ret, m_interruptCallback);
            logWarning() << QString("Cannot find stream information: \"%1\" (%2)").arg(errStr).arg(ret);
            emit errorOccurred(errStr);
            emit mediaStatusChanged(QMediaPlayer::InvalidMedia);
            return;
        }

        logDebug() << "--- DUMP FORMAT BEGIN ---";
        if (QmlAVUtils::loggingCategory().isDebugEnabled()) {
            av_dump_format(m_context->avFormatCtx, 0, source.toUtf8(), 0);
        }
        logDebug() << "--- DUMP FORMAT END ---";

        initDecoders(avOptions);

        if (!isLoaded()) {
            logWarning() << "Unable to open any decoder";
            emit errorOccurred(tr("Unable to open any decoder"));
            emit mediaStatusChanged(QMediaPlayer::InvalidMedia);
            return;
        }

        emit mediaStatusChanged(QMediaPlayer::LoadedMedia);
        logDebug() << "Media loaded successfully!";

        emit mediaStatusChanged(QMediaPlayer::BufferedMedia); // NOTE: In common case, we don't use buffering to reduce latency
    });
}

void QmlAVDemuxer::start()
{
    if (m_demuxerThread.isRunning()) {
        return;
    }

    emit playbackStateChanged(QMediaPlayer::PlayingState);

    auto stop = [this](QMediaPlayer::MediaStatus status = QMediaPlayer::InvalidMedia) {
        emit mediaStatusChanged(status);
        emit playbackStateChanged(QMediaPlayer::StoppedState);
        return QmlAVLoopController::Break;
    };

    m_demuxerThread = QmlAVThread::loop([=]() mutable -> QmlAVLoopController {
        int ret;
        AVPacketPtr avPacket;

        m_loaderThread.waitForFinished();

        if (!isLoaded()) {
            return stop();
        }

        // Dynamically toggle AVDISCARD_ALL on the audio stream when muted so FFmpeg
        // skips demuxing audio RTP packets altogether for muted viewports.
        int audioIdx = m_context->audioDecoder->streamIndex();
        if (audioIdx >= 0 && audioIdx < static_cast<int>(m_context->avFormatCtx->nb_streams)) {
            bool audioOn = m_audioEnabled.load(std::memory_order_relaxed);
            AVDiscard targetDiscard = audioOn ? AVDISCARD_DEFAULT : AVDISCARD_ALL;
            if (m_context->avFormatCtx->streams[audioIdx]->discard != targetDiscard) {
                m_context->avFormatCtx->streams[audioIdx]->discard = targetDiscard;
                if (!audioOn) {
                    m_context->audioDecoder->clearPacketQueue();
                }
            }
        }

        m_interruptCallback.resetTimer();

        ret = av_read_frame(m_context->avFormatCtx, avPacket);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                m_context->videoDecoder->waitForEmptyPacketQueue();
                m_context->audioDecoder->waitForEmptyPacketQueue();

                logDebug() << "End of media";
                return stop(QMediaPlayer::EndOfMedia);
            }

            if (ret != AVERROR_EXIT) {
                QString errStr = formatAvError(ret, m_interruptCallback);
                logWarning() << QString("Unable read frame: \"%1\" (%2)").arg(errStr).arg(ret);
                emit errorOccurred(errStr);
            } else if (m_interruptCallback.hasTimedOut()) {
                emit errorOccurred(tr("Connection timed out"));
            }

            return stop();
        }

        if (avPacket->size > 0) {
            m_bytesReadSinceLastCheck.fetch_add(avPacket->size, std::memory_order_relaxed);
        }

        if (avPacket->stream_index == m_context->videoDecoder->streamIndex()) {
            if (!m_context->videoDecoder->decodeAVPacket(std::move(avPacket))) {
                return stop();
            }
        } else if (avPacket->stream_index == audioIdx) {
            if (m_audioEnabled.load(std::memory_order_relaxed)) {
                if (!m_context->audioDecoder->decodeAVPacket(std::move(avPacket))) {
                    return stop();
                }
            }
        }

        return QmlAVLoopController::Continue;
    });
}

int64_t QmlAVDemuxer::startTime() const
{
    if (m_context->clock.startTime == 0) {
        m_context->clock.startTime = QmlAVDecoder::Clock::now();
    }

    return m_context->clock.startTime;
}

QVariantMap QmlAVDemuxer::stat() const
{
    auto &vc = m_context->videoDecoder->counters();
    auto &ac = m_context->audioDecoder->counters();
    return {
        { "videoPacketsDecoded", vc.packetsDecoded.get() },
        { "videoFramesDecoded", vc.framesDecoded.get() },
        { "videoFramesDiscarded", vc.framesDiscarded.get() },
        { "videoPacketsDropped", static_cast<qulonglong>(m_context->videoDecoder->packetsDropped()) },
        { "audioPacketsDecoded", ac.packetsDecoded.get() },
        { "audioBuffersDecoded", ac.framesDecoded.get() },
        { "audioBuffersDiscarded", ac.framesDiscarded.get() }
    };
}

QString QmlAVDemuxer::videoCodecName() const
{
    if (m_context && m_context->videoDecoder && m_context->videoDecoder->isOpen()) {
        return m_context->videoDecoder->name();
    }
    return QString();
}

QSize QmlAVDemuxer::videoResolution() const
{
    int w = m_videoWidth.load(std::memory_order_relaxed);
    int h = m_videoHeight.load(std::memory_order_relaxed);
    if (w > 0 && h > 0) {
        return QSize(w, h);
    }
    return QSize();
}

int64_t QmlAVDemuxer::bitrate() const
{
    int64_t nowUs = av_gettime_relative();
    int64_t bytes = m_bytesReadSinceLastCheck.exchange(0, std::memory_order_relaxed);
    if (m_lastBitrateCheckUs > 0 && nowUs > m_lastBitrateCheckUs) {
        int64_t elapsedUs = nowUs - m_lastBitrateCheckUs;
        if (bytes > 0) {
            m_measuredBitrate = (bytes * 8 * 1000000LL) / elapsedUs;
        }
    }
    m_lastBitrateCheckUs = nowUs;

    if (m_measuredBitrate > 0) {
        return m_measuredBitrate;
    }
    return m_containerBitrate.load(std::memory_order_relaxed);
}

bool QmlAVDemuxer::isHWAccelerated() const
{
    if (m_context && m_context->videoDecoder) {
        return m_context->videoDecoder->isHWAccelerated();
    }
    return false;
}

bool QmlAVDemuxer::isRealTime(QUrl url) const
{
    if (url.scheme() == "rtp"
            || url.scheme() == "srtp"
            || url.scheme().startsWith("rtmp") // rtmp{, e, s, t, te, ts}
            || url.scheme() == "rtsp"
            || url.scheme() == "udp") {
        return true;
    }

    return false;
}

void QmlAVDemuxer::initDecoders(const QmlAVOptions &avOptions)
{
    int bestVideoStream = av_find_best_stream(m_context->avFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (bestVideoStream >= 0 && !avOptions.videoDisable()) {
        if (m_context->videoDecoder->open(m_context->avFormatCtx->streams[bestVideoStream], avOptions)) {
            auto par = m_context->avFormatCtx->streams[bestVideoStream]->codecpar;
            if (par) {
                m_videoWidth.store(par->width, std::memory_order_relaxed);
                m_videoHeight.store(par->height, std::memory_order_relaxed);
            }
            logDebug() << QString("Codec \"%1\" for stream #%2 opened.").arg(m_context->videoDecoder->name()).arg(bestVideoStream);
        } else {
            bestVideoStream = -1;
        }
    } else {
        bestVideoStream = -1;
    }

    if (m_context->avFormatCtx) {
        m_containerBitrate.store(m_context->avFormatCtx->bit_rate, std::memory_order_relaxed);
    }

    int bestAudioStream = av_find_best_stream(m_context->avFormatCtx, AVMEDIA_TYPE_AUDIO, -1, bestVideoStream, nullptr, 0);
    if (bestAudioStream >= 0 && !avOptions.audioDisable()) {
        if (m_context->audioDecoder->open(m_context->avFormatCtx->streams[bestAudioStream], avOptions)) {
            logDebug() << QString("Codec \"%1\" for stream #%2 opened.").arg(m_context->audioDecoder->name()).arg(bestAudioStream);
        } else {
            bestAudioStream = -1;
        }
    } else {
        bestAudioStream = -1;
    }

    // Instruct FFmpeg demuxer to discard packets from any unused streams
    for (unsigned int i = 0; i < m_context->avFormatCtx->nb_streams; ++i) {
        if (static_cast<int>(i) != bestVideoStream && static_cast<int>(i) != bestAudioStream) {
            m_context->avFormatCtx->streams[i]->discard = AVDISCARD_ALL;
        }
    }
}

void QmlAVDemuxer::frameHandler(const std::shared_ptr<QmlAVFrame> frame)
{
    emit frameFinished(frame);
}
