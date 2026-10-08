#pragma once
#include "load_balancer.h"
#include <atomic>
namespace cookrpc {
    class RoundRobinLoadBalancer : public LoadBalancer {
    public:
        RoundRobinLoadBalancer() : current_index_(0) {}

        std::string select(const std::vector<std::string> &instances) override {
            if (instances.empty()) {
                return "";
            }

            size_t index = current_index_.fetch_add(1) % instances.size();
            return instances[index];

        }
 
    private:
    std::atomic<size_t> current_index_;
    };
}