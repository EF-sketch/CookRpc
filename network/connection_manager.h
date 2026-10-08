#pragma once
#include <memory>
#include <unordered_map>
#include <mutex>
#include "connection.h"

namespace cookrpc {
    class ConnectionManager {
    public:
        static ConnectionManager &GetInstance() {
            static ConnectionManager instance;
            return instance;
        }

        void AddConnection(std::shared_ptr<Connection> conn);
        void RemoveConnection(int fd);
        std::shared_ptr<Connection> GetConnection(int fd);
        size_t GetConnectionCount() const;
        void CloseAll();

        ~ConnectionManager();
        ConnectionManager(const ConnectionManager &) = delete;
        ConnectionManager &operator=(const ConnectionManager &) = delete;
        ConnectionManager(ConnectionManager &) = delete;
        ConnectionManager &operator=(ConnectionManager &&) = delete;

    private:
        ConnectionManager() = default;
        std::unordered_map<int, std::shared_ptr<Connection>> connections_;
        mutable std::mutex mutex_;
    };
}