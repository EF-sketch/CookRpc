#pragma once
#include <string>

namespace cookrpc {
    class Service {
    public:
        virtual ~Service() = default;

        virtual std::string GetServiceName() const = 0;

        virtual bool HandleRequest(const std::string &method_name, const std::string &args, std::string &result) = 0;
    };
}