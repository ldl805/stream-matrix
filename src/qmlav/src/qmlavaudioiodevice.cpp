#include "qmlavaudioiodevice.h"
#include "qmlavframe.h"

QmlAVAudioIODevice::QmlAVAudioIODevice(QObject *parent)
    : QIODevice(parent)
{
    open(QIODevice::ReadOnly);
}

QmlAVAudioIODevice::~QmlAVAudioIODevice()
{
    close();
}

void QmlAVAudioIODevice::enqueue(const std::shared_ptr<QmlAVAudioFrame> frame)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frames.push_back(frame);
}

void QmlAVAudioIODevice::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frames.clear();
}

qint64 QmlAVAudioIODevice::readData(char *data, qint64 maxSize)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    qint64 totalRead = 0;

    while (totalRead < maxSize && !m_frames.empty()) {
        auto &f = m_frames.front();
        size_t needed = static_cast<size_t>(maxSize - totalRead);
        size_t read = f->readData(reinterpret_cast<uint8_t *>(data + totalRead), needed);
        totalRead += read;

        if (f->dataSize() == 0 || read == 0) {
            m_frames.pop_front();
        }
    }

    return totalRead;
}
