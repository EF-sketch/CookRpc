#pragma once
#include<string>
#include<vector>
#include<memory>
#include<mutex>
#include<unordered_map>
#include<functional>
#include "../utils/log_manager.h"

namespace cookrpc {
    class RandomLoadBalancer;
    class RoundRobinLoadBalancer;
    class WeightedRoundRobinLoadBalancer;

    class LoadBalancer{
    public:
        virtual ~LoadBalancer() = default;

        virtual std::string select(const std::vector<std::string> &instances) = 0;

        static std::shared_ptr<LoadBalancer> getInstance();
        static bool initBalancer(const std::string &type = "random");

        static std::string selectServer(const std::vector<std::string> &servers);

    protected:
        LoadBalancer() noexcept = default;

    private:
        static std::shared_ptr<LoadBalancer> instance_;
        static std::mutex mutex_;

        LoadBalancer(const LoadBalancer &) = delete;
        LoadBalancer &operator=(const LoadBalancer &) = delete;
        
    };

    std::shared_ptr<LoadBalancer> createRandomBalancer();
    std::shared_ptr<LoadBalancer> createRoundRobinBalancer();
    std::shared_ptr<LoadBalancer> createWeightedBalancer();
}