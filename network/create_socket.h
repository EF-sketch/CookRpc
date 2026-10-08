#pragma once
#include "../utils/log_manager.h"
#include "../load_config/rpc_server_config.h"
#include <memory>
#include <string>
#include <netinet/tcp.h>

namespace cookrpc {
    class CreateSocket {
    public:
        static std::shared_ptr<CreateSocket> Create(const std::string &servers_name_prefix, uint16_t servers_port,
                                                    int servers_max_connections, int socket_timeout_ms, 
                                                    const std::string &servers_ip);
                                                    
        static std::shared_ptr<CreateSocket> Create(const cookrpc::RpcServerConfig &config);
        ~CreateSocket();
        int GetFd() const {
            return fd_;
        }

        const std::string &GetIp() const {
            return servers_ip_;
        }

        uint16_t GetPort() const  {
            return servers_port_;
        }

        CreateSocket(const std::string &servers_name_prefix, uint16_t swevwes_port,
                    int servers_max_connections, int socket_timeout_ms, const std::string &servers_ip);
        
        CreateSocket(const cookrpc::RpcServerConfig &config);

        int Accept();

    private:
        bool Init();
        bool SetSocketOpt();
        bool SetClientSocketOpt(int client_fd);

    private:
        int fd_;
        bool is_init_;
        std::string servers_name_prefix_;
        std::string servers_ip_;
        uint16_t servers_port_;
        int servers_max_connections_;
        int socket_timeout_ms_;
    };
}
