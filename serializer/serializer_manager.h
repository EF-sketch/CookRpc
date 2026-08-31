#pragma once
#include <type_traits>
#include "json_serializer.h"
#include "../utils/log_manager.h"
#include "protobuf_serializer.h"

namespace cookrpc {

    enum class SerializeType {
        JSON,
        PROTOBUF
    };


    class SerializerManager {
    public:
        template <typename T>
        static std::string serialize(const T &data, SerializeType type) {
            try {
                switch (type) {
                    case SerializeType::JSON:
                        if constexpr (std::is_same_v<T, nlohmann::json>) {
                            return serialize_json(data);
                        } else {
                            LOG_ERROR("Invalid type for JSON serialization");
                            return "";
                        }
                    case SerializeType::PROTOBUF:
                        if constexpr (std::is_base_of_v<google::protobuf::Message, T>) {
                            return serialize_protobuf(data);
                        } else {
                            LOG_ERROR("Invalid type for Protobuf serialization");
                            return "";
                        }
                    default:
                        LOG_ERROR("Unknown serialization type");
                        return "";
                }
            } catch (const std::exception &e) {
                LOG_ERROR("Serialization error: {}", e.what());
                return "";
            }
        }

        template <typename T>
        static bool deserialize(const std::string &buffer, T &data, SerializeType type) {
            try {
                switch (type) {
                    case SerializeType::JSON:
                        if constexpr (std::is_same_v<T, nlohmann::json>) {
                            return deserialize_json(buffer, data);
                        } else {
                            LOG_ERROR("Invalid type for JSON deserialization");
                            return false;
                        }
                    case SerializeType::PROTOBUF:
                        if constexpr (std::is_base_of_v<google::protobuf::Message, T>) {
                            return deserialize_protobuf(buffer, data);
                        } else {
                            LOG_ERROR("Invalid type for Protobuf deserialization");
                            return false;
                        }
                    default:
                        LOG_ERROR("Unknown serialization type");
                        return false;
                }
            } catch (const std::exception &e) {
                LOG_ERROR("Deserialization error: {}", e.what());
                return false;
            }
        }

    private:
        template <typename T>
        static std::string serialize_protobuf(const T &data) {
            return ProtobufSerializer::serialize(data);
        }

        static std::string serialize_json(const nlohmann::json &data) {
            return JSONSerializer::serialize(data);
        }

        template <typename T>
        static bool deserialize_protobuf(const std::string &buffer, T &data) {
            return ProtobufSerializer::deserialize(buffer, data);
        }

        template <typename T>
        static bool deserialize_json (const std::string &buffer, T &data) {
            return JSONSerializer::deserialize(buffer, data);
        }
    };

}
