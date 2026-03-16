#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace LibIndexer::Indexer {

template<typename T>
class CThreadSafeQueue
{
public:
    explicit CThreadSafeQueue(size_t maxSize = 0) : m_maxSize(maxSize) {}

    void Push(T value)
    {
        std::unique_lock lock(m_mutex);
        if (m_maxSize > 0)
            m_cvNotFull.wait(lock, [&]{ return m_queue.size() < m_maxSize || m_closed; });
        if (m_closed) return;
        m_queue.push(std::move(value));
        m_cvNotEmpty.notify_one();
    }

    [[nodiscard]] std::optional<T> Pop()
    {
        std::unique_lock lock(m_mutex);
        m_cvNotEmpty.wait(lock, [&]{ return !m_queue.empty() || m_closed; });
        if (m_queue.empty()) return std::nullopt;
        T value = std::move(m_queue.front());
        m_queue.pop();
        m_cvNotFull.notify_one();
        return value;
    }

    void Close()
    {
        std::lock_guard lock(m_mutex);
        m_closed = true;
        m_cvNotEmpty.notify_all();
        m_cvNotFull.notify_all();
    }

    [[nodiscard]] size_t Size() const
    {
        std::lock_guard lock(m_mutex);
        return m_queue.size();
    }

private:
    std::queue<T>           m_queue;
    mutable std::mutex      m_mutex;
    std::condition_variable m_cvNotEmpty;
    std::condition_variable m_cvNotFull;
    size_t                  m_maxSize{0};
    bool                    m_closed{false};
};

} // namespace LibIndexer::Indexer
