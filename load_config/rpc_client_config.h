#pragma once

#include <string>
#include <fstream>
#include <cstdint>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "../utils/log_manager.h"
#include "thread_pool_config.h"
#include "registry_config.h"

namespace cookrpc
{

    class RpcClientConfig
    {
    public:
        static RpcClientConfig &GetInstance()
        {
            static RpcClientConfig instance;
            return instance;
        }

        bool InitRpcClientConfig(const std::string &config_filename);

        RpcClientConfig(const RpcClientConfig &) = delete;
        RpcClientConfig &operator=(const RpcClientConfig &) = delete;

        RpcClientConfig(RpcClientConfig &&) = delete;
        RpcClientConfig &operator=(RpcClientConfig &&) = delete;

        void SetTimeout(int timeout_ms);
        void SetRetryTimes(int retry_times);
        void SetZkNamespace(const std::string &zk_namespace);
        void SetServerPort(int server_port);
        void SetZkHost(const std::string &zk_host);
        void SetZkPort(int zk_port);

        int GetTimeout() const { return timeout_ms_; }
        int GetRetryTimes() const { return retry_times_; }
        const std::string &GetZkNamespace() const { return zk_namespace_; }
        int GetServerPort() const { return server_port_; }
        const std::string &GetZkHost() const { return zk_host_; }
        int GetZkPort() const { return zk_port_; }

    private:
        std::string zk_namespace_;
        std::string zk_host_;
        int zk_port_;
        int timeout_ms_ = 3000;
        int retry_times_ = 3;
        int server_port_ = 0;

        RpcClientConfig() = default;
    };

}