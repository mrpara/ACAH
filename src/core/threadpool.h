// threadpool.h - a small persistent worker pool.
// Threads are created once and parked on a condition variable, so the renderer
// can fan work out every frame without paying thread-creation cost per frame.
//
// parallelFor() waits for every worker to *leave* the batch, not merely for the
// last item to finish. That distinction matters: the renderer issues two batches
// back to back each frame, and a worker still spinning on the shared item
// counter would otherwise start consuming indices belonging to the next batch.
#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace sb {

class ThreadPool {
public:
    ThreadPool() = default;
    ~ThreadPool() { shutdown(); }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // `workers` is the number of *extra* threads; the calling thread also works,
    // so threadCount() is workers + 1.
    void start(int workers) {
        shutdown();
        if (workers < 0) workers = 0;
        stop_ = false;
        generation_ = 0;
        workersActive_.store(0, std::memory_order_relaxed);
        threads_.reserve(static_cast<size_t>(workers));
        for (int i = 0; i < workers; ++i)
            threads_.emplace_back([this] { workerLoop(); });
    }

    void shutdown() {
        if (threads_.empty()) return;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
            ++generation_;
        }
        cvWork_.notify_all();
        for (std::thread& t : threads_) if (t.joinable()) t.join();
        threads_.clear();
    }

    int threadCount() const { return static_cast<int>(threads_.size()) + 1; }

    // Runs fn(i) for every i in [0, count). Blocks until all items are done and
    // every worker has left the batch.
    void parallelFor(int count, const std::function<void(int)>& fn) {
        if (count <= 0) return;
        if (threads_.empty()) {
            for (int i = 0; i < count; ++i) fn(i);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            job_ = &fn;
            jobCount_ = count;
            nextIndex_.store(0, std::memory_order_relaxed);
            workersActive_.store(static_cast<int>(threads_.size()), std::memory_order_release);
            ++generation_;
        }
        cvWork_.notify_all();

        drain(&fn, count);                      // the calling thread pitches in

        std::unique_lock<std::mutex> done(doneMutex_);
        cvDone_.wait(done, [this] { return workersActive_.load(std::memory_order_acquire) == 0; });
        done.unlock();

        std::lock_guard<std::mutex> lock(mutex_);
        job_ = nullptr;
        jobCount_ = 0;
    }

private:
    void workerLoop() {
        uint64_t seen = 0;
        for (;;) {
            const std::function<void(int)>* fn = nullptr;
            int count = 0;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cvWork_.wait(lock, [this, &seen] { return stop_ || generation_ != seen; });
                seen = generation_;
                if (stop_) return;
                fn = job_;                       // read under the lock, never racy
                count = jobCount_;
            }
            if (fn) drain(fn, count);
            if (workersActive_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                std::lock_guard<std::mutex> done(doneMutex_);
                cvDone_.notify_all();
            }
        }
    }

    // Pulls items off the shared counter until the batch is exhausted.
    void drain(const std::function<void(int)>* fn, int count) {
        for (;;) {
            const int i = nextIndex_.fetch_add(1, std::memory_order_relaxed);
            if (i >= count) break;
            (*fn)(i);
        }
    }

    std::vector<std::thread> threads_;
    std::mutex mutex_;
    std::condition_variable cvWork_;
    std::mutex doneMutex_;
    std::condition_variable cvDone_;

    const std::function<void(int)>* job_ = nullptr;
    int jobCount_ = 0;
    std::atomic<int> nextIndex_{0};
    std::atomic<int> workersActive_{0};
    uint64_t generation_ = 0;
    bool stop_ = false;
};

} // namespace sb
