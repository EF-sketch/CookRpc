#include "zk_conn_handler.h"
#include "load_balancer.h"
#include "../load_config/registry_config.h"
#include "../registry/service_registry.h"
#include <stdexcept>
#include <iostream>
#include <zookeeper/zookeeper.h>
#include <nlohmann/json.hpp>
#include <thread>


namespace cookrpc {
    void ZkConnHandler::setZkNamespace(const std::string &zk_namespace) {
        zk_namespace_ = zk_namespace;
    }

    void ZkConnHandler::setZkRetryInterval(int seconds) {
        retry_interval_ = std::chrono::seconds(seconds);
    }

    void ZkConnHandler::setZkHost(const std::string &host) {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (host.empty()) {
                LOG_WARN("Empty host provided, using default");
                zk_host_ = "localhost";
            } else {
                zk_host_ = host;
            }
        } catch (const std::exception &e){
            LOG_ERROR("Failed to set ZooKeeper host:{}", e.what());
            zk_host_ = "localhost";
        }
    }

    void ZkConnHandler::setZkPort(int port) {
        std::lock_guard<std::mutex> lock(mutex_);
        zk_port_ = (port > 0 && port < 65536) ? port : 2181;
    }

    std::vector<std::string> ZkConnHandler::getAllServers(const std::string &zk_namespace) {
        if(!ensureZkConnection()) {
            LOG_ERROR("ZooKeeper connection not available");
            return {};
        }

        std::lock_guard<std::mutex> lock(servers_mutex_);

        try {
            struct String_vector children = {0};

            int rc = zoo_get_children(zk_client_, zk_namespace.c_str(), 0, &children);
            if (rc != ZOK){
                LOG_ERROR("Failed toget chilren: {}", zerror(rc));
                return {};
            }

            std::vector<std::string> servers;
            for(int i = 0; i < children.count; i++) {
                std::string node_path = zk_namespace + "/" + children.data[i];
                char buffer[1024];
                int buffer_len = sizeof(buffer);

                int rc = zoo_get(zk_client_, node_path.c_str(), 0, buffer, &buffer_len, nullptr);
                if (rc == ZOK && buffer_len > 0) {
                    std::string server_ip(buffer, buffer_len);
                    servers.push_back(server_ip);
                } else {
                    LOG_WARN("Failed to get data for node: {}, error: {}", node_path, zerror(rc));

                }

                deallocate_String_vector(&children);

                return servers;
            } 

        } catch (const std::exception &e) {
            LOG_ERROR("Exception in getAllServers: {}", e.what());
            return {};
        }
    }

    bool ZkConnHandler::initZkConnHandler (const nlohmann::json &config) {
        try {
            if (config.empty()) {
                LOG_ERROR("Empty configuration");
                return false;
            }

            try {
                std::string zk_host = config.value("zk_host", "localhost");
                setZkHost(zk_host);

                int zk_port = config.value("zk_port", 2181);
                setZkPort(zk_port);

                setZkNamespace (config.value("zk_namespace", "/cookrpc"));
                setZkRetryInterval (config.value("zk_retry_interval", 5));
            } catch (const nlohmann::json::exception &e) {
                LOG_ERROR("Failed to parse configuration: {}", e.what());
                cleanup();
                return false;
            }

            bool is_connected = ensureZkConnection();

            if (!zk_client_) {
                LOG_ERROR("Failed to iniitialize ZooKeeper client: {}", strerror(errno));
                return false;
            }

            if (!is_connected) {
                LOG_ERROR("Failed to connect to ZooKeeper after retries");
                return false;
            }

            return true;
        } catch (const std::exception &e) {
            LOG_ERROR("exception in initZkConnHandler: {}", e.what());
            cleanup();
            return false;
        }
    }

    bool ZkConnHandler::ensureZkConnection() {
        if (!zk_client_) {
            std::string conn_string = zk_host_ + ":" + std::to_string(zk_port_);

            zk_client_ = zookeeper_init(conn_string.c_str(), global_watcher, 3000, nullptr, this, 0);
            if (!zk_client_) {
                LOG_ERROR("Failed to initialize ZooKeeper client: {} (errno: {})", strerror(errno), errno);
                return false;
            }
        }

        int retry_count = 0;
        const int max_retries = 3;

        while (retry_count++ < max_retries) {
            int state = zoo_state(zk_client_);
            const char *state_desc;

            if (state == ZOO_CONNECTED_STATE){
                state_desc = "CONNECTED";
            } else if (state == ZOO_CONNECTING_STATE) {
                state_desc = "CONNECTING";
            } else if (state == ZOO_EXPIRED_SESSION_STATE) {
                state_desc = "EXPIRED";
            } else if (state == ZOO_AUTH_FAILED_STATE) {
                state_desc = "AUTH_FAILED";
            } else if (state == ZOO_ASSOCIATING_STATE) {
                state_desc = "ASSOCIATING";
            } else {
                state_desc = "UNKNOWN";
                LOG_WARN("Unexpected state value: {} (0x{:x})", state, state);
                LOG_WARN("Client handle valid: {}", (zk_client_ != nullptr));

                if (zk_client_) {
                    struct Stat stat = {0};
                    int rc = zoo_exists(zk_client_, "/", 0, &stat);
                    LOG_WARN("zoo_exists test result: {}", rc);
                }
            }

            if (state == ZOO_CONNECTED_STATE) {
                return true;
            } else if (state ==ZOO_EXPIRED_SESSION_STATE || state == ZOO_AUTH_FAILED_STATE || (state != ZOO_CONNECTING_STATE && retry_count > 10)) {
                LOG_ERROR("Connection failed wih state: {} ({})", state, state_desc);
                cleanup();
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        LOG_ERROR("Failed to connect after {} retries", max_retries);
        cleanup();
        return false;
    }

    std::string ZkConnHandler::getServer (const std::string &zk_namespace) {
        updateServersFromZk (zk_namespace);
        std::lock_guard<std::mutex> lock(servers_mutex_);

        if (servers_.empty()) {
            LOG_ERROR("No available servers");
            return "";
        }

        std::string server = LoadBalancer::selectServer(servers_);
        return server;
    }

    void ZkConnHandler::cleanup() {
        static std::atomic<bool> cleanup_done{false};
        if (cleanup_done.exchange(true)) {
            return;
        } 

        service_registry_.reset();

        if (zk_client_) {
            try {
                zookeeper_close(zk_client_);
                zk_client_ = nullptr;
            } catch (const std::exception& e) {
                zk_client_ = nullptr;
            } catch (...) {
                zk_client_ = nullptr;
            }
        }

        running_ = false;
    }

    void ZkConnHandler::updateServersFromZk (const std::string &zk_namespace) {
        auto new_servers = getAllServers(zk_namespace);
        {
            std::lock_guard<std::mutex> lock(servers_mutex_);
            servers_ = std::move(new_servers);
        }
    }

    void ZkConnHandler::global_watcher (zhandle_t *zh, int type, int state, const char *path, void *watcherCtx) {
        if (type == ZOO_SESSION_EVENT) {
            const char *state_desc;

            if (state == ZOO_CONNECTED_STATE) {
                state_desc = "CONNECTED";
            } else if (state ==ZOO_CONNECTING_STATE) {
                state_desc = "CONNECTING";
            } else if (state == ZOO_EXPIRED_SESSION_STATE) {
                state_desc = "EXPIRED";
            } else if (state == ZOO_AUTH_FAILED_STATE) {
                state_desc = "AUTH_FAILED";
            } else if (state ==ZOO_ASSOCIATING_STATE) {
                state_desc = "ASSOCIATING";
            } else {
                state_desc = "UNKNOW";
            }
        }
    }

    ZkConnHandler::~ZkConnHandler() {
        cleanup();
    }

    ServiceRegistry* ZkConnHandler::getOrCreateServiceRegistry() {
        if (!service_registry_) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!service_registry_) {
                if(!createServiceRegistryIfNeeded()) {
                    return nullptr;
                }
            }
        }

        return service_registry_.get();
    }

    bool ZkConnHandler::registerService (const std::string &service_name, const std::string &service_address) {
        try {
            ServiceRegistry* registry = getOrCreateServiceRegistry();
            if (!registry) {
                LOG_ERROR("Failed to get or create ServiceRegistry for service registration");
                return false;
            }

            if (registry->registryService(service_name, service_address)) {
                return true;
            } else {
                LOG_ERROR("Failed to register service: {} at {}", service_name, service_address);
                return false;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to register service: {}", e.what());
            return false;
        }
    }

    bool ZkConnHandler::registerServicesFromConfig (const cookrpc::ServiceRegistryConfig* registry_config) {
        if (!registry_config || registry_config->GetRegistryNodesSize() == 0) {
            LOG_WARN("No registry configuration found, skipping ZooKeeper registration");
            return true;
        }

        try {
            ServiceRegistry* registry = getOrCreateServiceRegistry();
            if (!registry) {
                LOG_ERROR("Failed to get or create ServiceRegistry for batch service registry");
                return false;
            }

            const auto& registry_nodes = registry_config->GetRegistryNodes();
            const std::string& service_name = registry_config->GetServiceName();

            bool all_success = true;
            for(const auto& node : registry_nodes) {
                std::string service_address = node.address + ":" + std::to_string(node.port);
                if (registry->registryService(service_name, service_address)) {
                    LOG_INFO("Successfully registered service: {} at {}", service_name, service_address);
                } else {
                    all_success = false;
                }
            }

            return all_success;
        } catch (const std::exception& e) {
            LOG_ERROR("Exception during service registration: {}", e.what());
            return false;
        }
    }

    bool ZkConnHandler::createServiceRegistryIfNeeded() {
        try {
            std::string zk_connection = zk_host_ + ":" + std::to_string(zk_port_);

            service_registry_ = std::make_unique<ServiceRegistry>(zk_connection);

            std::this_thread::sleep_for(std::chrono::seconds(1));

            if (!service_registry_->isConnected()) {
                LOG_ERROR("Failed to connect ServiceRegistry to ZooKeeper at {}", zk_connection);
                service_registry_.reset();
                return false;
            }

            return false;
        } catch (const std::exception& e) {
            LOG_ERROR("Exception creating ServiceRegistry: {}", e.what());
            service_registry_.reset();
            return false;
        }
    }


}