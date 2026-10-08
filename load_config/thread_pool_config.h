#pragma once

#include<string>
#include<cstdint>
#include "../utils/log_manager.h"
#include<nlohmann/json.hpp>

namespace cookrpc {
    class ThreadPoolConfig {
    public:
        ThreadPoolConfig() = default;
        ~ThreadPoolConfig() = default;

        bool InitThreadPoolConfig(const nlohmann::json &config);

        uint16_t GetMaxThreads() const {
            return max_threads_;
        }

        uint32_t GetQueueSize() const {
            return queue_size_;
        }

        uint32_t GetKeepAliveTime() const {
            return keep_alive_time_;
        }

        uint16_t GetCoreThreads() const {
            return core_threads_;
        }

        void SetMaxThreads(uint16_t max_threads) {
            max_threads_ = max_threads;
        }

        void SetQueueSize(uint32_t queue_size) {
            queue_size_ = queue_size;
        }

        void SetKeepAliveTime(uint32_t keep_alive_time) {
            keep_alive_time_ = keep_alive_time;
        }

        void SetCoreThreads(uint16_t core_threads) {
            core_threads_ = core_threads;
        }
 
    private:
        uint16_t max_threads_ = 0;
        uint32_t queue_size_ = 0;
        uint32_t keep_alive_time_ = 0;
        uint16_t core_threads_ = 0;
    };
}