#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <future>
#include "service.h"
#include "../utils/log_manager.h"
#include "../thread_pool/thread_pool_singleton.h"

namespace cookrpc {
    class ServiceManager {
    public:
        static ServiceManager &GetInstance() {
            static ServiceManager instance;
            return instance;
        }

        bool RegisterService(std::shared_ptr<Service> service) {
            if (!service) {
                LOG_ERROR("register null service");
                return false;
            }

            std::lock_guard<std::mutex> lock(mutex_);
            std::string service_name = service->GetServiceName();

            if (services_.find(service_name) != services_.end()) {
                LOG_WARN("service already exists: {}", service_name);
                return false;
            }

            services_[service_name] = service;

            return true;
        }

        //获取服务
        std::shared_ptr<Service> GetService(const std::string &service_name) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = services_.find(service_name);
            if (it != services_.end()) {
                return it -> second;
            }
            LOG_WARN("service not found get service: {}", service_name);
            return nullptr;
        }

        //异步处理RPC请求 - 使用线程池
        std::future<bool> HandleRpcRequestAsync(const std::string &service_name, const std::string &method_name,
                                                const std::string &args, std::shared_ptr<std::string> result) {
            if (!meeting_ctrl::ThreadPoolSingleton::exists()) {
                meeting_ctrl::ThreadPoolSingleton::init(std::thread::hardware_concurrency());
            }                                        

            return meeting_ctrl::ThreadPoolSingleton::getInstance().enqueue(
                meeting_ctrl::TaskPriority::HIGH, [this, service_name, method_name, args, result]() -> bool {
                    return this->HandleRpcRequestSync(service_name, method_name, args, *result);
                }
            );
        }

        //同步处理RPC请求
        bool HandleRequest (const std::string &service_name, const std::string &method_name,
                            const std::string &args, std::string &result) {
            return HandleRpcRequestSync(service_name, method_name, args, result);
        }

        meeting_ctrl::ThreadPool::Stats GetThreadPoolStats() const {
            if (meeting_ctrl::ThreadPoolSingleton::exists()) {
                return meeting_ctrl::ThreadPoolSingleton::getInstance().get_stats();
            }
            return meeting_ctrl::ThreadPool::Stats{};
        }
    private:
        ServiceManager() = default;

        ServiceManager(const ServiceManager &) = delete;
        ServiceManager &operator=(const ServiceManager &) = delete;

        bool HandleRpcRequestSync (const std::string &service_name, const std::string &method_name,
                                    const std::string &args, std::string &result) { 
            auto service = GetService(service_name);
            if (!service) {
                LOG_ERROR("service not found handle rpc request: {}", service_name);
                return false;
            }    

            if (!service->HandleRequest(method_name, args, result)) {
                LOG_ERROR("handle reequest failed: service={}, method={}", service_name, method_name);
                return false;
            }
            return true;
        }

        //服务映射表
        std::unordered_map<std::string, std::shared_ptr<Service>> services_;
        //互斥锁
        mutable std::mutex mutex_;
    };
}