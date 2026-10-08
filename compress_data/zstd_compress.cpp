#include "zstd_compress.h"
#include <stdexcept>
#include <iostream>

namespace cookrpc {
    bool ZstdCompress::CompressString(const std::string &src, std::string &dst, Level level) {
        if (src.empty()) {
            dst.clear();
            return true;
        }

        size_t const compressed_size = ZSTD_compressBound(src.size());
        dst.resize(compressed_size);

        size_t const actual_size = ZSTD_compress(dst.data(), compressed_size, src.data(), src.size(), static_cast<int>(level));

        if (ZSTD_isError(actual_size)) {
            return false;
        }

        dst.resize(actual_size);
        return true;
    }

    bool ZstdCompress::DecompressString(const std::string &src, std::string &dst) {
        if (src.empty()) {
            dst.clear();
            return true;
        }

        unsigned long long const decompressed_size = ZSTD_getFrameContentSize(src.data(), src.size());

        if (decompressed_size == ZSTD_CONTENTSIZE_ERROR || decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
            return false;
        }

        dst.resize(decompressed_size);

        size_t const actual_size = ZSTD_decompress(
            dst.data(), decompressed_size,
            src.data(), src.size()
        );

        if (ZSTD_isError(actual_size)) {
            return false;
        }

        dst.resize(actual_size);
        return true;
    }



    bool ZstdCompress::CompressData(const char *src, size_t src_len, std::vector<char> &dst, Level level) {
        if (!src || src_len == 0) {
            dst.clear();
            return true;
        }

        size_t const compressed_size = ZSTD_compressBound(src_len);
        dst.resize(compressed_size);

        size_t const actual_size = ZSTD_compress(
            dst.data(), compressed_size,
            src, src_len,
            static_cast<int>(level)
        );

        if (ZSTD_isError(actual_size)) {
            return false;
        }

        dst.resize(actual_size);
        return true;
    }

    bool ZstdCompress::DecompressData(const char *src, size_t src_len, std::vector<char> &dst) {
        if (!src ||src_len == 0) {
            dst.clear();
            return true;
        }

        unsigned long long const decompressed_size = ZSTD_getFrameContentSize(src, src_len);

        if (decompressed_size == ZSTD_CONTENTSIZE_ERROR || decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
            return false;
        }

        dst.resize(decompressed_size);

        size_t const actual_size = ZSTD_decompress(
            dst.data(), decompressed_size,
            src, src_len
        );

        if (ZSTD_isError(actual_size)) {
            return false;
        }

        dst.resize(actual_size);
        return true;
    }

    ZstdCompress::~ZstdCompress() { }

}