#ifndef QMLAVWAITINGQUEUE_H
#define QMLAVWAITINGQUEUE_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

template<typename T>
class QmlAVWaitingQueue
{
public:
    QmlAVWaitingQueue()
        : m_interrupted(false)
        , m_producerLimit(0) // Unlim
        , m_consumerLimit(1) { }
    virtual ~QmlAVWaitingQueue()
    {
        requestInterrupt();
    }

    QmlAVWaitingQueue(const QmlAVWaitingQueue &other) = delete;
    QmlAVWaitingQueue(QmlAVWaitingQueue &&other) {
        std::scoped_lock lock(m_mutex, other.m_mutex);
        m_queue = std::move(other.m_queue);
        m_interrupted = other.m_interrupted.load();
        m_producerLimit = other.m_producerLimit;
        m_consumerLimit = other.m_consumerLimit;
    }

    QmlAVWaitingQueue &operator=(const QmlAVWaitingQueue &other) = delete;
    QmlAVWaitingQueue &operator=(QmlAVWaitingQueue &&other) {
        std::scoped_lock lock(m_mutex, other.m_mutex);
        if (this != std::addressof(other)) {
            m_queue = std::move(other.m_queue);
            m_interrupted = other.m_interrupted.load();
            m_producerLimit = other.m_producerLimit;
            m_consumerLimit = other.m_consumerLimit;
        }
        return *this;
    }

    template<typename URef>
    bool enqueue(URef &&value) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_producerCond.wait(lock, [&] {
                return m_interrupted.load(std::memory_order_relaxed) ||
                       m_producerLimit == 0 ||
                       m_queue.size() < m_producerLimit;
            });

            if (m_interrupted.load(std::memory_order_relaxed)) {
                return false;
            }

            m_queue.push(std::forward<T>(value));
        }
        m_consumerCond.notify_one();
        return true;
    }

    bool head(T &value) {
        std::unique_lock<std::mutex> lock(m_mutex);

        m_consumerCond.wait(lock, [&] {
            return m_interrupted.load(std::memory_order_relaxed) ||
                   m_queue.size() >= m_consumerLimit;
        });

        if (m_queue.empty()) {
            return false;
        }

        value = m_queue.front();
        return true;
    }

    bool dequeue(T &value) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_consumerCond.wait(lock, [&] {
                return m_interrupted.load(std::memory_order_relaxed) ||
                       m_queue.size() >= m_consumerLimit;
            });

            if (m_queue.empty()) {
                return false;
            }

            value = std::move(m_queue.front());
            m_queue.pop();
        }
        m_producerCond.notify_all();
        return true;
    }

    void dequeue() {
        {
            std::scoped_lock lock(m_mutex);
            if (!m_queue.empty()) {
                m_queue.pop();
            }
        }
        m_producerCond.notify_all();
    }

    void clear() {
        {
            std::scoped_lock lock(m_mutex);
            while (!m_queue.empty()) {
                m_queue.pop();
            }
        }
        m_producerCond.notify_all();
    }

    void waitForEmpty() {
        std::unique_lock<std::mutex> lock(m_mutex);

        m_producerCond.wait(lock, [&] {
            return m_interrupted.load(std::memory_order_relaxed) || m_queue.empty();
        });
    }

    void requestInterrupt() {
        {
            std::scoped_lock lock(m_mutex);
            m_interrupted.store(true, std::memory_order_release);
            m_producerLimit = 0;
            m_consumerLimit = 0;
        }
        m_producerCond.notify_all();
        m_consumerCond.notify_all();
    }

    void setProducerLimit(size_t limit) {
        {
            std::scoped_lock lock(m_mutex);
            m_producerLimit = limit;
        }
        m_producerCond.notify_all();
    }

    void setConsumerLimit(size_t limit) {
        {
            std::scoped_lock lock(m_mutex);
            m_consumerLimit = limit;
        }
        m_consumerCond.notify_all();
    }

    bool isEmpty() const {
        std::scoped_lock lock(m_mutex);
        return m_queue.empty();
    }

    int length() const {
        std::scoped_lock lock(m_mutex);
        return static_cast<int>(m_queue.size());
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_producerCond;
    std::condition_variable m_consumerCond;

    std::atomic<bool> m_interrupted;
    std::queue<T> m_queue;
    size_t m_producerLimit;
    size_t m_consumerLimit;
};

#endif // QMLAVWAITINGQUEUE_H
