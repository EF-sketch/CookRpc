#include "connection_manager.h"
#include "../utils/log_manager.h"

namespace cookrpc
{

    void ConnectionManager::AddConnection(std::shared_ptr<Connection> conn)
    {
        if (!conn)
        {
            LOG_ERROR("add null connection");
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        int fd = conn->GetFd();

        // 检查是否已经存在
        auto it = connections_.find(fd);
        if (it != connections_.end())
        {
            LOG_WARN("connection already exists, fd: {}", fd);
            return;
        }

        connections_[fd] = conn;
    }

    void ConnectionManager::RemoveConnection(int fd)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(fd);
        if (it != connections_.end())
        {
            connections_.erase(it);
        }
        else
        {
            LOG_WARN("connection not found, fd: {}", fd);
        }
    }

    std::shared_ptr<Connection> ConnectionManager::GetConnection(int fd)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(fd);
        if (it != connections_.end())
        {
            return it->second;
        }
        return nullptr;
    }

    size_t ConnectionManager::GetConnectionCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return connections_.size();
    }

    // 关闭所有连接
    void ConnectionManager::CloseAll()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &pair : connections_)
        {
            if (auto conn = pair.second)
            {
                conn->Close();
            }
        }
        connections_.clear();
    }

    ConnectionManager::~ConnectionManager()
    {
        CloseAll();
    }

} // namespace cookrpc