#include "thread_pool.hpp"
#include "common.hpp"

void ThreadPool::start() {
    debugPrint("Starting ThreadPool");
    for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
        workers_.emplace_back(&ThreadPool::workerFunction, this);
    }
}

void ThreadPool::stop() {
    debugPrint("Stopping ThreadPool");
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        stop_ = true;
    }
    condition_.notify_all();
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void ThreadPool::submitTask(Task task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        tasks_.push(std::move(task));
    }
    condition_.notify_one();
}

void ThreadPool::workerFunction() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            condition_.wait(lock, [this] {
                return stop_ || !tasks_.empty();
            });

            if (stop_ && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }
        try {
            task();
        } catch (const std::exception& e) {
            // Log error hoặc xử lý exception
            debugPrint("Task execution failed: {}", e.what());
        }
    }
} 