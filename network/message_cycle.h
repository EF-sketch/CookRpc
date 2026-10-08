#pragma once
#include <set>
#include <atomic>
#include <pthread.h>
#include <unordered_map>
#include <future>
#include "../network/connection_manager.h"
#include "../protocol/rpc_protocol.h"
#include "../utils/log_manager.h"
#include "../service/service_manager.h"
#include "../core/error_code.h"
#include "../thread_pool/thread_pool_singleton.h"
#include <netinet/tcp.h>

#ifdef __APPLE__
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#else
#include <sys/epoll.h>
#endif

namespace cookrpc {
    class ConnectionManager; 
    class Connection;

    class MessageCycle {
    public:
        MessageCycle(ConnectionManager *manager);
        virtual ~MessageCycle();
        bool AddListenFd(int fd);
        bool RemoveListenFd(int fd);
        size_t GetListenFdCount() const;
        void Loop();
        void Stop();
        void HandleRpcRequest(const std::shared_ptr<Connection> &conn, const RpcRequest &request);
        void HandleRpcRequestAsync(const std::shared_ptr<Connection> &conn, const RpcRequest &request);

    private:
            pthread_t thread_id_;
            std::set<int> listen_fds_;
            std::atomic<bool> running_;
            mutable std::mutex mutex_;

#ifdef __APPLE__
            int kqueue_fd_;
#else
            int epoll_fd_;
#endif

#ifdef __APPLE__
            void HandleKqueueEvents(struct kevent *events, int nfds);
#else
            void HandleEpollEvents(struct epoll_event *events, int nfds);
#endif
            void HandleNewConnection(int fd);
            void HandleClientData(int fd);
            void RemoveConnection(int fd);

            ConnectionManager *connection_manager_;
            static const int MAX_EVENTS = 1024;
            void RemoveInvalidFileDescriptor(int fd);
            bool IsValidFileDescriptor(int fd);
            bool ValidateRequest(const RpcRequest &request);
            //发送成功响应
            void SendSuccessResponse(const std::shared_ptr<Connection> &conn,
                                    uint64_t sequence_id, 
                                    const std::string &result);
            //发送错误响应
            void SendErrorResponse(const std::shared_ptr<Connection> &conn,
                                  uint64_t sequence_id,
                                  cookrpc::ErrorCode error_code,
                                  const std::string &error_message);
            //通用响应发送方法（内部使用）
            void SendResponse(const std::shared_ptr<Connection> &conn, 
                             const RpcResponse &response,
                             const std::string &response_type);
            //内部同步处理RPC请求方法
            void HandleRpcRequestSync(const std::shared_ptr<Connection> &conn,
                                     const RpcRequest &request);
    };

}