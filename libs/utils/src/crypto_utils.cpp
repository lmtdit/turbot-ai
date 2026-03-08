#include "turbot/utils/crypto_utils.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <limits>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace turbot::utils::crypto {

namespace {

    // 函数声明
    bool is_base64(uint8_t c);

    // RAII wrapper for EVP_CIPHER_CTX
    struct EvpCipherCtxDeleter {
        void operator()(EVP_CIPHER_CTX* ctx) const {
            if (ctx) EVP_CIPHER_CTX_free(ctx);
        }
    };
    using EvpCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, EvpCipherCtxDeleter>;

    // RAII wrapper for EVP_MD_CTX
    struct EvpMdCtxDeleter {
        void operator()(EVP_MD_CTX* ctx) const {
            if (ctx) EVP_MD_CTX_free(ctx);
        }
    };
    using EvpMdCtxPtr = std::unique_ptr<EVP_MD_CTX, EvpMdCtxDeleter>;

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
        // 输入验证：长度必须为偶数
        if (hex.length() % 2 != 0) {
            throw std::invalid_argument("Hex string must have even length");
        }

        std::vector<uint8_t> data;
        data.reserve(hex.length() / 2);

        for (size_t i = 0; i < hex.length(); i += 2) {
            // 验证字符是否为有效十六进制字符
            if (!std::isxdigit(static_cast<unsigned char>(hex[i])) ||
                !std::isxdigit(static_cast<unsigned char>(hex[i + 1]))) {
                throw std::invalid_argument("Invalid hex character in string");
            }
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
        size_t i = 0;   // index into char_array_4 (0-3)
        size_t j = 0;   // index into char_array_3
        size_t in = 0;  // index into encoded — size_t avoids signed overflow on large inputs
        uint8_t char_array_4[4];
        uint8_t char_array_3[3];
        std::vector<uint8_t> result;

        while (in_len-- && (encoded[in] != '=') &&
               is_base64(encoded[in])) {
            char_array_4[i++] = encoded[in];
            in++;
            if (i == 4) {
                for (i = 0; i < 4; i++) {
                    size_t pos = base64_chars.find(char_array_4[i]);
                    if (pos == std::string_view::npos) {
                        throw std::runtime_error("Invalid base64 character");
                    }
                    char_array_4[i] = static_cast<uint8_t>(pos);
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
                // Only validate characters that were actually read from input
                // j < i means it was a real character, j >= i means it's padding (0)
                if (j < i) {
                    size_t pos = base64_chars.find(char_array_4[j]);
                    if (pos == std::string_view::npos) {
                        throw std::runtime_error("Invalid base64 character");
                    }
                    char_array_4[j] = static_cast<uint8_t>(pos);
                }
                // Padding characters (j >= i) are already 0, which is correct for decoding
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
    EvpMdCtxPtr ctx(EVP_MD_CTX_new());
    if (!ctx) {
        throw std::runtime_error("Failed to create digest context");
    }

    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("Failed to initialize SHA256");
    }

    if (EVP_DigestUpdate(ctx.get(), data.data(), data.size()) != 1) {
        throw std::runtime_error("Failed to update SHA256");
    }

    std::vector<uint8_t> hash(SHA256_DIGEST_LENGTH);
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(ctx.get(), hash.data(), &hash_len) != 1) {
        throw std::runtime_error("Failed to finalize SHA256");
    }

    return to_hex(hash);
}

std::vector<uint8_t> sha256(const std::vector<uint8_t>& data) {
    EvpMdCtxPtr ctx(EVP_MD_CTX_new());
    if (!ctx) {
        throw std::runtime_error("Failed to create digest context");
    }

    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("Failed to initialize SHA256");
    }

    if (EVP_DigestUpdate(ctx.get(), data.data(), data.size()) != 1) {
        throw std::runtime_error("Failed to update SHA256");
    }

    std::vector<uint8_t> hash(SHA256_DIGEST_LENGTH);
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(ctx.get(), hash.data(), &hash_len) != 1) {
        throw std::runtime_error("Failed to finalize SHA256");
    }

    return hash;
}

std::string hmac_sha256(const std::string& key, const std::string& data) {
    // Use an explicit output buffer (not nullptr) so we are independent of
    // OpenSSL’s internal static buffer, which is shared across threads on older
    // OpenSSL versions and causes a data race under concurrent use.
    // Also explicitly cast key size to int to avoid implicit narrowing.
    std::array<uint8_t, EVP_MAX_MD_SIZE> buf{};
    unsigned int digest_len = 0;

    if (key.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("HMAC key too large");
    }

    unsigned char* digest = HMAC(
        EVP_sha256(),
        key.data(), static_cast<int>(key.size()),
        reinterpret_cast<const unsigned char*>(data.data()), data.size(),
        buf.data(), &digest_len);

    if (!digest) {
        throw std::runtime_error("HMAC computation failed");
    }

    return to_hex(std::vector<uint8_t>(buf.data(), buf.data() + digest_len));
}

AesGcmResult aes_256_gcm_encrypt(const std::string& plaintext, const std::string& key) {
    if (key.size() != 32) {
        throw std::runtime_error("AES-256-GCM requires a 32-byte key");
    }

    if (plaintext.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("Plaintext too large for AES-GCM encryption");
    }

    AesGcmResult result;

    // 生成随机nonce（12字节二进制）
    auto nonce_bytes = random_bytes(12);
    result.nonce = std::string(nonce_bytes.begin(), nonce_bytes.end());

    // 准备密文缓冲区（明文 + 16字节tag）
    std::vector<uint8_t> ciphertext(plaintext.size());

    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr,
                           reinterpret_cast<const uint8_t*>(key.data()),
                           reinterpret_cast<const uint8_t*>(result.nonce.data())) != 1) {
        throw std::runtime_error("Failed to initialize encryption");
    }

    int len;
    if (EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &len,
                          reinterpret_cast<const uint8_t*>(plaintext.data()),
                          static_cast<int>(plaintext.size())) != 1) {
        throw std::runtime_error("Failed to encrypt");
    }

    int ciphertext_len = len;
    if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + len, &len) != 1) {
        throw std::runtime_error("Failed to finalize encryption");
    }
    ciphertext_len += len;

    // 获取tag
    std::vector<uint8_t> tag(16);
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1) {
        throw std::runtime_error("Failed to get tag");
    }

    result.ciphertext = std::string(ciphertext.begin(), ciphertext.begin() + ciphertext_len);
    result.tag = std::string(tag.begin(), tag.end());

    return result;
}

std::string aes_256_gcm_decrypt(const AesGcmResult& encrypted, const std::string& key) {
    if (key.size() != 32) {
        throw std::runtime_error("AES-256-GCM requires a 32-byte key");
    }

    if (encrypted.nonce.size() != 12) {
        throw std::runtime_error("Invalid nonce size: AES-256-GCM requires a 12-byte nonce");
    }

    if (encrypted.tag.size() != 16) {
        throw std::runtime_error("Invalid tag size");
    }

    if (encrypted.ciphertext.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("Ciphertext too large for AES-GCM decryption");
    }

    std::vector<uint8_t> plaintext(encrypted.ciphertext.size());

    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr,
                           reinterpret_cast<const uint8_t*>(key.data()),
                           reinterpret_cast<const uint8_t*>(encrypted.nonce.data())) != 1) {
        throw std::runtime_error("Failed to initialize decryption");
    }

    int len;
    if (EVP_DecryptUpdate(ctx.get(), plaintext.data(), &len,
                          reinterpret_cast<const uint8_t*>(encrypted.ciphertext.data()),
                          static_cast<int>(encrypted.ciphertext.size())) != 1) {
        throw std::runtime_error("Failed to decrypt");
    }

    int plaintext_len = len;

    // 设置tag - OpenSSL API requires non-const pointer
    std::vector<uint8_t> tag_copy(encrypted.tag.begin(), encrypted.tag.end());
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, encrypted.tag.size(),
                            tag_copy.data()) != 1) {
        throw std::runtime_error("Failed to set tag");
    }

    // 验证tag并完成解密
    int ret = EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + len, &len);

    if (ret <= 0) {
        throw std::runtime_error("Decryption failed: tag verification failed");
    }

    plaintext_len += len;

    return std::string(plaintext.begin(), plaintext.begin() + plaintext_len);
}

std::vector<uint8_t> random_bytes(size_t length) {
    if (length > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("Requested byte count exceeds INT_MAX");
    }
    std::vector<uint8_t> data(length);

    if (RAND_bytes(data.data(), static_cast<int>(length)) != 1) {
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
    constexpr uint8_t chars_size = 62;
    constexpr uint8_t max_valid_byte = 247;  // 62 * 4 - 1 = 247 (largest value where byte % 62 is unbiased)
    
    std::string result;
    result.reserve(length);
    
    // Use rejection sampling to eliminate modulo bias
    while (result.size() < length) {
        auto bytes = random_bytes(length - result.size() + 16);  // Get extra bytes for rejection
        for (auto byte : bytes) {
            if (byte <= max_valid_byte) {
                result += chars[byte % chars_size];
                if (result.size() >= length) break;
            }
            // Reject bytes > max_valid_byte to avoid modulo bias
        }
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

    // Compile the regex only once (it is expensive to construct)
    static const std::regex uuid_regex(
        "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-4[0-9a-fA-F]{3}-[89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}$"
    );

    return std::regex_match(uuid, uuid_regex);
}

} // namespace turbot::utils::crypto