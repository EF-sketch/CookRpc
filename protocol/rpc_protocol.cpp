#include "rpc_protocol.h"
#include "../utils/log_manager.h"
#include <cstring>

namespace cookrpc
{
    const uint32_t RpcHeader::MAGIC = 0x12345678;

    bool RpcRequest::Serialize(std::string &out) const
    {
        try
        {
            // 计算消息体长度
            size_t body_size = service_name_.size() + method_name_.size() + payload_.size() +
                               3 * sizeof(uint32_t); // 3个字符串长度字段

            // 预分配空间
            out.resize(sizeof(RpcHeader) + body_size);

            // 填充头部
            RpcHeader header;
            header.magic_number = RpcHeader::MAGIC;
            header.message_length = body_size;
            header.sequence_id = sequence_id_;
            std::memcpy(&out[0], &header, sizeof(header));

            size_t pos = sizeof(header);

            // 写入 service_name
            uint32_t len = service_name_.size();
            std::memcpy(&out[pos], &len, sizeof(len));
            pos += sizeof(len);
            std::memcpy(&out[pos], service_name_.data(), len);
            pos += len;

            // 写入 method_name
            len = method_name_.size();
            std::memcpy(&out[pos], &len, sizeof(len));
            pos += sizeof(len);
            std::memcpy(&out[pos], method_name_.data(), len);
            pos += len;

            // 写入 payload
            len = payload_.size();
            std::memcpy(&out[pos], &len, sizeof(len));
            pos += sizeof(len);
            std::memcpy(&out[pos], payload_.data(), len);

            return true;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("Serialize failed: {}", e.what());
            return false;
        }
    }

    bool RpcRequest::Deserialize(const std::string &in)
    {
        try
        {

            // 检查头部
            if (in.size() < sizeof(RpcHeader))
            {
                LOG_ERROR("Input too small for header Deserialize. Size: {}, Need: {}",
                          in.size(), sizeof(RpcHeader));
                return false;
            }

            // 读取头部
            RpcHeader header;
            std::memcpy(&header, in.data(), sizeof(header));

            // 验证魔数
            if (header.magic_number != RpcHeader::MAGIC)
            {
                LOG_ERROR("Invalid magic number: {:#x}, expected: {:#x}",
                          header.magic_number, RpcHeader::MAGIC);
                return false;
            }

            size_t pos = sizeof(RpcHeader);
            uint32_t len = 0;

            // 读取 service_name
            if (pos + sizeof(uint32_t) > in.size())
            {
                LOG_ERROR("Cannot read service_name length. pos: {}, size: {}", pos, in.size());
                return false;
            }
            std::memcpy(&len, &in[pos], sizeof(len));
            pos += sizeof(len);

            if (len > in.size() - pos)
            {
                LOG_ERROR("Service name too long: len={}, remaining={}", len, in.size() - pos);
                return false;
            }
            service_name_ = in.substr(pos, len);
            pos += len;

            // 读取 method_name
            if (pos + sizeof(uint32_t) > in.size())
            {
                LOG_ERROR("Cannot read method_name length. pos: {}, size: {}", pos, in.size());
                return false;
            }
            std::memcpy(&len, &in[pos], sizeof(len));
            pos += sizeof(len);

            if (len > in.size() - pos)
            {
                LOG_ERROR("Method name too long: len={}, remaining={}", len, in.size() - pos);
                return false;
            }
            method_name_ = in.substr(pos, len);
            pos += len;

            // 读取 payload
            if (pos + sizeof(uint32_t) > in.size())
            {
                LOG_ERROR("Cannot read payload length. pos: {}, size: {}", pos, in.size());
                return false;
            }
            std::memcpy(&len, &in[pos], sizeof(len));
            pos += sizeof(len);

            if (len > in.size() - pos)
            {
                LOG_ERROR("Payload too long: len={}, remaining={}", len, in.size() - pos);
                return false;
            }
            payload_ = in.substr(pos, len);

            sequence_id_ = header.sequence_id;
            return true;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("Deserialize failed: {}", e.what());
            return false;
        }
    }

    bool RpcResponse::Serialize(std::string &out) const
    {
        try
        {

            // 计算消息体长度
            size_t body_size = result_data_.size() + error_message_.size() +
                               2 * sizeof(uint32_t) + // 用来表示result_data_和error_message_的长度
                               sizeof(error_code_) +  // error_code
                               sizeof(sequence_id_);  // sequence_id

            // 预分配空间
            out.resize(sizeof(RpcHeader) + body_size);

            // 填充头部
            RpcHeader header;
            header.magic_number = RpcHeader::MAGIC;
            header.message_length = body_size;
            header.sequence_id = sequence_id_;
            std::memcpy(&out[0], &header, sizeof(header));

            size_t pos = sizeof(header);

            // 写入 result_data
            uint32_t len = result_data_.size();
            std::memcpy(&out[pos], &len, sizeof(len));
            pos += sizeof(len);
            std::memcpy(&out[pos], result_data_.data(), len);
            pos += len;

            // 写入 error_message
            len = error_message_.size();
            std::memcpy(&out[pos], &len, sizeof(len));
            pos += sizeof(len);
            std::memcpy(&out[pos], error_message_.data(), len);
            pos += len;

            // 写入 error_code
            std::memcpy(&out[pos], &error_code_, sizeof(error_code_));
            pos += sizeof(error_code_);

            // 写入 sequence_id
            std::memcpy(&out[pos], &sequence_id_, sizeof(sequence_id_));
            pos += sizeof(sequence_id_);

            return true;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("Response serialize failed: {}", e.what());
            return false;
        }
    }

    bool RpcResponse::Deserialize(const std::string &in)
    {
        try
        {

            // 检查头部
            if (in.size() < sizeof(RpcHeader))
            {
                LOG_ERROR("Input too small for header Deserialize. Size: {}, Need: {}",
                          in.size(), sizeof(RpcHeader));
                return false;
            }

            // 读取头部
            RpcHeader header;
            std::memcpy(&header, in.data(), sizeof(header));

            // 验证魔数
            if (header.magic_number != RpcHeader::MAGIC)
            {

                return false;
            }

            // 验证消息长度
            if (in.size() != sizeof(header) + header.message_length)
            {
                LOG_ERROR("Invalid message length: expected {}, got {}",
                          sizeof(header) + header.message_length, in.size());
                return false;
            }

            size_t pos = sizeof(header);
            uint32_t len = 0;

            // 读取 result_data
            if (pos + sizeof(uint32_t) > in.size())
            {
                LOG_ERROR("Cannot read result_data length. pos: {}, size: {}", pos, in.size());
                return false;
            }
            std::memcpy(&len, &in[pos], sizeof(len));
            pos += sizeof(len);

            if (pos + len > in.size())
            {
                LOG_ERROR("Result data too long: len={}, remaining={}", len, in.size() - pos);
                return false;
            }
            result_data_ = in.substr(pos, len);
            pos += len;

            // 读取 error_message
            if (pos + sizeof(uint32_t) > in.size())
            {
                LOG_ERROR("Cannot read error_message length. pos: {}, size: {}", pos, in.size());
                return false;
            }
            std::memcpy(&len, &in[pos], sizeof(len));
            pos += sizeof(len);

            if (pos + len > in.size())
            {
                LOG_ERROR("Error message too long: len={}, remaining={}", len, in.size() - pos);
                return false;
            }
            error_message_ = in.substr(pos, len);
            pos += len;

            // 读取 error_code
            if (pos + sizeof(error_code_) > in.size())
            {
                LOG_ERROR("Cannot read error_code. pos: {}, size: {}", pos, in.size());
                return false;
            }
            std::memcpy(&error_code_, &in[pos], sizeof(error_code_));
            pos += sizeof(error_code_);

            // 读取 sequence_id
            if (pos + sizeof(sequence_id_) > in.size())
            {
                LOG_ERROR("Cannot read sequence_id. pos: {}, size: {}", pos, in.size());
                return false;
            }
            std::memcpy(&sequence_id_, &in[pos], sizeof(sequence_id_));
            pos += sizeof(sequence_id_);

            return true;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("Response deserialize failed: {}", e.what());
            return false;
        }
    }

} // namespace cookrpc