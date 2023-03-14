#include "thread_pool.h"
#include <functional>
#include <mutex>
#include <shared_mutex>

namespace thread_pool {

std::unique_ptr<std::packaged_task<void()>> ThreadPool::get_element_or_quit()
    & {
    std::unique_lock<std::mutex> uLock(mQueueMtx);
    mQueueNotEmptyCv.wait(uLock, [this]() {
        return mMode == QueueMode::RETURN_NULLPTR || !mQueue.empty();
    });
    if (mMode == QueueMode::RETURN_NULLPTR) {
        return nullptr;
    }

    auto ptr =
        std::make_unique<std::packaged_task<void()>>(std::move(mQueue.front()));
    mQueue.pop();
    return ptr;
}

void ThreadPool::change_mode(QueueMode mode) & {
    if (mMode == mode) {
        return;
    }
    mMode = mode;
    if (mMode == QueueMode::RETURN_NULLPTR) {
        mQueueNotEmptyCv.notify_all();
    }
}

ThreadPool::ThreadPool(std::size_t maxThreads) noexcept
    : mMaxThreads(maxThreads) {
    mThreads.reserve(maxThreads);
}

void ThreadPool::worker() {
    while (true) {  // not ub because we wait
        auto res = get_element_or_quit();
        if (res) {
            (*res)();  // notifies future object
        } else {
            // quit
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
        std::unique_lock<std::shared_timed_mutex> const uLock(
            mUnfinishedTasksMtx);
        auto iter = mUnfinishedTasks.find(taskId);
        if (iter != mUnfinishedTasks.end()) {
            future = std::move(iter->second);
        }
    }
    if (future.valid()) {
        future.wait();
        std::unique_lock<std::shared_timed_mutex> const uLock(
            mUnfinishedTasksMtx);
        mUnfinishedTasks.erase(taskId);
        // erasing takes more time in .wait()
        // but also less time in .wait_all()
    }
    // else: taskId is wrong or was already waited on => do nothing
}

ThreadPool::~ThreadPool() noexcept {
    // should I add wait_all()?
    change_mode(QueueMode::RETURN_NULLPTR);
    for (auto &thread : mThreads) {
        thread.join();
    }
}

void ThreadPool::wait_all() {
    std::unique_lock<std::shared_timed_mutex> const queueLock(
        mUnfinishedTasksMtx);
    for (auto &idFuture : mUnfinishedTasks) {
        idFuture.second.wait();
    }
    mUnfinishedTasks.clear();
    // possible optimization:
    // mUnfinishedTasks: unordered_map<id, std::shared_ptr<std::future<void>>>
    // 1) {create snapshot of mUnfinishedTasks} under mutex
    // 2) wait on snapshot
    // 3) {delete all ids from snapshot that weren't yet deleted} under mutex
}

}  // namespace thread_pool