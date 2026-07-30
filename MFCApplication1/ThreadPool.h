// ThreadPool.h: Lightweight thread pool for background tasks
#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <future>

class CThreadPool
{
public:
    explicit CThreadPool(size_t nThreads = 4)
        : m_bStop(false), m_bJoined(false)
    {
        for (size_t i = 0; i < nThreads; ++i)
        {
            m_workers.emplace_back([this] {
                while (true)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_cv.wait(lock, [this] {
                            return m_bStop.load() || !m_tasks.empty();
                        });
                        if (m_bStop.load() && m_tasks.empty())
                            return;
                        task = std::move(m_tasks.front());
                        m_tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ~CThreadPool()
    {
        Join();
    }

    // Non-copyable, non-movable
    CThreadPool(const CThreadPool&) = delete;
    CThreadPool& operator=(const CThreadPool&) = delete;

    // Submit a task and return a future
    template<typename F, typename... Args>
    auto Submit(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>>
    {
        using ReturnType = typename std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<ReturnType> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_bStop.load())
                throw std::runtime_error("ThreadPool is stopped");
            m_tasks.emplace([task]() { (*task)(); });
        }
        m_cv.notify_one();
        return result;
    }

    // Submit a fire-and-forget task (no return value)
    void Enqueue(std::function<void()> task)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_bStop.load()) return;
            m_tasks.emplace(std::move(task));
        }
        m_cv.notify_one();
    }

    // Wait for all queued tasks to complete, then stop workers
    void Join()
    {
        if (m_bJoined.exchange(true))
            return;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_bStop.store(true);
        }
        m_cv.notify_all();
        for (auto& worker : m_workers)
        {
            if (worker.joinable())
                worker.join();
        }
    }

    size_t PendingCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tasks.size();
    }

private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_bStop;
    std::atomic<bool> m_bJoined;
};