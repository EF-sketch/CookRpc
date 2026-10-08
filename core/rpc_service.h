#pragma once
#include "../utils/log_manager.h"
#include "../service/service.h"
#include <nlohmann/json.hpp>

namespace cookrpc {
    class RpcService : public Service {
    public:
        std::string GetServiceName() const override {
            return "RpcService";
        }

        bool HandleRequest(const std::string &method_name,
                           const std::string &args,
                           std::string &result) override
        {
            if (method_name == "Echo") {
                try {
                    nlohmann::json request = nlohmann::json::parse(args);

                    nlohmann::json response;
                    response["echo"] = "hahah i amrpc server, welcome to C++ training camp, come on";
                    response["received_message"] = request.value("message", "");

                    result = response.dump();
                    return true;
                } catch (const std::exception &e) {
                    LOG_ERROR("Failed to process Echo request: {}", e.what());

                    nlohmann::json error_response;
                    error_response["error"] = "Invalid request format";
                    result = error_response.dump();
                    return false;
                }
            }

            LOG_ERROR("Unknow method: {}", method_name);
            return false;
        }

    };
}
