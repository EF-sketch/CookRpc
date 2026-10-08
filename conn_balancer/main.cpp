#include "load_balancer.h"
#include <iostream>
#include <vector>

int main()
{
    // 1. 初始化负载均衡器 (轮询算法)
    cookrpc::LoadBalancer::initBalancer("round");

    // 2. 服务实例列表
    std::vector<std::string> servers = {
        "192.168.1.100:8001",
        "192.168.1.101:8001",
        "192.168.1.102:8001"
    };

    // 3. 多次选择服务器
    for (int i = 0; i < 10; ++i)
    {
        std::string server = cookrpc::LoadBalancer::selectServer(servers);
        std::cout << "Request " << i << ": " << server << std::endl;
    }

    return 0;
}
