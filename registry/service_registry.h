#pragma once

#include <string>
#include <memory>
#include <zookeeper/zookeeper.h>
#include "../utils/log_manager.h"   // 使用 spdlog 日志

class ServiceRegistry {
public:
    explicit ServiceRegistry(const std::string &zk_hosts);
    ~ServiceRegistry();

    // 注意：函数名与 cpp 实现一致，均为 registryService
    bool registryService(const std::string &service_name, const std::string &service_address);

    bool isConnected() const;

private:
    static void globalWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx);

    bool createNode(const std::string &path, const std::string &data, int flags = 0);
    bool ensurePath(const std::string &path);

    zhandle_t *zk_handle_;
    bool is_connected_;
    static const std::string ROOT_PATH;   
};