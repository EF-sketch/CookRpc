#pragma once
#include "load_balancer.h"
#include <map>

namespace cookrpc {

    class WeightedRoundRobinLoadBalancer : public LoadBalancer {
    public:
        WeightedRoundRobinLoadBalancer() : current_index_(0) {}

        std::string select(const std::vector<std::string> &instances) override {
            if (instances.empty()) {
                return "";
            }

            std::vector<int> weights(instances.size(), 1);

            int total_weight = 0;
            for (int weight : weights) {
                total_weight += weight;
            }

            current_index_ = (current_index_ + 1) % total_weight;

            int current_weight = 0;
            for (size_t i = 0; i < instances.size(); i++) {
                current_weight += weights[i];
                if (current_index_ < current_weight) {
                    return instances[i];
                }
            }
            return instances[0];
        }

    private:
        std::atomic<int> current_index_;

        void setWeight(const std::string &instance, int weight) {
            weights_[instance] = weight;
        }
        std::map<std::string, int> weights_;
    };
}