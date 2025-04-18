#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include "common.hpp"

class ThreadPool {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency()) 
        : stop_(false) {
        debugPrint("Creating ThreadPool with {} threads", numThreads);
    }

    ~ThreadPool() {
        stop();
        debugPrint("ThreadPool destroyed");
    }

    void start();
    void stop();
    void submitTask(Task task);

private:
    std::vector<std::thread> workers_;
    std::queue<Task> tasks_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    bool stop_;

    void workerFunction();
}; 