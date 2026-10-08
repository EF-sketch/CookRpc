#include "connection.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "../serializer/serializer_manager.h"
#include "../encrypt/aes_encrypt.h"
#include "../compress_data/zstd_compress.h"

namespace cookrpc

{
    const size_t Connection::MAX_MESSAGE_SIZE;

    Connection::Connection(int fd) : fd_(fd)
    {
        // 先检查 fd 的有效性
        if (fd_ < 0)
        {
            LOG_ERROR("connection fd is invalid: {}", fd_);
            state_ = State::DISCONNECTED;
            return;
        }

        // 检查是否是 socket
        int type;
        socklen_t type_len = sizeof(type);
        if (getsockopt(fd_, SOL_SOCKET, SO_TYPE, &type, &type_len) < 0)
        {
            LOG_ERROR("Not a socket fd: {}, errno: {}", fd_, errno);
            state_ = State::DISCONNECTED;
            return;
        }

        // 获取对端地址信息
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        memset(&addr, 0, sizeof(addr));

        if (getpeername(fd_, (struct sockaddr *)&addr, &len) == 0)
        {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
            peer_addr_ = ip;
            peer_port_ = ntohs(addr.sin_port);
            state_ = State::CONNECTED;
            // // LOG_INFO("Connection established with {}:{}, fd: {}", peer_addr_, peer_port_, fd_);
        }
        else
        {
            LOG_ERROR("getpeername failed, fd: {}, errno: {} ({})",
                      fd_, errno, strerror(errno));
            state_ = State::DISCONNECTED;
        }
    }

    Connection::~Connection()
    {
        try {
            Close();
        } catch (const std::exception& e) {
            // 在析构函数中不应该抛出异常，只记录错误
            // 注意：这里不能使用LOG_ERROR，因为Logger可能已经被销毁
            std::cerr << "Exception in Connection destructor: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown exception in Connection destructor" << std::endl;
        }
    }

    bool Connection::ReadWithTimeout(int timeout_ms)
    {
        while (true)
        {
            fd_set read_set;
            struct timeval timeout;
            timeout.tv_sec = timeout_ms / 1000;
            timeout.tv_usec = (timeout_ms % 1000) * 1000;

            // 每次循环都需要重新设置 fd_set
            FD_ZERO(&read_set);
            FD_SET(fd_, &read_set);

            int ret = select(fd_ + 1, &read_set, nullptr, nullptr, &timeout);
            if (ret < 0)
            {
                if (errno == EINTR)
                {
                    // 如果是被信号中断，则继续
                    continue;
                }
                LOG_ERROR("select error, fd: {}, errno: {}", fd_, errno);
                return false;
            }

            if (ret == 0)
            {
                // 超时
                LOG_ERROR("read timeout after {} ms, fd: {}", timeout_ms, fd_);
                return false;
            }

            // 有数据可读，调用已有的 Read 函数
            if (!Read())
            {
                LOG_ERROR("read failed after select, fd: {}", fd_);
                return false;
            }

            return true;
        }
    }

    bool Connection::Read()
    {
        // 检查连接状态
        if (state_ != State::CONNECTED)
        {
            LOG_ERROR("Connection is not in CONNECTED state, fd: {}, state: {}", 
                     fd_, static_cast<int>(state_));
            return false;
        }

        char buf[4096];
        bool has_read_data = false;
        
        // 在边缘触发模式下，需要循环读取直到EAGAIN
        while (true)
        {
            ssize_t n = read(fd_, buf, sizeof(buf));
            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // 在边缘触发模式下，这表示所有数据都已读取完毕
                    LOG_DEBUG("No more data available now, fd: {} (read {} bytes total)", 
                             fd_, has_read_data ? "some" : "no");
                    return has_read_data || !has_read_data; // 即使没读到数据也不算错误
                }
                LOG_ERROR("read error, fd: {}, errno: {}, error: {}", fd_, errno, strerror(errno));
                state_ = State::DISCONNECTED;
                return false;
            }

            if (n == 0)
            {
                // // LOG_INFO("connection closed by peer, fd: {}", fd_);
                state_ = State::DISCONNECTED;
                return false; // 连接关闭
            }

            has_read_data = true;
            LOG_DEBUG("Read {} bytes from fd: {}", n, fd_);

            // 检查缓冲区大小
            if (read_buffer_.size() + n > MAX_BUFFER_SIZE)
            {
                LOG_ERROR("read buffer overflow, fd: {}, current size: {}, trying to add: {}", 
                         fd_, read_buffer_.size(), n);
                state_ = State::DISCONNECTED;
                return false;
            }

            read_buffer_.insert(read_buffer_.end(), buf, buf + n);
            
            // 如果读取的数据小于缓冲区大小，可能已经读完了
            if (n < sizeof(buf))
            {
                break;
            }
        }
        return true;
    }

    bool Connection::ProcessMessage()
    {

        if (read_buffer_.size() >= sizeof(RpcHeader))
        {
            try
            {
                // vector<char> 转换为 string
                std::string encrypted_data(read_buffer_.begin(), read_buffer_.end());

                // // LOG_INFO("ProcessMessage: encrypted_data size: {}, fd: {}", 
                        // encrypted_data.size(), fd_);

                // 解密数据
                std::string decrypted_data;

                if (encrypted_data.empty())
                {
                    LOG_ERROR("Encrypted data is empty, fd: {}", fd_);
                    return false;
                }

                if (!AesEncrypt::getInstance().Decrypt(encrypted_data, decrypted_data))
                {
                    LOG_ERROR("Failed to decrypt data, encrypted size: {}, fd: {}",
                              encrypted_data.size(), fd_);
                    return false;
                }

                // // LOG_INFO("ProcessMessage: decrypted_data size: {}, fd: {}", 
                //         decrypted_data.size(), fd_);

                // 检查解密后的数据
                if (decrypted_data.empty())
                {
                    LOG_ERROR("Decrypted data is empty, fd: {}", fd_);
                    return false;
                }

                std::string decompressed_data;
                if (!ZstdCompress::getInstance().DecompressString(decrypted_data, decompressed_data))
                {
                    LOG_ERROR("Failed to decompress data, decrypted size: {}, fd: {}", 
                             decrypted_data.size(), fd_);
                    return false;
                }

                // // LOG_INFO("ProcessMessage: decompressed_data size: {}, fd: {}", 
                //         decompressed_data.size(), fd_);

                // 检查数据长度是否足够包含头部
                if (decompressed_data.size() < sizeof(RpcHeader))
                {
                    LOG_DEBUG("Waiting for more data, current size: {}, need: {}",
                             decompressed_data.size(), sizeof(RpcHeader));
                    return true; // 数据不完整，等待更多数据
                }

                RpcRequest request;
                if (!request.Deserialize(decompressed_data))
                {
                    LOG_ERROR("Failed to deserialize request body, decompressed size: {}, fd: {}", 
                             decompressed_data.size(), fd_);
                    return false;
                }

                // // LOG_INFO("ProcessMessage: successfully deserialized request, fd: {}", fd_);

                // 处理消息
                HandleMessage(request);

                // 清理已处理的数据
                read_buffer_.clear(); // 因为我们已经处理完整个加密数据
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("Error in ProcessMessage: {}, fd: {}", e.what(), fd_);
                return false;
            }
        }
        return true;
    }

    bool Connection::Write(const std::string &data)
    {
        // 检查连接状态
        if (state_ != State::CONNECTED)
        {
            LOG_ERROR("Connection is not in CONNECTED state for write, fd: {}, state: {}", 
                     fd_, static_cast<int>(state_));
            return false;
        }

        if (data.empty())
        {
            LOG_ERROR("write data is empty, fd: {}", fd_);
            return false;
        }

        // 添加到写缓冲
        write_buffer_.insert(write_buffer_.end(), data.begin(), data.end());

        // 尝试发送数据
        while (!write_buffer_.empty())
        {
            ssize_t n = write(fd_, write_buffer_.data(), write_buffer_.size());
            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break; // 非阻塞模式下暂时无法发送
                }
                LOG_ERROR("write error, fd: {}, errno: {}", fd_, errno);
                state_ = State::DISCONNECTED;
                return false;
            }
            write_buffer_.erase(write_buffer_.begin(), write_buffer_.begin() + n);
        }
        return true;
    }

    bool Connection::Write(const RpcResponse &response)
    {
        std::string data;
        if (!response.Serialize(data))
        {
            LOG_ERROR("serialize response failed");
            return false;
        }
        return Write(data);
    }

    void Connection::HandleError()
    {
        if (close_callback_)
        {
            close_callback_(shared_from_this());
        }
        Close();
    }

    void Connection::Close()
    {
        if (fd_ >= 0)
        {
            if (state_ == State::CONNECTED)
            {
                state_ = State::DISCONNECTING;
                // // LOG_INFO("Closing connection to {}:{}, fd: {}", peer_addr_, peer_port_, fd_);
            }
            
            ::close(fd_);
            fd_ = -1;
            state_ = State::DISCONNECTED;
            
            // // LOG_INFO("Connection closed, was connected to {}:{}", peer_addr_, peer_port_);
        }
    }

    void Connection::HandleMessage(const RpcRequest &request)
    {
        if (!message_callback_)
        {
            LOG_WARN("No message callback set, fd: {}", fd_);
            return;
        }

        try
        {
            message_callback_(shared_from_this(), request);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("Message callback failed: {}, service: {}, method: {}, fd: {}",
                      e.what(), request.getServiceName(), request.getMethodName(), fd_);
        }
        catch (...)
        {
            LOG_ERROR("Unknown exception in message callback, fd: {}", fd_);
        }
    }

} // namespace cookrpc
