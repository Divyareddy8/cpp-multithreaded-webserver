#include "thread_pool.h"
#include "logger.h"
#include <stdexcept>

ThreadPool::ThreadPool(size_t num_threads) : stop_(false) {
    if (num_threads == 0)
        throw std::invalid_argument("ThreadPool: num_threads must be > 0");

    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&ThreadPool::worker_loop, this);
    }
    LOG_INFO("ThreadPool: started " + std::to_string(num_threads) + " worker threads");
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (stop_.load()) return;
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::shutdown() {
    stop_.store(true);
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();
    LOG_INFO("ThreadPool: all worker threads joined");
}

size_t ThreadPool::queue_size() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

void ThreadPool::worker_loop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this] {
                return stop_.load() || !tasks_.empty();
            });

            if (stop_.load() && tasks_.empty()) return;

            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}
