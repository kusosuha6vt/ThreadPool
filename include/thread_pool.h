#ifndef THREADPOOL_THREAD_POOL_H
#define THREADPOOL_THREAD_POOL_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace thread_pool {

enum class QueueMode {
    WAIT_FOR_ELEMENT,  // get_element_or_quit will wait then !q.empty()
    RETURN_NULLPTR     // get_element_or_quit will return nullptr if q.empty()
};

struct Task {
    std::packaged_task<void()> func;
    std::size_t id;
};

class ThreadPool {
public:
    ThreadPool(std::size_t maxThreads) noexcept;
    ~ThreadPool() noexcept;

    ThreadPool(ThreadPool &&) = delete;
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(ThreadPool &&) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

    template <typename Func, typename ...Args>
    std::size_t add_task(std::function<Func> &&function, Args &&... args);

    void wait(std::size_t taskId);

    void wait_all();

private:
    std::queue<Task> mQueue;
    std::mutex mQueueMtx;
    std::condition_variable mQueueNotEmptyCv;
    std::condition_variable mQueueEmptyCv;
    QueueMode mMode = QueueMode::WAIT_FOR_ELEMENT;
    std::size_t mCntAdded = 0;

    std::size_t mMaxThreads;

    std::vector<std::thread> mThreads;

    std::unordered_map<std::size_t, std::future<void>> mTaskStatus;
    std::shared_timed_mutex mTaskStatusMtx;
    std::condition_variable mTaskStatusCv;

    void worker();

    void add_thread_if_possible();

    void change_mode(QueueMode mode) &;

    // should be std::optional
    std::unique_ptr<Task> get_element_or_quit() &;
};

}  // namespace thread_pool

#endif  // THREADPOOL_THREAD_POOL_H
