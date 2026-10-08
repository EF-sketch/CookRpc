#pragma once
#include "load_balancer.h"
#include<random>

namespace cookrpc {
    class RandomLoadBalancer : public LoadBalancer {
    public:
        RandomLoadBalancer() : rd_(),gen_(rd_()) {}

        std::string select(const std::vector<std::string> &instances) override {
            if (instances.empty()) {
                return "";
            }
            std::uniform_int_distribution<> dis(0, instances.size() - 1);
            return instances[dis(gen_)];
        }
    private:
        std::random_device rd_;
        std::mt19937 gen_;
    };
}