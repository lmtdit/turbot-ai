#pragma once

#include <turbot/utils/export.hpp>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <filesystem>

namespace turbot::utils::crypto {

/**
 * @brief SHA256哈希
 * @param data 输入数据
 * @return SHA256哈希值（十六进制字符串）
 */
[[nodiscard]] TURBOT_UTILS_API std::string sha256(const std::string& data);

/**
 * @brief SHA256哈希（字节数组）
 * @param data 输入数据
 * @return SHA256哈希值（32字节）
 */
[[nodiscard]] TURBOT_UTILS_API std::vector<uint8_t> sha256(const std::vector<uint8_t>& data);

/**
 * @brief HMAC-SHA256签名
 * @param key 密钥
 * @param data 数据
 * @return HMAC-SHA256签名（十六进制字符串）
 */
[[nodiscard]] TURBOT_UTILS_API std::string hmac_sha256(const std::string& key,
                                             const std::string& data);

/**
 * @brief AES-256-GCM加密结果
 */
struct AesGcmResult {
    std::string ciphertext;  ///< 密文
    std::string tag;         ///< 认证标签（16字节）
    std::string nonce;       ///< 随机数（12字节）
};

/**
 * @brief AES-256-GCM加密
 * @param plaintext 明文
 * @param key 密钥（32字节）
 * @return 加密结果
 * @throws std::runtime_error 如果加密失败
 */
[[nodiscard]] TURBOT_UTILS_API AesGcmResult aes_256_gcm_encrypt(const std::string& plaintext,
                                                      const std::string& key);

/**
 * @brief AES-256-GCM解密
 * @param encrypted 加密结果
 * @param key 密钥（32字节）
 * @return 明文
 * @throws std::runtime_error 如果解密失败或验证失败
 */
[[nodiscard]] TURBOT_UTILS_API std::string aes_256_gcm_decrypt(const AesGcmResult& encrypted,
                                                     const std::string& key);

/**
 * @brief 生成随机字节
 * @param length 字节数
 * @return 随机字节
 * @throws std::runtime_error 如果生成失败
 */
[[nodiscard]] TURBOT_UTILS_API std::vector<uint8_t> random_bytes(size_t length);

/**
 * @brief 生成随机字符串（十六进制）
 * @param length 字符串长度
 * @return 随机字符串
 * @throws std::runtime_error 如果生成失败
 */
[[nodiscard]] TURBOT_UTILS_API std::string random_string(size_t length);

/**
 * @brief 生成随机字符串（字母数字）
 * @param length 字符串长度
 * @return 随机字符串
 * @throws std::runtime_error 如果生成失败
 */
[[nodiscard]] TURBOT_UTILS_API std::string random_alphanumeric(size_t length);

/**
 * @brief Base64编码
 * @param data 输入数据
 * @return Base64编码字符串
 */
[[nodiscard]] TURBOT_UTILS_API std::string base64_encode(const std::string& data);

/**
 * @brief Base64编码（字节数组）
 * @param data 输入数据
 * @return Base64编码字符串
 */
[[nodiscard]] TURBOT_UTILS_API std::string base64_encode(const std::vector<uint8_t>& data);

/**
 * @brief Base64解码
 * @param encoded Base64编码字符串
 * @return 解码后的数据
 * @throws std::runtime_error 如果解码失败
 */
[[nodiscard]] TURBOT_UTILS_API std::string base64_decode(const std::string& encoded);

/**
 * @brief Base64解码（返回字节数组）
 * @param encoded Base64编码字符串
 * @return 解码后的数据
 * @throws std::runtime_error 如果解码失败
 */
[[nodiscard]] TURBOT_UTILS_API std::vector<uint8_t> base64_decode_bytes(const std::string& encoded);

/**
 * @brief 生成UUID v4
 * @return UUID字符串（格式：xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx）
 */
[[nodiscard]] TURBOT_UTILS_API std::string generate_uuid();

/**
 * @brief 验证UUID格式
 * @param uuid UUID字符串
 * @return 是否为有效的UUID
 */
[[nodiscard]] TURBOT_UTILS_API bool is_valid_uuid(const std::string& uuid);

/**
 * @brief 计算文件内容的 SHA-256 哈希
 * @param path 文件路径
 * @return 十六进制 SHA-256 哈希字符串，文件不存在或无法读取时返回空字符串
 */
[[nodiscard]] TURBOT_UTILS_API std::string sha256_file(const std::filesystem::path& path);

} // namespace turbot::utils::crypto