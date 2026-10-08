#include "thread_pool.h"
#include <iostream>
#include <sstream>

namespace meeting_ctrl {
    ThreadPool::ThreadPool(size_t threads) : stop_(false)
                                            ,state_(ThreadPoolState::RUNNING)
                                            ,tasks_completed_(0)
                                            ,tasks_failed_(0)
                                            ,active_threads_(0)
                                            ,total_task_time_(0.0)
                                            ,current_threads_(0) 
    {
        config_.core_threads = threads;
        config_.max_threads = threads;
        config_.max_queue_size = 0;

        workers_.reserve(threads);
        thread_last_active_.resize(threads);

        for(size_t i = 0; i < threads; i++) {
            workers_.emplace_back(&ThreadPool::worker_thread, this);
            thread_last_active_[i] = std::chrono::steady_clock::now();
        }
        current_threads_ = threads;
    }

    ThreadPool::ThreadPool(const ThreadPoolStruct& config) : config_(config)
                                                            ,stop_(false)
                                                            ,state_(ThreadPoolState::RUNNING)
                                                            ,tasks_completed_(0)
                                                            ,tasks_failed_(0)
                                                            ,active_threads_(0)
                                                            ,total_task_time_(0.0)
                                                            ,current_threads_(0)
    {
        workers_.reserve(config_.max_threads);
        thread_last_active_.resize(config_.max_threads);

        for(size_t i = 0; i < config_.core_threads; ++i) {
            workers_.emplace_back(&ThreadPool::worker_thread, this);
            thread_last_active_[i] = std::chrono::steady_clock::now();
        }

        current_threads_ = config_.core_threads;
    }

    void ThreadPool::worker_thread() {
        std::thread::id thread_id = std::this_thread::get_id();
        std::ostringstream oss;
        oss << thread_id;
        std::string thread_id_str = oss.str();

        auto last_active  = std::chrono::steady_clock::now();

        while(true) {
            TaskWrapper task_wrapper{};
            {
                std::unique_lock<std::mutex> lock(queue_mutex);

                auto wait_timeout = config_.keep_alive_time;
                bool has_task = condition_.wait_for(lock, wait_timeout, [this] {
                    return stop_ || (state_ == ThreadPoolState::RUNNING && !tasks_.empty()) ||
                    (state_ == ThreadPoolState::SHUTTING_DOWN && !tasks_.empty());
                });

                if(!has_task && current_threads_ > config_.core_threads) {
                    auto now = std::chrono::steady_clock::now();
                    if (now - last_active > config_.keep_alive_time) {
                        current_threads_--;
                        LOG_INFO("[ThreadPool] Worker thread {} exiting due to timeout, remaining threads: {}", thread_id_str, current_threads_.load());
                        return;
                    }
                }

                if ((stop_ ||state_ == ThreadPoolState::STOPPED) && tasks_.empty()) {
                    LOG_INFO("[ThreadPool] Worker thread {} exiting", thread_id_str);
                    return;
                }

                if (state_ == ThreadPoolState::PAUSED) {
                    continue;
                }

                if (!tasks_.empty()) {
                    task_wrapper = std::move(const_cast<TaskWrapper&>(tasks_.top()));
                    tasks_.pop();
                    last_active = std::chrono::steady_clock::now();

                    LOG_INFO("[ThreadPool] Thread {} picked up task, queue size: {}, active threads: {}", thread_id_str, tasks_.size(), active_threads_.load() + 1);

                    if(config_.max_queue_size > 0 && tasks_.size() < config_.max_queue_size) {
                        not_full_condition_.notify_one();
                    }
                }
            }

            if (task_wrapper.valid()) {
                auto start_time = std::chrono::high_resolution_clock::now();
                active_threads_++;

                try {
                    task_wrapper.execute();
                    tasks_completed_++;

                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time-start_time);
                    double duration_ms = duration.count() / 1000.0;

                    LOG_INFO("[ThreadPool] Thread {} completed task successfully in {}ms, total completed: {}", thread_id_str, duration_ms, tasks_completed_.load());

                } catch(const std::exception& e) {
                    tasks_failed_++;
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
                    double duration_ms = duration.count() / 1000.0;

                    LOG_INFO("[ThreadPool] Thread {} task failed after {}ms, error: {}, total failed: {}", 
                        thread_id_str, duration_ms, e.what(), tasks_failed_.load());
                }

                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    total_task_time_ += duration.count() / 1000.0;
                }

                active_threads_--;

                if (state_ == ThreadPoolState::SHUTTING_DOWN && active_threads_ == 0 && tasks_.empty()) {
                    condition_.notify_all();
                }
            }

        }//while(true)

    }

    void ThreadPool::pause() {
        auto expected = ThreadPoolState::RUNNING;
        if(state_.compare_exchange_strong(expected, ThreadPoolState::RUNNING)) {
            condition_.notify_all();
        }
    }

    void ThreadPool::resume() {
        auto expected = ThreadPoolState::PAUSED;
        if(state_.compare_exchange_strong(expected, ThreadPoolState::RUNNING)) {
            condition_.notify_all();
        }
    }
    bool ThreadPool::shutdown(std::chrono::milliseconds wait_timeout_ms) {
        ThreadPoolState expected = ThreadPoolState::RUNNING;
        if(!state_.compare_exchange_strong(expected, ThreadPoolState::SHUTTING_DOWN)) {
            return state_ == ThreadPoolState::STOPPED;
        }

        condition_.notify_all();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            bool tasks_completed = condition_.wait_for(lock, wait_timeout_ms, [this] {
                return tasks_.empty() && active_threads_ == 0;
            });

            if(!tasks_completed) {
                return false;
            }

            state_ = ThreadPoolState::STOPPED;
            stop_ = true;
        }

        condition_.notify_all();

        for(std::thread &worker: workers_) {
            if(worker.joinable()) {
                worker.join();
            }
        }

        return true;
    }

    void ThreadPool::stop_now() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            state_ = ThreadPoolState::STOPPED;
            stop_ = true;

            while(!tasks_.empty()) {
                tasks_.pop();
            }
        }

        condition_.notify_all();

        for(std::thread &worker: workers_) {
            if(worker.joinable()) {
                worker.join();
            }
        }
    }

    ThreadPoolState ThreadPool::get_state() const noexcept {
        return state_;
    }

    ThreadPool::Stats ThreadPool::get_stats() const {
        Stats stats;
        stats.tasks_completed = tasks_completed_;
        stats.tasks_failed = tasks_failed_;
        stats.active_threads = active_threads_;

        std::lock_guard<std::mutex> lock(stats_mutex_);
        if(tasks_completed_ > 0) {
            stats.avg_task_time_ms = total_task_time_ / tasks_completed_;
        }

        return stats;
    }

    size_t ThreadPool::queue_size() const noexcept {
        std::unique_lock<std::mutex> lock(queue_mutex);
        return tasks_.size();
    }

    ThreadPool::~ThreadPool() {
        if(state_ != ThreadPoolState::STOPPED) {
            stop_now();
        }
    }

    void ThreadPool::adjust_thread_count() {
        if(current_threads_ < config_.max_threads && tasks_.size() > active_threads_) {
            size_t new_thread_id = current_threads_++;
            if(new_thread_id < thread_last_active_.size()) {
                thread_last_active_[new_thread_id] = std::chrono::steady_clock::now();
            }

            try {
                workers_.emplace_back(&ThreadPool::worker_thread, this);
            } catch(const std::exception& e) {
                current_threads_--;
            }
        }
    }

}