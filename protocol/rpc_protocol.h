#pragma once
#include <string>
#include <stdint.h>
#include "../serializer/serializer_manager.h"

namespace cookrpc
{

    
    struct RpcHeader
    {
        uint32_t magic_number;   // 魔数
        uint32_t message_length; // 消息体长度
        uint32_t sequence_id;    // 序列号

        static const uint32_t MAGIC;
    };

    class RpcMessage
    {
    public:
        virtual ~RpcMessage() = default;
        virtual bool Serialize(std::string &out) const = 0;
        virtual bool Deserialize(const std::string &in) = 0;
        uint32_t sequence_id_; // 序列号
        uint32_t getSequenceId() const { return sequence_id_; }
        void setSequenceId(uint32_t sequence_id) { sequence_id_ = sequence_id; }
    };

    class RpcRequest : public RpcMessage
    {
    public:
        void setPayload(const std::string &payload) { payload_ = payload; }
        void setServiceName(const std::string &service_name) { service_name_ = service_name; }
        void setMethodName(const std::string &method_name) { method_name_ = method_name; }
        std::string getPayload() const { return payload_; }
        uint32_t getSequenceId() const { return sequence_id_; }
        std::string getServiceName() const { return service_name_; }
        std::string getMethodName() const { return method_name_; }

        bool Serialize(std::string &out) const override;
        bool Deserialize(const std::string &in) override;

    private:
        std::string service_name_; // 服务名
        std::string method_name_;  // 函数名
        std::string payload_;      // 参数
    };

    class RpcResponse : public RpcMessage
    {
    public:
        bool Serialize(std::string &out) const override;
        bool Deserialize(const std::string &in) override;
        std::string getResultData() const { return result_data_; }
        std::string getErrorMessage() const { return error_message_; }

        uint32_t getErrorCode() const { return error_code_; }
        void setResultData(const std::string &result_data) { result_data_ = result_data; }
        void setErrorMessage(const std::string &error_message) { error_message_ = error_message; }
        void setErrorCode(uint32_t error_code) { error_code_ = error_code; }

    private:
        std::string result_data_;
        std::string error_message_;
        uint32_t error_code_;
    };
}