#pragma once

#include <memory>
#include <string>
#include <filesystem>
#include <iostream>
#include <thread>
#include <sstream>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/daily_file_sink.h"

#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define LOG_FATAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

#define CLIENT_INFO(...) SPDLOG_INFO("[CLIENT] " __VA_ARGS__)
#define CLIENT_DEBUG(...) SPDLOG_DEBUG("[CLIENT] " __VA_ARGS__)
#define CLIENT_WARN(...) SPDLOG_WARN("[CLIENT] " __VA_ARGS__)
#define CLIENT_ERROR(...) SPDLOG_ERROR("[CLIENT] " __VA_ARGS__)
#define CLIENT_FATAL(...) SPDLOG_CRITICAL("[CLIENT] " __VA_ARGS__)

// formatter 特化（必须在全局命名空间）
template <>
struct fmt::formatter<std::thread::id> : fmt::formatter<std::string> {
    template <typename FormatContext>
    auto format(const std::thread::id &id, FormatContext &ctx) const {
        std::ostringstream oss;
        oss << id;
        return fmt::formatter<std::string>::format(oss.str(), ctx);
    }
};

class Logger {
public:
    static Logger &GetInstance() {
        static Logger instance;
        return instance;
    }

    bool Init(const std::string &log_dir = "../logs") {
        try {
            std::filesystem::path log_path(log_dir);
            if (!std::filesystem::exists(log_path)) {
                if (!std::filesystem::create_directories(log_path)) {
                    std::cerr << "Failed to create log directory: " << log_dir << std::endl;
                    return false;
                }
            }

            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            if (!console_sink) {
                std::cerr << "Failed to create console sink" << std::endl;
                return false;
            }

            std::string log_file = log_dir + "/cookrpc.log";
            auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_file, 0, 0);
            if (!file_sink) {
                std::cerr << "Failed to create file sink: " << log_file << std::endl;
                return false;
            }

            std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
            logger_ = std::make_shared<spdlog::logger>("main", sinks.begin(), sinks.end());
            if (!logger_) {
                std::cerr << "Failed to create logger" << std::endl;
                return false;
            }

            logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
            logger_->set_level(spdlog::level::trace);
            spdlog::set_default_logger(logger_);

            // 可选：立即写一条测试日志
            LOG_INFO("Logger initialized successfully, log dir: {}", log_dir);
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Logger initialization failed: " << e.what() << std::endl;
            return false;
        }
    }

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

private:
    Logger() = default;
    ~Logger() = default;
    std::shared_ptr<spdlog::logger> logger_;
};