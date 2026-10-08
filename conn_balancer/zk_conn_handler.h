#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <zookeeper/zookeeper.h>
#include <zookeeper/zookeeper_version.h>
#include <zookeeper/proto.h>

#ifdef __cplusplus
}
#endif

#include <atomic>
#include <memory>
#include <string>
#include <chrono>
#include <mutex>
#include <vector>
#include <nlohmann/json.hpp>
#include "../utils/log_manager.h"
#include "../registry/service_registry.h"

class LoadBalancer;

namespace cookrpc {
    class ServiceRegistryConfig;

    class ZkConnHandler {
    public:
        static ZkConnHandler &GetInstance() {
            static ZkConnHandler instance;
            return instance;
        }

        bool initZkConnHandler(const nlohmann::json &config);

        std::vector<std::string> getAllServers(const std::string &zk_namespace);
        
        std::string getServer(const std::string &zk_namespace);

        bool registerService(const std::string &service_name, const std::string &service_address);
        bool registerServicesFromConfig(const cookrpc::ServiceRegistryConfig* registry_config);

        ServiceRegistry* getServiceRegistry() {
            return service_registry_.get();
        }

        const ServiceRegistry* getServiceRegistry() const{
            return service_registry_.get();
        }

        bool hasServiceRegistry() const{
            return service_registry_ != nullptr;
        }

        ServiceRegistry* getOrCreateServiceRegistry();

        void setZkHost(const std::string &host);
        void setZkPort(int port);
        void setZkRetryInterval(int seconds);
        void setZkNamespace(const std::string &zk_namespace);

        ~ZkConnHandler();

        void cleanup();

        ZkConnHandler(const ZkConnHandler &) = delete;
        ZkConnHandler &operator=(const ZkConnHandler &) = delete;
        ZkConnHandler (ZkConnHandler &&) = delete;
        ZkConnHandler &operator=(ZkConnHandler &&) = delete;

        void updateServersFromZk(const std::string &zk_namespace);
        bool ensureZkConnection();

    private:
        ZkConnHandler() : zk_client_(nullptr), running_(false), service_registry_(nullptr) {

        }

        static void global_watcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx);

        std::atomic<bool> running_;
        std::chrono::seconds retry_interval_;

        std::mutex mutex_;
        std::mutex servers_mutex_;
        std::vector<std::string> servers_;
        zhandle_t *zk_client_;

        std::string zk_host_;
        int zk_port_;
        std::string zk_namespace_;

        std::unique_ptr<ServiceRegistry> service_registry_;

        bool createServiceRegistryIfNeeded();

        bool initZkClient();
    };
}