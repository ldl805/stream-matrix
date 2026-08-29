#ifndef QMLAVDEMUXER_H
#define QMLAVDEMUXER_H

extern "C" {
#include <libavutil/time.h>
}

#include <QVideoFrame>
#include <QMediaPlayer>
#include <QVideoSurfaceFormat>
#include <QAudioOutput>
#include <QSize>

#include "qmlavmediacontextholder.h"
#include "qmlavoptions.h"
#include "qmlavthread.h"
#include "qmlavdecoder.h"

class QmlAVInterruptCallback : public AVIOInterruptCB
{
public:
    QmlAVInterruptCallback() {
        opaque = this;
        callback = [](void *opaque) -> int {
            if (!opaque) return 1;
            auto cb = static_cast<QmlAVInterruptCallback *>(opaque);
            if (cb->isAVInterruptRequested()) return 1;
            int64_t expire = cb->m_expireTime.load(std::memory_order_relaxed);
            if (expire > 0 && av_gettime_relative() > expire) return 1;
            return 0;
        };
    }

    QmlAVInterruptCallback(const QmlAVInterruptCallback &other) : AVIOInterruptCB() {
        opaque = this;
        callback = other.callback;
        m_timeout.store(other.m_timeout.load(std::memory_order_relaxed), std::memory_order_relaxed);
        m_expireTime.store(other.m_expireTime.load(std::memory_order_relaxed), std::memory_order_relaxed);
        m_avInterruptRequested.store(other.m_avInterruptRequested.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    QmlAVInterruptCallback &operator=(const QmlAVInterruptCallback &other) {
        if (this != &other) {
            opaque = this;
            callback = other.callback;
            m_timeout.store(other.m_timeout.load(std::memory_order_relaxed), std::memory_order_relaxed);
            m_expireTime.store(other.m_expireTime.load(std::memory_order_relaxed), std::memory_order_relaxed);
            m_avInterruptRequested.store(other.m_avInterruptRequested.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }

    void requestAVInterrupt() { m_avInterruptRequested.store(true, std::memory_order_release); }
    bool isAVInterruptRequested() const { return m_avInterruptRequested.load(std::memory_order_acquire); }

    void setTimeout(int64_t timeout) {
        m_timeout.store(timeout, std::memory_order_relaxed);
        resetTimer();
    }
    void resetTimer() {
        int64_t t = m_timeout.load(std::memory_order_relaxed);
        m_expireTime.store(t > 0 ? (av_gettime_relative() + t) : 0, std::memory_order_relaxed);
    }

private:
    std::atomic<int64_t> m_timeout = 0;
    std::atomic<int64_t> m_expireTime = 0;
    std::atomic<bool> m_avInterruptRequested = false;
};

// NOTE: Public API for GUI thread only!
class QmlAVDemuxer : public QObject
{
    Q_OBJECT

public:
    QmlAVDemuxer(QObject *parent = nullptr);
    virtual ~QmlAVDemuxer();

    void load(const QUrl &url, const QmlAVOptions &avOptions);
    void start();

    const auto &clock() const { return m_context->clock; }
    int64_t startTime() const;

    QVariantMap stat() const;

    QString videoCodecName() const;
    QSize videoResolution() const;
    int64_t bitrate() const;
    bool isHWAccelerated() const;

signals:
    void playbackStateChanged(QMediaPlayer::State state);
    void mediaStatusChanged(QMediaPlayer::MediaStatus status);
    void frameFinished(const std::shared_ptr<QmlAVFrame> frame);

protected:
    auto &context() { return m_context; }

    bool isRealTime(QUrl url) const;
    bool isLoaded() const { return m_context->videoDecoder->isOpen() || m_context->audioDecoder->isOpen(); }
    void initDecoders(const QmlAVOptions &avOptions);

    void frameHandler(const std::shared_ptr<QmlAVFrame> frame);
    
private:
    QmlAVInterruptCallback m_interruptCallback;

    QmlAVThreadLiveController<void> m_loaderThread;
    QmlAVThreadLiveController<QmlAVLoopController> m_demuxerThread;

    std::shared_ptr<QmlAVMediaContextHolder> m_context;

    friend class QmlAVDecoder;
};
Q_DECLARE_METATYPE(std::shared_ptr<QmlAVFrame>)

#endif // QMLAVDEMUXER_H
