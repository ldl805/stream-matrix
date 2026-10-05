#ifndef QMLAVWAITINGQUEUE_H
#define QMLAVWAITINGQUEUE_H

#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iterator>
#include <atomic>

template<typename T>
class QmlAVWaitingQueue
{
public:
    // Returns true if decoding can (re)start cleanly at this element (e.g. a video keyframe)
    using SyncPointPredicate = std::function<bool(const T &)>;

    QmlAVWaitingQueue()
        : m_interrupted(false)
        , m_producerLimit(0) // Unlim
        , m_consumerLimit(1)
        , m_dropOnOverflow(false) { }
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
        m_dropOnOverflow = other.m_dropOnOverflow;
        m_isSyncPoint = std::move(other.m_isSyncPoint);
        m_waitForSync = other.m_waitForSync;
        m_droppedCount = other.m_droppedCount.load();
    }

    QmlAVWaitingQueue &operator=(const QmlAVWaitingQueue &other) = delete;
    QmlAVWaitingQueue &operator=(QmlAVWaitingQueue &&other) {
        std::scoped_lock lock(m_mutex, other.m_mutex);
        if (this != std::addressof(other)) {
            m_queue = std::move(other.m_queue);
            m_interrupted = other.m_interrupted.load();
            m_producerLimit = other.m_producerLimit;
            m_consumerLimit = other.m_consumerLimit;
            m_dropOnOverflow = other.m_dropOnOverflow;
            m_isSyncPoint = std::move(other.m_isSyncPoint);
            m_waitForSync = other.m_waitForSync;
            m_droppedCount = other.m_droppedCount.load();
        }
        return *this;
    }

    template<typename URef>
    bool enqueue(URef &&value) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            if (m_interrupted.load(std::memory_order_relaxed)) {
                return false;
            }

            T item(std::forward<URef>(value));

            // After an overflow flush, discard everything until the next sync point (e.g. keyframe),
            // because those packets cannot be decoded cleanly without their references.
            if (m_waitForSync) {
                if (m_isSyncPoint && !m_isSyncPoint(item)) {
                    ++m_droppedCount;
                    return true;
                }
                m_waitForSync = false;
            }

            if (m_dropOnOverflow && m_producerLimit > 0 && m_queue.size() >= m_producerLimit) {
                // NOTE: The front element is never dropped: the consumer may be processing it right now
                // via head() and will remove it with dequeue() afterwards.
                if (m_isSyncPoint) {
                    // Skip ahead to the newest queued sync point, or drop everything after the front
                    // and wait for the next sync point to arrive. A sync point directly behind the front
                    // is ignored, since keeping it would not free any space.
                    auto first = m_queue.begin() + 1;
                    auto syncIt = m_queue.end();
                    for (auto it = m_queue.end(); it != first; ) {
                        --it;
                        if (it != first && m_isSyncPoint(*it)) {
                            syncIt = it;
                            break;
                        }
                    }
                    const bool foundSync = syncIt != m_queue.end();

                    m_droppedCount += static_cast<size_t>(std::distance(first, syncIt));
                    m_queue.erase(first, syncIt);

                    if (!foundSync && !m_isSyncPoint(item)) {
                        m_waitForSync = true;
                        ++m_droppedCount;
                        return true;
                    }
                } else if (m_queue.size() > 1) {
                    m_queue.erase(m_queue.begin() + 1);
                    ++m_droppedCount;
                }
            } else {
                m_producerCond.wait(lock, [&] {
                    return m_interrupted.load(std::memory_order_relaxed) ||
                           m_producerLimit == 0 ||
                           m_queue.size() < m_producerLimit;
                });
            }

            if (m_interrupted.load(std::memory_order_relaxed)) {
                return false;
            }

            m_queue.push_back(std::move(item));
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
            m_queue.pop_front();
        }
        m_producerCond.notify_all();
        return true;
    }

    void dequeue() {
        {
            std::scoped_lock lock(m_mutex);
            if (!m_queue.empty()) {
                m_queue.pop_front();
            }
        }
        m_producerCond.notify_all();
    }

    void clear() {
        {
            std::scoped_lock lock(m_mutex);
            m_queue.clear();
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

    void setDropOnOverflow(bool drop) {
        std::scoped_lock lock(m_mutex);
        m_dropOnOverflow = drop;
    }

    bool dropOnOverflow() const {
        std::scoped_lock lock(m_mutex);
        return m_dropOnOverflow;
    }

    // When set, overflow handling skips ahead to sync points instead of dropping single elements
    void setSyncPointPredicate(SyncPointPredicate predicate) {
        std::scoped_lock lock(m_mutex);
        m_isSyncPoint = std::move(predicate);
        m_waitForSync = false;
    }

    // Total number of elements discarded because of overflow
    size_t droppedCount() const {
        return m_droppedCount.load(std::memory_order_relaxed);
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
    std::deque<T> m_queue;
    size_t m_producerLimit;
    size_t m_consumerLimit;
    bool m_dropOnOverflow;
    SyncPointPredicate m_isSyncPoint;
    bool m_waitForSync = false;
    std::atomic<size_t> m_droppedCount{0};
};

#endif // QMLAVWAITINGQUEUE_H
