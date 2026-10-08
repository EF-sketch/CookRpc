#include "aes_encrypt.h"
#include "../utils/log_manager.h"
#include <random>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace cookrpc {
    namespace {
        const std::string BASE64_CHARS =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string GenerateRandomBytes(size_t length) {
            std::string result;
            result.reserve(length);

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 255);

            for (size_t i = 0; i < length; ++i) {
                result.push_back(static_cast<char>(dis(gen)));
            }
            return result;
        }

        std::string BytesToHexString(const std::string &data, size_t max_bytes = 32) {
            std::stringstream ss;
            ss << std::hex << std::uppercase << std::setfill('0');

            size_t bytes_to_show = std::min(data.size(), max_bytes);
            for (size_t i = 0; i < bytes_to_show; ++i) {
                ss << std::setw(2) << static_cast<int>(static_cast<unsigned char>(data[i])) << "";
            }

            if (data.size() > max_bytes) {
                ss << "...";
            }

            return ss.str();
        }
    }

    AesEncrypt::AesEncrypt() {
        master_key_ = "CookRPC_Secret_key_2024_Production!@#$%^&*";
    }

    std::string AesEncrypt::Base64Encode(const std::string &input) const {
        std::string ret;
        int i = 0;
        int j = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];

        const unsigned char *bytes_to_encode = 
            reinterpret_cast<const unsigned char *>(input.data());
        size_t in_len = input.length();
        
        while (in_len--) {
            char_array_3[i++] = *(bytes_to_encode++);

            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                for (i = 0; i < 4; i++) {
                    ret += BASE64_CHARS[char_array_4[i]];
                }

                i = 0;
            }
        }

        if (i) {
            for (j = i; j < 3; j++) {
                char_array_3[j] = '\0';
            }

            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

            for (j = 0; j < i + 1; j++) {
                ret += BASE64_CHARS[char_array_4[j]];
            }

            while ((i++ < 3)) {
                ret += '=';
            }

        }

        return ret;
    }

    std::string AesEncrypt::Base64Decode(const std::string &input) const {
        std::string ret;
        std::vector<int> base64_map(256, -1);
        for (size_t i = 0; i < BASE64_CHARS.size(); i++) {
            base64_map[BASE64_CHARS[i]] = i;
        }

        size_t in_len = input.size();
        int i = 0;
        int j = 0;
        int in_ = 0;
        unsigned char char_array_4[4], char_array_3[3];
        
        while (in_len-- && input[in_] != '=') {
            char_array_4[i++] = input[in_];
            in_++;
            if (i == 4) {
                for (i = 0; i < 4; i++) {
                    char_array_4[i] = base64_map[char_array_4[i]];
                }

                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                for (i = 0; i < 3; i++) {
                    ret += char_array_3[i];
                }

                i = 0;
            }
        }

        if (i) {
            for (j = i; j < 4; j++) {
                char_array_4[j] = 0;
            }

            for (j = 0; j < 4; j++) {
                char_array_4[j] = base64_map[char_array_4[j]];
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (j = 0; j < i - 1; j++)
            {
                ret += char_array_3[j];
            }


        }

        return ret;
    }




    std::string AesEncrypt::ShiftEncrypt (const std::string &input, const std::string &key) const {
        std::string result;
        result.reserve(input.size());

        for (size_t i = 0; i < input.size(); ++i) {
            unsigned char c = input[i];
            unsigned char k = key[i % key.size()];
            unsigned char shift = (i % 256);

            result.push_back(static_cast<char>((c + k + shift) % 256));
        } 
        return result;
    }

    std::string AesEncrypt::ShiftDecrypt(const std::string &input, const std::string &key) const {
        std::string result;
        result.reserve(input.size());

        for (size_t i = 0; i < input.size(); i++) {
            unsigned char c = input[i];
            unsigned char k = key[i %key.size()];
            unsigned char shift = (i % 256);

            result.push_back(static_cast<char>((c - k - shift + 512) % 256));
        }
        return result;
    }

    bool AesEncrypt::Encrypt(const std::string &data, std::string &ciphertext) {
        try {
            if (data.empty()) {
                LOG_ERROR("Input data is empty");
                return false;
            }

            std::string session_key = GenerateRandomBytes(KEY_LENGTH);
            std::string encrypted = ShiftEncrypt(data, session_key);

            std::string encrypted_key = ShiftEncrypt(session_key, master_key_);
            std::string combined = encrypted_key + encrypted; 

            ciphertext = Base64Encode(combined);

            return true;
            
        } catch (const std::exception &e) {
            LOG_ERROR("Encryption failed: {}", e.what());
            return false;
        }
    }

    bool AesEncrypt::Decrypt(const std::string &encrypted_data, std::string &plaintext) {
        try {
            std::string decoded = Base64Decode(encrypted_data);

            if (decoded.size() <= KEY_LENGTH) {
                LOG_ERROR("Decoded data too short: {} bytes, need > {} bytes", decoded.size(), KEY_LENGTH);
                return false;
            }

            std::string encrypted_key = decoded.substr(0, KEY_LENGTH);

            std::string encrypted_data_part = decoded.substr(KEY_LENGTH);

            std::string session_key = ShiftDecrypt(encrypted_key, master_key_);

            plaintext = ShiftDecrypt(encrypted_data_part, session_key);

            return true;
        } catch (const std::exception &e) {
            LOG_ERROR("Decryption failed: {}", e.what());
            return false;
        }
    } 


    
}

// int main() {
//     auto& encryptor = cookrpc::AesEncrypt::getInstance();

//     std::string original_data = "Hello, CookRPC! This is a secret message.";
//     std::string ciphertext;
//     std::string decrypted;

//     if (encryptor.Encrypt(original_data, ciphertext)) {
//         std::cout << "Encryption successful!" << std::endl;
//         std::cout << "Ciphertext: " << ciphertext << std::endl;
//         std::cout << "Ciphertext length: " << ciphertext.size() << std::endl;
//     } else {
//         std::cerr << "Encryption failed!" << std::endl;
//         return 1;
//     }

//     if (encryptor.Decrypt(ciphertext, decrypted)) {
//         std::cout << "Decryption successful!" << std::endl;
//         std::cout << "Decrypted: " << decrypted << std::endl;
//         std::cout << "Match: " << (original_data == decrypted ? "YES" : "NO") << std::endl;
//     } else {
//         std::cerr << "Decryption failed!" << std::endl;
//         return 1;
//     }

//     return 0;
// }