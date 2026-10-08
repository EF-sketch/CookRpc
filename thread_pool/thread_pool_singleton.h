#pragma once

#include "thread_pool.h"
#include <memory>
#include <mutex>
#include <chrono>
#include <future> 

namespace meeting_ctrl {
    class ThreadPoolSingleton {
    public:
        static bool init(size_t threads = std::thread::hardware_concurrency()) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (instance_) {
                return false;
            } 
            instance_ = std::make_unique<ThreadPool>(threads);
            return true;
        } 

        static bool init(const ThreadPoolStruct& config) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (instance_) {
                return false;
            }
            instance_ = std::make_unique<ThreadPool>(config);
            return true;
        }

        static ThreadPool& getInstance() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!instance_) {
                instance_ = std::make_unique<ThreadPool>(std::thread::hardware_concurrency());
            }
            return *instance_;
        }

        static ThreadPool& getInstance(const ThreadPoolStruct& config) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!instance_) {
                instance_ = std::make_unique<ThreadPool>(config);
            }
            return *instance_;
        }

        template<class F, class... Args>
        static auto enqueue(TaskPriority priority, F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
            return getInstance().enqueue(priority, std::forward<F>(f), std::forward<Args>(args)...);
        }

        static ThreadPool::Stats getStats() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!instance_) {
                return ThreadPool::Stats{};
            }
            return instance_->get_stats();
        }

        static size_t getQueueSize() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!instance_) {
                return 0;
            }
            return instance_->queue_size();
        }

        static size_t getPoolSize() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!instance_) {
                return 0;
            }
            instance_->pause();
            return true;
        }

        static bool resume() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!instance_) {
                return false;
            }

            instance_->resume();
            return true;
        }

        static bool shutdown(std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!instance_) {
                return true;
            }

            bool success = instance_->shutdown(timeout);
            if (success) {
                instance_.reset();
            }
            return success;
        }

        static void destroy() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (instance_) {
                instance_->stop_now();
                instance_.reset();
            }
        }

        static bool exists() {
            std::lock_guard<std::mutex> lock(mutex_);
            return static_cast<bool>(instance_);
        }

        static ThreadPoolState getState() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!instance_) {
                return ThreadPoolState::STOPPED;
            }
            return instance_->get_state();
        }

        ThreadPoolSingleton(const ThreadPoolSingleton&) = delete;
        ThreadPoolSingleton& operator=(const ThreadPoolSingleton&) = delete;

    private:
        ThreadPoolSingleton() = default;
        
        static std::unique_ptr<ThreadPool> instance_;
        static std::mutex mutex_;
    };
}