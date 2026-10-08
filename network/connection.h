#pragma once
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include "../protocol/rpc_protocol.h"
#include "../utils/log_manager.h"

namespace cookrpc
{

    // 前向声明
    class RpcMessage;
    class RpcRequest;
    class RpcResponse;

    class Connection : public std::enable_shared_from_this<Connection>
    {
    public:
        enum class State
        {
            CONNECTED,
            DISCONNECTING,
            DISCONNECTED
        };

        // 定义消息回调函数类型
        using MessageCallback = std::function<void(const std::shared_ptr<Connection> &, const RpcRequest &)>;
        using CloseCallback = std::function<void(const std::shared_ptr<Connection> &)>;

        explicit Connection(int fd);
        ~Connection();

        // 基础 IO 操作
        bool Read();
        bool Write(const std::string &data);
        bool Write(const RpcResponse &response);
        void Close();
        bool IsValid() const { return fd_ > 0 && state_ == State::CONNECTED; }
        bool ReadWithTimeout(int timeout_ms);

        // 获取连接信息
        int GetFd() const { return fd_; }
        const std::string &GetPeerAddress() const { return peer_addr_; }
        uint16_t GetPeerPort() const { return peer_port_; }
        State GetState() const { return state_; }

        // 获取读缓冲区数据（客户端使用）
        std::string GetReadBufferData() 
        {
            std::string data(read_buffer_.begin(), read_buffer_.end());
            read_buffer_.clear(); // 清空缓冲区
            return data;
        }

        // 设置回调
        void SetMessageCallback(const MessageCallback &cb) { message_callback_ = cb; }
        void SetCloseCallback(const CloseCallback &cb) { close_callback_ = cb; }

        static const size_t MAX_MESSAGE_SIZE = 1024 * 1024; // 1MB
        bool ProcessMessage();

    private:
        void HandleMessage(const RpcRequest &request);
        void HandleError();
        bool SendInBuffer();
        int fd_ = -1;
        std::string peer_addr_ = "";
        uint16_t peer_port_ = 0;

        std::vector<char> read_buffer_;
        std::vector<char> write_buffer_;

        MessageCallback message_callback_ = nullptr;
        CloseCallback close_callback_ = nullptr;

        static const size_t MAX_BUFFER_SIZE = 65536;

        State state_ = State::DISCONNECTED;
        std::atomic<bool> is_writing_ = false;
    };

} // namespace cookrpc