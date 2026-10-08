#pragma once

#include<vector>
#include<queue>
#include<memory>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<future>
#include<functional>
#include<stdexcept>
#include<atomic>
#include<chrono>
#include "../utils/log_manager.h"

namespace meeting_ctrl {
    enum class TaskPriority {
        LOW,
        NORMAL,
        HIGH
    };

    enum class ThreadPoolState {
        RUNNING,
        PAUSED,
        SHUTTING_DOWN,
        STOPPED
    };

    struct ThreadPoolStruct {
        size_t core_threads = std::thread::hardware_concurrency();
        size_t max_threads = std::thread::hardware_concurrency() * 2;
        size_t max_queue_size = 1000;
        std::chrono::milliseconds keep_alive_time{60000};
    };

    class TaskWrapper {
    public:
        TaskWrapper(std::function<void()>&& t, TaskPriority p) : task_(std::move(t)), priority_(p) {}

        TaskWrapper() : priority_(TaskPriority::NORMAL) {}

        void execute() {
            if (valid()) {
                task_();
            }
        }

        bool valid() const {
            return static_cast<bool>(task_);
        }

        bool operator<(const TaskWrapper& other) const {
            return priority_< other.priority_;
        }

    private:
        std::function<void()> task_;
        TaskPriority priority_; 
    };

    class ThreadPool {
    public:
        struct Stats {
            size_t tasks_completed = 0;
            size_t tasks_failed = 0;
            double avg_task_time_ms = 0;
            size_t active_threads = 0;
        };

        explicit ThreadPool (size_t threads = std::thread::hardware_concurrency());

        explicit ThreadPool (const ThreadPoolStruct& config);

        template<class F, class... Args>
        auto enqueue(TaskPriority priority, F&& f, Args&&...args) -> std::future<typename std::invoke_result<F, Args...>::type>;

        void pause();
        void resume();

        Stats get_stats() const;

        size_t size() const noexcept {
            return workers_.size();
        }

        size_t queue_size() const noexcept;

        bool shutdown(std::chrono::milliseconds wait_timeout_ms = std::chrono::milliseconds::max());

        void stop_now();

        ThreadPoolState get_state() const noexcept;
        ~ThreadPool();

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

    private:
        void worker_thread();
        void adjust_thread_count();

        std::vector<std::thread> workers_;
        std::atomic<size_t> current_threads_{0};
        std::vector<std::chrono::steady_clock::time_point> thread_last_active_;

        std::priority_queue<TaskWrapper> tasks_;

        ThreadPoolStruct config_;

        mutable std::mutex queue_mutex;
        std::condition_variable condition_;
        std::condition_variable not_full_condition_;

        bool stop_{false};

        std::atomic<ThreadPoolState> state_{ThreadPoolState::RUNNING};

        mutable std::mutex stats_mutex_;
        std::atomic<size_t> tasks_completed_{0};
        std::atomic<size_t> tasks_failed_{0};
        std::atomic<size_t> active_threads_{0};
        double total_task_time_{0};
    };

    template<class F, class... Args>
    auto ThreadPool::enqueue(TaskPriority priority, F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type>
    {
        using return_type = typename std::invoke_result<F, Args...>::type;

        // 1. 创建任务包装器
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        // 2. 创建与任务关联的 future
        std::future<return_type> res = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            // 3. 检查线程池状态
            if(stop_) {
                return std::future<return_type>{}; // 返回空 future
            }

            // 4. 背压控制: 队列满时等待
            if(config_.max_queue_size > 0) {
                not_full_condition_.wait(lock, [this] {
                    return stop_ || tasks_.size() < config_.max_queue_size;
                });

                if(stop_) {
                    return std::future<return_type>{};
                }
            }

            // 5. 将任务添加到优先级队列
            tasks_.emplace(
                [task]() { (*task)(); },  // Lambda 捕获 shared_ptr
                priority
            );
        }

        // 6. 记录任务提交信息
        std::string priority_str = (priority == TaskPriority::HIGH) ? "HIGH" :
                                (priority == TaskPriority::NORMAL) ? "NORMAL" : "LOW";
        LOG_INFO("[ThreadPool] Enqueuing task with priority: {}, current queue size: {}/{}, active threads: {}, current threads: {}",
            priority_str, tasks_.size(), config_.max_queue_size,
            active_threads_.load(), current_threads_.load());

        // 7. 动态线程创建: 任务数超过活跃线程数且未达到最大线程数
        if(tasks_.size() > active_threads_ && current_threads_ < config_.max_threads) {
            adjust_thread_count();
        }

        // 8. 唤醒一个等待任务的工作线程
        condition_.notify_one();
        return res;
    }

}

