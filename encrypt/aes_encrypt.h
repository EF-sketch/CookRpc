#pragma once
#include <string>
#include <vector>
#include <memory>

namespace cookrpc {
    class AesEncrypt {
    public:
        static AesEncrypt &getInstance() {
            static AesEncrypt instance;
            return instance;
        }

        bool Encrypt (const std::string &data, std::string &ciphertext);

        bool Decrypt (const std::string &encrypted_data, std::string &plaintext);

        AesEncrypt  (const AesEncrypt &) = delete;
        AesEncrypt &operator=(const AesEncrypt &) = delete;



    private:
        AesEncrypt();
        ~AesEncrypt() = default;

        std::string GenerateKey() const;

        std::string Base64Encode(const std::string &input) const;

        std::string Base64Decode(const std::string &input) const;

        std::string ShiftEncrypt(const std::string &input, const std::string &key) const;

        std::string ShiftDecrypt(const std::string &input, const std::string &key) const;

        std::string master_key_;
        static constexpr size_t KEY_LENGTH = 32;
        static constexpr size_t BLOCK_SIZE = 16;
    };
}