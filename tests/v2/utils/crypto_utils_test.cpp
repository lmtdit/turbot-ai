#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <turbot/utils/crypto_utils.hpp>
#include <filesystem>
#include <fstream>

using namespace turbot::utils::crypto;

// ==================== SHA256 Tests ====================

TEST_CASE("Crypto.SHA256.String", "[Utils][Crypto]") {
    std::string input = "hello world";
    std::string hash = sha256(input);
    
    // SHA256 produces 64 hex characters
    REQUIRE(hash.length() == 64);
    // Known SHA256 of "hello world"
    REQUIRE(hash == "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
}

TEST_CASE("Crypto.SHA256.EmptyString", "[Utils][Crypto]") {
    std::string hash = sha256("");
    REQUIRE(hash.length() == 64);
    // SHA256 of empty string
    REQUIRE(hash == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_CASE("Crypto.SHA256.Bytes", "[Utils][Crypto]") {
    std::vector<uint8_t> data = {'h', 'e', 'l', 'l', 'o'};
    std::vector<uint8_t> hash = sha256(data);
    
    // SHA256 produces 32 bytes
    REQUIRE(hash.size() == 32);
}

TEST_CASE("Crypto.SHA256.BytesEmpty", "[Utils][Crypto]") {
    std::vector<uint8_t> data;
    std::vector<uint8_t> hash = sha256(data);
    REQUIRE(hash.size() == 32);
}

// ==================== HMAC-SHA256 Tests ====================

TEST_CASE("Crypto.HMACSHA256.Basic", "[Utils][Crypto]") {
    std::string key = "secret_key";
    std::string data = "message to sign";
    std::string signature = hmac_sha256(key, data);
    
    REQUIRE(signature.length() == 64);
    // Same inputs should produce same output
    REQUIRE(signature == hmac_sha256(key, data));
}

TEST_CASE("Crypto.HMACSHA256.DifferentKeys", "[Utils][Crypto]") {
    std::string data = "same data";
    std::string sig1 = hmac_sha256("key1", data);
    std::string sig2 = hmac_sha256("key2", data);
    
    REQUIRE(sig1 != sig2);
}

TEST_CASE("Crypto.HMACSHA256.DifferentData", "[Utils][Crypto]") {
    std::string key = "same_key";
    std::string sig1 = hmac_sha256(key, "data1");
    std::string sig2 = hmac_sha256(key, "data2");
    
    REQUIRE(sig1 != sig2);
}

// ==================== AES-256-GCM Tests ====================

TEST_CASE("Crypto.AES256GCM.EncryptDecrypt", "[Utils][Crypto]") {
    std::string plaintext = "This is a secret message!";
    std::string key(32, 'k');  // 32-byte key
    
    AesGcmResult encrypted = aes_256_gcm_encrypt(plaintext, key);
    
    REQUIRE(!encrypted.ciphertext.empty());
    REQUIRE(encrypted.tag.length() == 16);
    REQUIRE(encrypted.nonce.length() == 12);
    
    std::string decrypted = aes_256_gcm_decrypt(encrypted, key);
    REQUIRE(decrypted == plaintext);
}

TEST_CASE("Crypto.AES256GCM.EmptyPlaintext", "[Utils][Crypto]") {
    std::string plaintext = "";
    std::string key(32, 'k');
    
    AesGcmResult encrypted = aes_256_gcm_encrypt(plaintext, key);
    std::string decrypted = aes_256_gcm_decrypt(encrypted, key);
    
    REQUIRE(decrypted == plaintext);
}

TEST_CASE("Crypto.AES256GCM.DifferentKeys", "[Utils][Crypto]") {
    std::string plaintext = "test data";
    std::string key1(32, 'a');
    std::string key2(32, 'b');
    
    AesGcmResult encrypted = aes_256_gcm_encrypt(plaintext, key1);
    
    // Decrypting with wrong key should throw
    REQUIRE_THROWS_AS(aes_256_gcm_decrypt(encrypted, key2), std::runtime_error);
}

TEST_CASE("Crypto.AES256GCM.LongPlaintext", "[Utils][Crypto]") {
    std::string plaintext(10000, 'x');  // 10KB of data
    std::string key(32, 'k');
    
    AesGcmResult encrypted = aes_256_gcm_encrypt(plaintext, key);
    std::string decrypted = aes_256_gcm_decrypt(encrypted, key);
    
    REQUIRE(decrypted == plaintext);
}

// ==================== Random Bytes Tests ====================

TEST_CASE("Crypto.RandomBytes.Length", "[Utils][Crypto]") {
    std::vector<uint8_t> bytes1 = random_bytes(16);
    std::vector<uint8_t> bytes2 = random_bytes(32);
    
    REQUIRE(bytes1.size() == 16);
    REQUIRE(bytes2.size() == 32);
}

TEST_CASE("Crypto.RandomBytes.Uniqueness", "[Utils][Crypto]") {
    std::vector<uint8_t> bytes1 = random_bytes(32);
    std::vector<uint8_t> bytes2 = random_bytes(32);
    
    // Two random byte arrays should be different
    REQUIRE(bytes1 != bytes2);
}

TEST_CASE("Crypto.RandomBytes.ZeroLength", "[Utils][Crypto]") {
    std::vector<uint8_t> bytes = random_bytes(0);
    REQUIRE(bytes.empty());
}

// ==================== Random String Tests ====================

TEST_CASE("Crypto.RandomString.Length", "[Utils][Crypto]") {
    std::string str1 = random_string(16);
    std::string str2 = random_string(32);
    
    REQUIRE(str1.length() == 16);
    REQUIRE(str2.length() == 32);
}

TEST_CASE("Crypto.RandomString.HexFormat", "[Utils][Crypto]") {
    std::string str = random_string(64);
    
    // All characters should be hex digits
    for (char c : str) {
        bool is_hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        REQUIRE(is_hex);
    }
}

TEST_CASE("Crypto.RandomString.Uniqueness", "[Utils][Crypto]") {
    std::string str1 = random_string(32);
    std::string str2 = random_string(32);
    
    REQUIRE(str1 != str2);
}

// ==================== Random Alphanumeric Tests ====================

TEST_CASE("Crypto.RandomAlphanumeric.Length", "[Utils][Crypto]") {
    std::string str = random_alphanumeric(20);
    REQUIRE(str.length() == 20);
}

TEST_CASE("Crypto.RandomAlphanumeric.Format", "[Utils][Crypto]") {
    std::string str = random_alphanumeric(100);
    
    for (char c : str) {
        bool is_alnum = (c >= '0' && c <= '9') ||
                        (c >= 'a' && c <= 'z') ||
                        (c >= 'A' && c <= 'Z');
        REQUIRE(is_alnum);
    }
}

TEST_CASE("Crypto.RandomAlphanumeric.Uniqueness", "[Utils][Crypto]") {
    std::string str1 = random_alphanumeric(32);
    std::string str2 = random_alphanumeric(32);
    
    REQUIRE(str1 != str2);
}

// ==================== Base64 Tests ====================

TEST_CASE("Crypto.Base64.EncodeString", "[Utils][Crypto]") {
    std::string input = "Hello, World!";
    std::string encoded = base64_encode(input);
    
    REQUIRE(encoded == "SGVsbG8sIFdvcmxkIQ==");
}

TEST_CASE("Crypto.Base64.EncodeBytes", "[Utils][Crypto]") {
    std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0x03};
    std::string encoded = base64_encode(data);
    
    REQUIRE_FALSE(encoded.empty());
}

TEST_CASE("Crypto.Base64.EncodeEmpty", "[Utils][Crypto]") {
    std::string encoded = base64_encode("");
    REQUIRE(encoded.empty());
}

TEST_CASE("Crypto.Base64.DecodeString", "[Utils][Crypto]") {
    std::string encoded = "SGVsbG8sIFdvcmxkIQ==";
    std::string decoded = base64_decode(encoded);
    
    REQUIRE(decoded == "Hello, World!");
}

TEST_CASE("Crypto.Base64.DecodeBytes", "[Utils][Crypto]") {
    std::string encoded = "AAECAw==";
    std::vector<uint8_t> decoded = base64_decode_bytes(encoded);
    
    REQUIRE(decoded.size() == 4);
    REQUIRE(decoded[0] == 0x00);
    REQUIRE(decoded[1] == 0x01);
    REQUIRE(decoded[2] == 0x02);
    REQUIRE(decoded[3] == 0x03);
}

TEST_CASE("Crypto.Base64.RoundTrip", "[Utils][Crypto]") {
    std::string original = "Test string with various characters: !@#$%^&*()";
    std::string encoded = base64_encode(original);
    std::string decoded = base64_decode(encoded);
    
    REQUIRE(decoded == original);
}

TEST_CASE("Crypto.Base64.InvalidDecode", "[Utils][Crypto]") {
    // "invalid" is valid base64 (all letters), only special chars are ignored
    REQUIRE_FALSE(base64_decode("invalid!@#$").empty());
}

// ==================== UUID Tests ====================

TEST_CASE("Crypto.UUID.Format", "[Utils][Crypto]") {
    std::string uuid = generate_uuid();
    
    // UUID format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    REQUIRE(uuid.length() == 36);
    REQUIRE(uuid[8] == '-');
    REQUIRE(uuid[13] == '-');
    REQUIRE(uuid[18] == '-');
    REQUIRE(uuid[23] == '-');
    // Version 4
    REQUIRE(uuid[14] == '4');
    // Variant
    REQUIRE((uuid[19] == '8' || uuid[19] == '9' || uuid[19] == 'a' || uuid[19] == 'b'));
}

TEST_CASE("Crypto.UUID.Uniqueness", "[Utils][Crypto]") {
    std::string uuid1 = generate_uuid();
    std::string uuid2 = generate_uuid();
    
    REQUIRE(uuid1 != uuid2);
}

TEST_CASE("Crypto.UUID.IsValid.Valid", "[Utils][Crypto]") {
    std::string uuid = generate_uuid();
    REQUIRE(is_valid_uuid(uuid));
}

TEST_CASE("Crypto.UUID.IsValid.InvalidFormat", "[Utils][Crypto]") {
    REQUIRE_FALSE(is_valid_uuid("not-a-uuid"));
    REQUIRE_FALSE(is_valid_uuid("12345678-1234-1234-1234-12345678901"));  // Too short
    REQUIRE_FALSE(is_valid_uuid("12345678-1234-1234-1234-1234567890123"));  // Too long
}

TEST_CASE("Crypto.UUID.IsValid.Empty", "[Utils][Crypto]") {
    REQUIRE_FALSE(is_valid_uuid(""));
}

TEST_CASE("Crypto.UUID.IsValid.InvalidVersion", "[Utils][Crypto]") {
    // Valid format but wrong version (not 4)
    REQUIRE_FALSE(is_valid_uuid("12345678-1234-1234-1234-123456789012"));
}

// ==================== File Hash Tests ====================

TEST_CASE("Crypto.SHA256File.NonExistent", "[Utils][Crypto]") {
    std::string hash = sha256_file("/non/existent/file.txt");
    REQUIRE(hash.empty());
}

TEST_CASE("Crypto.SHA256FileEx.NonExistent", "[Utils][Crypto]") {
    auto result = sha256_file_ex("/non/existent/file.txt");
    REQUIRE(std::holds_alternative<FileHashError>(result));
    REQUIRE(std::get<FileHashError>(result) == FileHashError::NotFound);
}

TEST_CASE("Crypto.SHA256FileEx.ValidFile", "[Utils][Crypto]") {
    // Create a temp file
    std::filesystem::path temp_path = std::filesystem::temp_directory_path() / "turbot_test_hash.txt";
    std::ofstream file(temp_path);
    file << "test content";
    file.close();
    
    auto result = sha256_file_ex(temp_path);
    REQUIRE(std::holds_alternative<std::string>(result));
    
    std::string hash = std::get<std::string>(result);
    REQUIRE(hash.length() == 64);
    
    // Cleanup
    std::filesystem::remove(temp_path);
}

TEST_CASE("Crypto.SHA256File.ValidFile", "[Utils][Crypto]") {
    // Create a temp file
    std::filesystem::path temp_path = std::filesystem::temp_directory_path() / "turbot_test_hash2.txt";
    std::ofstream file(temp_path);
    file << "hello world";
    file.close();
    
    std::string hash = sha256_file(temp_path);
    REQUIRE(hash.length() == 64);
    REQUIRE(hash == "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
    
    // Cleanup
    std::filesystem::remove(temp_path);
}
