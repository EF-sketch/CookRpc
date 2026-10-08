#include "load_balancer.h"
#include "random.h"
#include "round.h"
#include "weight.h"
#include <unordered_map>
#include <functional>

namespace cookrpc {
    std::mutex LoadBalancer::mutex_;
    std::shared_ptr<LoadBalancer> LoadBalancer::instance_ = nullptr;

    namespace {
        struct BalancerFactory {
            static std::shared_ptr<LoadBalancer> createRandom() {
                return std::make_shared<RandomLoadBalancer> ();
            }

            static std::shared_ptr<LoadBalancer> createRoundRobin() {
                return std::make_shared<RoundRobinLoadBalancer> ();
            }

            static std::shared_ptr<LoadBalancer> createWeighted() {
                return std::make_shared<WeightedRoundRobinLoadBalancer> ();
            }

            static const std::unordered_map<std::string, std::function<std::shared_ptr<LoadBalancer>()>> &getMap() {
                static const std::unordered_map<std::string, std::function<std::shared_ptr<LoadBalancer>()>>
                    map = {
                    {"random", createRandom},
                    {"round", createRoundRobin},
                    {"weight", createWeighted}
                };
                return map;
            }
        };
    }

    std::shared_ptr<LoadBalancer> LoadBalancer::getInstance() {
        if (!instance_) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!instance_) {
                instance_ = BalancerFactory::createRandom();
            }
        }
        return instance_;
    }

    bool LoadBalancer::initBalancer(const std::string &type) {
        std::lock_guard<std::mutex> lock(mutex_);

        const auto &balancer_map = BalancerFactory::getMap();
        auto it = balancer_map.find(type);

        if (it != balancer_map.end()) {
            try {
                instance_ = it->second();
                LOG_INFO("Initialized {} load balancer", type);
                return true;
            } catch (const std::exception &e) {
                LOG_ERROR("Failed to initialize {} load balancer: {}", type, e.what());
                instance_ = BalancerFactory::createRandom();
                return false;
            }
        }

        LOG_WARN("Unknow load balancer type: {}, falling back to random",type);
        instance_ = BalancerFactory::createRandom();
        return false;
    }

    std::string LoadBalancer::selectServer(const std::vector<std::string> &servers) {
        if (!instance_) {
            LOG_WARN("Load balancer not initialized, using default random balancer");
            std::lock_guard<std::mutex> lock(mutex_);
            if (!instance_) {
                instance_ = std::make_shared<RandomLoadBalancer>();
            }
        }

        if (servers.empty()) {
            LOG_ERROR("No instances available for load balancing");
            return "";
        }

        try {
            return instance_->select(servers);
        } catch (const std::exception &e){
            LOG_ERROR("Error in load balancer select: {}", e.what());
            return servers[0];
        }
    }
}