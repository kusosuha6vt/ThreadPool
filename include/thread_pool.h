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

// struct Task {
//     std::packaged_task<void()> func;
//     std::size_t id;
// };

class ThreadPool {
public:
    ThreadPool(std::size_t maxThreads) noexcept;
    ~ThreadPool() noexcept;

    ThreadPool(ThreadPool &&) = delete;
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(ThreadPool &&) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

    template <typename Func, typename... Args>
    std::size_t add_task(Func &&function, Args &&...args) {
        std::size_t taskId = mCntAdded++;
        auto task = std::packaged_task<void()>(
            [argFunc = std::bind(std::forward<Func>(function),
                                 std::forward<Args>(args)...)]() {
                argFunc();  // set return value to void
            });
        {
            std::unique_lock<std::shared_timed_mutex> const uLock(
                mUnfinishedTasksMtx);
            mUnfinishedTasks[taskId] = task.get_future();
        }
        // it's better to set mUnfinishedTasks _before_ setting mQueue
        // for .wait_all() method skipping new tasks less frequently
        {
            std::unique_lock<std::mutex> const uLock(mQueueMtx);
            add_thread_if_possible();
            taskId = mCntAdded++;
            mQueue.emplace(std::move(task));
        }
        mQueueNotEmptyCv
            .notify_one();  // notifying under mutex is pessimization

        return taskId;
    }

    void wait(std::size_t taskId);

    void wait_all();

private:
    std::queue<std::packaged_task<void()>> mQueue;  // guarded by mQueueMtx
    std::mutex mQueueMtx;
    std::condition_variable mQueueNotEmptyCv;
    std::atomic<QueueMode> mMode{
        QueueMode::WAIT_FOR_ELEMENT};  // guarded by mQueueMtx
    std::atomic<std::size_t> mCntAdded{0};

    std::size_t mMaxThreads;  // guarded by mQueueMtx

    std::vector<std::thread> mThreads;  // guarded by mQueueMtx

    std::unordered_map<std::size_t, std::future<void>>
        mUnfinishedTasks;  // guarded by mUnfinishedTasksMtx
    std::shared_timed_mutex mUnfinishedTasksMtx;

    void worker();

    void add_thread_if_possible();

    void change_mode(QueueMode mode) &;

    // should be std::optional
    std::unique_ptr<std::packaged_task<void()>> get_element_or_quit() &;
};

}  // namespace thread_pool

#endif  // THREADPOOL_THREAD_POOL_H
