#include "thread_pool.h"
#include <functional>
#include <mutex>
#include <shared_mutex>

namespace thread_pool {

std::unique_ptr<Task> ThreadPool::get_element_or_quit() & {
    std::unique_lock<std::mutex> uLock(mQueueMtx);
    mQueueNotEmptyCv.wait(uLock, [this]() { return mMode == QueueMode::RETURN_NULLPTR || !mQueue.empty(); });
    if (mQueue.empty()) {
        return nullptr;
    }

    auto ptr = std::make_unique<Task>(std::move(mQueue.front()));
    mQueue.pop();
    return ptr;
}

void ThreadPool::change_mode(QueueMode mode) & {
    std::unique_lock<std::mutex> uLock(mQueueMtx);
    if (mMode == mode) {
        return;
    }
    mMode = mode;
    if (mMode == QueueMode::RETURN_NULLPTR) {
        uLock.unlock();
        mQueueNotEmptyCv.notify_all();
    }
}

ThreadPool::ThreadPool(std::size_t maxThreads) noexcept : mMaxThreads(maxThreads) {
    mThreads.reserve(maxThreads);
}

void ThreadPool::worker() {
    while (true) {  // not ub because we wait
        auto res = get_element_or_quit();
        if (res) {
            res->func();  // notifies future object
        } else {  // quit
            mQueueEmptyCv.notify_all();
            break;
        }
    }
}

void ThreadPool::add_thread_if_possible() {
    if (mThreads.size() < mMaxThreads) {
        mThreads.emplace_back(&ThreadPool::worker, this);
    }
}

void ThreadPool::wait(std::size_t taskId) {
    std::future<void> future;
    {
        std::shared_lock<std::shared_timed_mutex> const sLock(mTaskStatusMtx);
        auto iter = mTaskStatus.find(taskId);
        if (iter != mTaskStatus.end()) {
            future = std::move(iter->second);
        }  // else .wait() called before .add_task()
    }
    if (future.valid()) {
        future.wait();
    } else {
        throw std::runtime_error("ThreadPool: waiting on task that doesn't yet exist");
    }
}

ThreadPool::~ThreadPool() noexcept {

}
void ThreadPool::wait_all() {
    change_mode(QueueMode::RETURN_NULLPTR);
    {
        std::unique_lock<std::mutex> sLock(mQueueMtx);
        mQueueEmptyCv.wait(sLock, [this](){
            return mQueue.empty();
        });
    }
    // if you want to add tasks after .wait_all()
    change_mode(QueueMode::WAIT_FOR_ELEMENT);
}

template <typename Func, typename... Args>
std::size_t ThreadPool::add_task(std::function<Func> &&function, Args &&...args) {

    std::size_t taskId{};
    std::future<void> future;
    {
        std::unique_lock<std::mutex> const uLock(mQueueMtx);
        add_thread_if_possible();
        taskId = mCntAdded++;
        auto task = std::packaged_task<void()>(std::move(function), std::forward<Args>(args)...);
        future = task.get_future();
        // if wait() is called >1 times, add .shared()
        mQueue.push({std::move(task), taskId});
    }
    mQueueNotEmptyCv.notify_one();  // calling under mutex is pessimization
    {
        std::unique_lock<std::shared_timed_mutex> const uLock(mTaskStatusMtx);
        mTaskStatus[taskId] = std::move(future);
    }
    return taskId;
}

}