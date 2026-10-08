#include "service_registry.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>   // for zerror


const std::string ServiceRegistry::ROOT_PATH = "/cookrpc";

ServiceRegistry::ServiceRegistry(const std::string &zk_hosts)
    : zk_handle_(nullptr), is_connected_(false)
{
    zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);

    zk_handle_ = zookeeper_init(zk_hosts.c_str(), globalWatcher,
                                30000, 0, this, 0);
    if (!zk_handle_)
    {
        throw std::runtime_error("Failed to connect to ZooKeeper");
    }

    // 等待连接建立（简单轮询）
    int retry = 0;
    while (!is_connected_ && retry < 10)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        retry++;
    }
}

ServiceRegistry::~ServiceRegistry()
{
    if (zk_handle_)
    {
        try {
            zookeeper_close(zk_handle_);
            zk_handle_ = nullptr;
        } catch (const std::exception& e) {
            std::cerr << "Exception closing ZooKeeper handle in destructor: "
                      << e.what() << std::endl;
            zk_handle_ = nullptr;
        } catch (...) {
            std::cerr << "Unknown exception closing ZooKeeper handle in destructor"
                      << std::endl;
            zk_handle_ = nullptr;
        }
    }
}

// 全局会话事件回调
void ServiceRegistry::globalWatcher(zhandle_t *zh, int type,
                                    int state, const char *path,
                                    void *watcherCtx)
{
    if (!watcherCtx)
        return;

    ServiceRegistry *registry = static_cast<ServiceRegistry *>(watcherCtx);
    if (type == ZOO_SESSION_EVENT)
    {
        if (state == ZOO_CONNECTED_STATE)
        {
            registry->is_connected_ = true;
            std::cout << "Connected to ZooKeeper" << std::endl;
        }
        else
        {
            registry->is_connected_ = false;
            std::cout << "Disconnected from ZooKeeper" << std::endl;
        }
    }
}

// 普通字符串操作回调（未使用，但保留）
void string_completion_cb(int rc, const char *value, const void *data)
{
    if (rc == ZOK)
    {
        std::cout << "Operation completed successfully" << std::endl;
    }
    else
    {
        std::cerr << "Operation failed: " << zerror(rc) << std::endl;
    }
}

bool ServiceRegistry::createNode(const std::string &path,
                                 const std::string &data,
                                 int flags)
{
    if (!zk_handle_)
        return false;

    int ret = zoo_create(zk_handle_, path.c_str(), data.c_str(),
                         data.length(), &ZOO_OPEN_ACL_UNSAFE,
                         flags, NULL, 0);

    if (ret == ZOK)
    {
        return true;
    }
    else if (ret == ZNODEEXISTS)
    {
        LOG_WARN("Service instance node already exists: {}", path);
        return true;   // 节点已存在视为成功
    }
    else
    {
        LOG_ERROR("Failed to create service instance node: {}, error: {}", path, zerror(ret));
        return false;
    }
}

bool ServiceRegistry::ensurePath(const std::string &path)
{
    if (path.empty())
    {
        LOG_ERROR("Path is empty");
        return false;   // 空路径应视为失败
    }

    if (!zk_handle_)
    {
        LOG_ERROR("ZooKeeper handle is null");
        return false;
    }

    // 递归创建父路径
    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos && pos > 0)
    {
        std::string parent = path.substr(0, pos);
        if (!ensurePath(parent))
        {
            LOG_ERROR("Failed to create parent path: {}", parent);
            return false;
        }
    }

    struct Stat stat;
    int ret = zoo_exists(zk_handle_, path.c_str(), 0, &stat);

    if (ret == ZOK)
    {
        return true;
    }
    else if (ret == ZNONODE)
    {
        ret = zoo_create(zk_handle_, path.c_str(), "", 0, &ZOO_OPEN_ACL_UNSAFE,
                         0, NULL, 0);   // flags=0 表示持久节点
        if (ret == ZOK)
        {
            return true;
        }
        else if (ret == ZNODEEXISTS)
        {
            return true;   // 其他进程已创建
        }
        else
        {
            LOG_ERROR("Failed to create persistent path: {}, error: {}", path, zerror(ret));
            return false;
        }
    }
    else
    {
        LOG_ERROR("Failed to check persistent path: {}, error: {}", path, zerror(ret));
        return false;
    }
}

bool ServiceRegistry::registryService(const std::string &service_name,
                                      const std::string &service_address)
{
    if (!isConnected() || !zk_handle_)
    {
        LOG_ERROR("Not connected to ZooKeeper");
        return false;
    }

    std::string service_path = ROOT_PATH + "/" + service_name;
    if (!ensurePath(service_path))
    {
        LOG_ERROR("Failed to ensure service type path: {}", service_path);
        return false;
    }

    std::string instance_path = service_path + "/" + service_address;
    return createNode(instance_path, service_address, ZOO_EPHEMERAL);
}

// 用于获取子节点列表的回调（示例，未使用）
void strings_completion_cb(int rc, const struct String_vector *strings, const void *data)
{
    if (rc == ZOK && strings)
    {
        // LOG_INFO("Found {} services", strings->count);
    }
}

bool ServiceRegistry::isConnected() const
{
    return is_connected_ && zk_handle_ != nullptr;
}

// ---------- main 函数 ----------
// int main() {
//     // 初始化日志系统（日志目录可自定义）
//     if (!Logger::GetInstance().Init("../logs")) {
//         std::cerr << "Failed to initialize logger" << std::endl;
//         return 1;
//     }

//     try {
//         ServiceRegistry registry("127.0.0.1:2181");

//         if (!registry.isConnected()) {
//             LOG_ERROR("Failed to connect to ZooKeeper");
//             return 1;
//         }

//         std::string service_name = "UserService";
//         std::string service_address = "192.168.1.100:8001";

//         if (registry.registryService(service_name, service_address)) {
//             LOG_INFO("Service registered successfully");
//         } else {
//             LOG_ERROR("Failed to register service");
//             return 1;
//         }

//         LOG_INFO("Service running, press Ctrl+C to exit...");
//         while (true) {
//             std::this_thread::sleep_for(std::chrono::seconds(10));

//             if (!registry.isConnected()) {
//                 LOG_ERROR("Lost connection to ZooKeeper!");
//                 break;
//             }
//         }
//     } catch (const std::exception &e) {
//         LOG_ERROR("Exception: {}", e.what());
//         return 1;
//     }

//     return 0;
// }