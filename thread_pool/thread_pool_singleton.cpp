#include "thread_pool_singleton.h"

namespace meeting_ctrl {
    std::unique_ptr<ThreadPool> ThreadPoolSingleton::instance_;

    std::mutex ThreadPoolSingleton::mutex_;
}