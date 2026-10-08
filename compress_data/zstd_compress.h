#pragma once
#include <memory>
#include <string>
#include <vector>
#include <zstd.h>
#include <mutex>

namespace cookrpc {
    enum class Level {
        FASTEST = 1,
        DEFAULT = 3,
        BETTER = 7,
        BEST = 19,
        MAX = 22
    };

    class ZstdCompress {
    public:
        static ZstdCompress &getInstance() {
            static ZstdCompress instance;
            return instance;
        }

        bool CompressString(const std::string &src, std::string &dst, Level level = Level::DEFAULT);

        bool DecompressString(const std::string &src, std::string &dst);

        bool CompressData(const char *src, size_t src_len, std::vector<char> &dst, Level level = Level::DEFAULT);

        bool DecompressData(const char *src, size_t src_len, std::vector<char> &dst);

        size_t GetCompressBound(size_t src_size) const {
            return ZSTD_compressBound(src_size);
        }

        ZstdCompress(const ZstdCompress &) = delete;
        ZstdCompress &operator=(const ZstdCompress &) = delete;

        ZstdCompress(ZstdCompress &&) = delete;
        ZstdCompress &operator=(ZstdCompress &&) = delete;

    private:
        ZstdCompress() = default;
        ~ZstdCompress();
    };
}