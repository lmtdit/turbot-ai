#include "turbot/utils/crypto_utils.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace turbot::utils::crypto {

namespace {

    // 函数声明
    bool is_base64(uint8_t c);

    // 十六进制编码
    std::string to_hex(const std::vector<uint8_t>& data) {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (auto byte : data) {
            oss << std::setw(2) << static_cast<int>(byte);
        }
        return oss.str();
    }

    // 十六进制解码
    std::vector<uint8_t> from_hex(const std::string& hex) {
        std::vector<uint8_t> data;

        for (size_t i = 0; i < hex.length(); i += 2) {
            std::string byte_string = hex.substr(i, 2);
            uint8_t byte = static_cast<uint8_t>(std::stoul(byte_string, nullptr, 16));
            data.push_back(byte);
        }

        return data;
    }

    // Base64编码表
    constexpr std::string_view base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    // Base64编码
    std::string base64_encode_impl(const uint8_t* data, size_t length) {
        std::string result;
        size_t i = 0;
        uint8_t char_array_3[3];
        uint8_t char_array_4[4];

        while (length--) {
            char_array_3[i++] = *(data++);
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                for (size_t j = 0; j < 4; j++) {
                    result += base64_chars[char_array_4[j]];
                }
                i = 0;
            }
        }

        if (i) {
            for (size_t j = i; j < 3; j++) {
                char_array_3[j] = '\0';
            }

            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (size_t j = 0; j < i + 1; j++) {
                result += base64_chars[char_array_4[j]];
            }

            while (i++ < 3) {
                result += '=';
            }
        }

        return result;
    }

    // Base64解码
    std::vector<uint8_t> base64_decode_impl(const std::string& encoded) {
        size_t in_len = encoded.size();
        int i = 0;
        int j = 0;
        int in = 0;
        uint8_t char_array_4[4];
        uint8_t char_array_3[3];
        std::vector<uint8_t> result;

        while (in_len-- && (encoded[in] != '=') &&
               is_base64(encoded[in])) {
            char_array_4[i++] = encoded[in];
            in++;
            if (i == 4) {
                for (i = 0; i < 4; i++) {
                    char_array_4[i] = static_cast<uint8_t>(base64_chars.find(char_array_4[i]));
                }

                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                for (i = 0; i < 3; i++) {
                    result.push_back(char_array_3[i]);
                }
                i = 0;
            }
        }

        if (i) {
            for (j = i; j < 4; j++) {
                char_array_4[j] = 0;
            }

            for (j = 0; j < 4; j++) {
                char_array_4[j] = static_cast<uint8_t>(base64_chars.find(char_array_4[j]));
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (j = 0; j < i - 1; j++) {
                result.push_back(char_array_3[j]);
            }
        }

        return result;
    }

    bool is_base64(uint8_t c) {
        return (isalnum(c) || (c == '+') || (c == '/'));
    }
}

std::string sha256(const std::string& data) {
    std::vector<uint8_t> hash(SHA256_DIGEST_LENGTH);
    SHA256(reinterpret_cast<const uint8_t*>(data.c_str()), data.size(), hash.data());
    return to_hex(hash);
}

std::vector<uint8_t> sha256(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> hash(SHA256_DIGEST_LENGTH);
    SHA256(data.data(), data.size(), hash.data());
    return hash;
}

std::string hmac_sha256(const std::string& key, const std::string& data) {
    unsigned int digest_len;

    unsigned char* digest = HMAC(EVP_sha256(),
                                  key.c_str(), key.size(),
                                  reinterpret_cast<const unsigned char*>(data.c_str()), data.size(),
                                  nullptr, &digest_len);

    // 检查 HMAC 是否成功
    if (!digest) {
        throw std::runtime_error("HMAC computation failed");
    }

    return to_hex(std::vector<uint8_t>(digest, digest + digest_len));
}

AesGcmResult aes_256_gcm_encrypt(const std::string& plaintext, const std::string& key) {
    if (key.size() != 32) {
        throw std::runtime_error("AES-256-GCM requires a 32-byte key");
    }

    AesGcmResult result;

    // 生成随机nonce（12字节二进制）
    auto nonce_bytes = random_bytes(12);
    result.nonce = std::string(nonce_bytes.begin(), nonce_bytes.end());

    // 准备密文缓冲区（明文 + 16字节tag）
    std::vector<uint8_t> ciphertext(plaintext.size());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                           reinterpret_cast<const uint8_t*>(key.data()),
                           reinterpret_cast<const uint8_t*>(result.nonce.data())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize encryption");
    }

    int len;
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                          reinterpret_cast<const uint8_t*>(plaintext.data()),
                          plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to encrypt");
    }

    int ciphertext_len = len;
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to finalize encryption");
    }
    ciphertext_len += len;

    // 获取tag
    std::vector<uint8_t> tag(16);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to get tag");
    }

    EVP_CIPHER_CTX_free(ctx);

    result.ciphertext = std::string(ciphertext.begin(), ciphertext.begin() + ciphertext_len);
    result.tag = std::string(tag.begin(), tag.end());

    return result;
}

std::string aes_256_gcm_decrypt(const AesGcmResult& encrypted, const std::string& key) {
    if (key.size() != 32) {
        throw std::runtime_error("AES-256-GCM requires a 32-byte key");
    }

    if (encrypted.tag.size() != 16) {
        throw std::runtime_error("Invalid tag size");
    }

    std::vector<uint8_t> plaintext(encrypted.ciphertext.size());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                           reinterpret_cast<const uint8_t*>(key.data()),
                           reinterpret_cast<const uint8_t*>(encrypted.nonce.data())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize decryption");
    }

    int len;
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len,
                          reinterpret_cast<const uint8_t*>(encrypted.ciphertext.data()),
                          encrypted.ciphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to decrypt");
    }

    int plaintext_len = len;

    // 设置tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, encrypted.tag.size(),
                            const_cast<char*>(encrypted.tag.data())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to set tag");
    }

    // 验证tag并完成解密
    int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);

    if (ret <= 0) {
        throw std::runtime_error("Decryption failed: tag verification failed");
    }

    plaintext_len += len;

    return std::string(plaintext.begin(), plaintext.begin() + plaintext_len);
}

std::vector<uint8_t> random_bytes(size_t length) {
    std::vector<uint8_t> data(length);

    if (RAND_bytes(data.data(), length) != 1) {
        throw std::runtime_error("Failed to generate random bytes");
    }

    return data;
}

std::string random_string(size_t length) {
    auto bytes = random_bytes(length);
    return to_hex(bytes).substr(0, length);
}

std::string random_alphanumeric(size_t length) {
    const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.size() - 1);

    std::string result;
    result.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        result += chars[dis(gen)];
    }

    return result;
}

std::string base64_encode(const std::string& data) {
    return base64_encode_impl(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());
}

std::string base64_encode(const std::vector<uint8_t>& data) {
    return base64_encode_impl(data.data(), data.size());
}

std::string base64_decode(const std::string& encoded) {
    auto decoded = base64_decode_impl(encoded);
    return std::string(decoded.begin(), decoded.end());
}

std::vector<uint8_t> base64_decode_bytes(const std::string& encoded) {
    return base64_decode_impl(encoded);
}

std::string generate_uuid() {
    std::array<uint8_t, 16> uuid;
    if (RAND_bytes(uuid.data(), uuid.size()) != 1) {
        throw std::runtime_error("Failed to generate UUID");
    }

    // 设置版本号（4）和变体（RFC 4122）
    uuid[6] = (uuid[6] & 0x0F) | 0x40;  // 版本4
    uuid[8] = (uuid[8] & 0x3F) | 0x80;  // 变体

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (size_t i = 0; i < uuid.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            oss << '-';
        }
        oss << std::setw(2) << static_cast<int>(uuid[i]);
    }

    return oss.str();
}

bool is_valid_uuid(const std::string& uuid) {
    if (uuid.size() != 36) {
        return false;
    }

    // 检查格式：xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    std::regex uuid_regex(
        "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-4[0-9a-fA-F]{3}-[89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}$"
    );

    return std::regex_match(uuid, uuid_regex);
}

} // namespace turbot::utils::crypto