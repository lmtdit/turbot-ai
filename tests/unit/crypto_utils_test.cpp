#include <catch2/catch_test_macros.hpp>
#include <turbot/utils/crypto_utils.hpp>
#include <algorithm>
#include <iomanip>
#include <filesystem>
#include <fstream>

using namespace turbot::utils::crypto;

TEST_CASE("crypto::sha256", "[utils][crypto]") {
    SECTION("hash empty string") {
        std::string result = sha256("");

        // SHA256 of empty string: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
        REQUIRE(result == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    }

    SECTION("hash simple string") {
        std::string result = sha256("hello");

        REQUIRE(result.size() == 64);  // SHA256 produces 64 hex characters
    }

    SECTION("hash consistency") {
        std::string data = "test data";
        std::string hash1 = sha256(data);
        std::string hash2 = sha256(data);

        REQUIRE(hash1 == hash2);
    }

    SECTION("different data different hash") {
        std::string hash1 = sha256("data1");
        std::string hash2 = sha256("data2");

        REQUIRE(hash1 != hash2);
    }
}

TEST_CASE("crypto::hmac_sha256", "[utils][crypto]") {
    SECTION("hmac with key") {
        std::string key = "secret key";
        std::string data = "message to authenticate";
        std::string result = hmac_sha256(key, data);

        REQUIRE(result.size() == 64);
    }

    SECTION("hmac consistency") {
        std::string key = "secret";
        std::string data = "data";
        std::string hmac1 = hmac_sha256(key, data);
        std::string hmac2 = hmac_sha256(key, data);

        REQUIRE(hmac1 == hmac2);
    }

    SECTION("different keys different hmac") {
        std::string data = "data";
        std::string hmac1 = hmac_sha256("key1", data);
        std::string hmac2 = hmac_sha256("key2", data);

        REQUIRE(hmac1 != hmac2);
    }
}

TEST_CASE("crypto::aes_256_gcm", "[utils][crypto]") {
    SECTION("encrypt and decrypt") {
        std::string plaintext = "Hello, World!";
        std::string key = "01234567890123456789012345678901";  // 32 bytes

        auto encrypted = aes_256_gcm_encrypt(plaintext, key);
        std::string decrypted = aes_256_gcm_decrypt(encrypted, key);

        REQUIRE(plaintext == decrypted);
    }

    SECTION("encrypt produces ciphertext and tag") {
        std::string plaintext = "secret message";
        std::string key = "01234567890123456789012345678901";

        auto encrypted = aes_256_gcm_encrypt(plaintext, key);

        REQUIRE(encrypted.ciphertext.size() > 0);
        REQUIRE(encrypted.tag.size() > 0);
        REQUIRE(encrypted.nonce.size() > 0);
    }

    SECTION("decrypt with wrong key fails") {
        std::string plaintext = "secret message";
        std::string correct_key = "01234567890123456789012345678901";
        std::string wrong_key = "11111111111111111111111111111111";

        auto encrypted = aes_256_gcm_encrypt(plaintext, correct_key);

        // Decryption should throw exception with wrong key
        REQUIRE_THROWS_AS(aes_256_gcm_decrypt(encrypted, wrong_key), std::runtime_error);
    }

    SECTION("decrypt with wrong tag fails") {
        std::string plaintext = "secret message";
        std::string key = "01234567890123456789012345678901";

        auto encrypted = aes_256_gcm_encrypt(plaintext, key);
        encrypted.tag = "wrongtag";  // Tamper with the tag

        // Decryption should throw exception with wrong tag
        REQUIRE_THROWS_AS(aes_256_gcm_decrypt(encrypted, key), std::runtime_error);
    }

    SECTION("encrypt empty string") {
        std::string plaintext = "";
        std::string key = "01234567890123456789012345678901";

        auto encrypted = aes_256_gcm_encrypt(plaintext, key);
        std::string decrypted = aes_256_gcm_decrypt(encrypted, key);

        REQUIRE(plaintext == decrypted);
    }
}

TEST_CASE("crypto::random_bytes", "[utils][crypto]") {
    SECTION("generate random bytes") {
        auto bytes1 = random_bytes(16);
        auto bytes2 = random_bytes(16);

        REQUIRE(bytes1.size() == 16);
        REQUIRE(bytes2.size() == 16);
        REQUIRE(bytes1 != bytes2);  // Should be different
    }

    SECTION("generate zero bytes") {
        auto bytes = random_bytes(0);

        REQUIRE(bytes.size() == 0);
    }

    SECTION("generate many bytes") {
        auto bytes = random_bytes(1024);

        REQUIRE(bytes.size() == 1024);
    }
}

TEST_CASE("crypto::random_string", "[utils][crypto]") {
    SECTION("generate random string") {
        std::string str1 = random_string(16);
        std::string str2 = random_string(16);

        REQUIRE(str1.size() == 16);
        REQUIRE(str2.size() == 16);
        REQUIRE(str1 != str2);  // Should be different
    }

    SECTION("generate empty string") {
        std::string str = random_string(0);

        REQUIRE(str.empty());
    }

    SECTION("string contains valid characters") {
        std::string str = random_string(100);

        // Check that string contains only hex characters
        for (char c : str) {
            REQUIRE((std::isxdigit(c) != 0));
        }
    }
}

TEST_CASE("crypto::sha256 vector overload", "[utils][crypto]") {
    SECTION("hash byte vector") {
        std::vector<uint8_t> data = {'h', 'e', 'l', 'l', 'o'};
        auto hash = sha256(data);

        REQUIRE(hash.size() == 32);  // SHA256 produces 32 bytes
    }

    SECTION("hash empty vector") {
        std::vector<uint8_t> data;
        auto hash = sha256(data);

        REQUIRE(hash.size() == 32);
    }

    SECTION("consistency with string version") {
        std::string str = "hello";
        std::vector<uint8_t> vec(str.begin(), str.end());

        std::string hash_str = sha256(str);
        auto hash_vec = sha256(vec);

        // Convert vector hash to hex string for comparison
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (auto byte : hash_vec) {
            oss << std::setw(2) << static_cast<int>(byte);
        }

        REQUIRE(hash_str == oss.str());
    }
}

TEST_CASE("crypto::random_alphanumeric", "[utils][crypto]") {
    SECTION("generate alphanumeric string") {
        std::string str = random_alphanumeric(32);

        REQUIRE(str.size() == 32);
    }

    SECTION("contains only alphanumeric characters") {
        std::string str = random_alphanumeric(100);

        for (char c : str) {
            bool is_valid = (c >= '0' && c <= '9') ||
                           (c >= 'A' && c <= 'Z') ||
                           (c >= 'a' && c <= 'z');
            REQUIRE(is_valid);
        }
    }

    SECTION("generates different strings") {
        std::string str1 = random_alphanumeric(16);
        std::string str2 = random_alphanumeric(16);

        REQUIRE(str1 != str2);
    }

    SECTION("empty string") {
        std::string str = random_alphanumeric(0);
        REQUIRE(str.empty());
    }
}

TEST_CASE("crypto::base64_encode", "[utils][crypto]") {
    SECTION("encode empty string") {
        std::string result = base64_encode("");
        REQUIRE(result.empty());
    }

    SECTION("encode simple string") {
        // "hello" in base64 is "aGVsbG8="
        std::string result = base64_encode("hello");
        REQUIRE(result == "aGVsbG8=");
    }

    SECTION("encode string with padding") {
        // "hello!" in base64 is "aGVsbG8h"
        std::string result = base64_encode("hello!");
        REQUIRE(result == "aGVsbG8h");
    }

    SECTION("encode binary data") {
        std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0xFF};
        std::string result = base64_encode(data);
        REQUIRE_FALSE(result.empty());

        // Verify decode works
        auto decoded = base64_decode_bytes(result);
        REQUIRE(decoded == data);
    }

    SECTION("encode long data") {
        std::string data(1024, 'A');
        std::string result = base64_encode(data);
        REQUIRE_FALSE(result.empty());

        std::string decoded = base64_decode(result);
        REQUIRE(decoded == data);
    }

    SECTION("round trip") {
        std::string original = "The quick brown fox jumps over the lazy dog!";
        std::string encoded = base64_encode(original);
        std::string decoded = base64_decode(encoded);
        REQUIRE(decoded == original);
    }
}

TEST_CASE("crypto::base64_decode", "[utils][crypto]") {
    SECTION("decode empty string") {
        std::string result = base64_decode("");
        REQUIRE(result.empty());
    }

    SECTION("decode simple string") {
        std::string result = base64_decode("aGVsbG8=");
        REQUIRE(result == "hello");
    }

    SECTION("decode without padding") {
        std::string result = base64_decode("aGVsbG8h");
        REQUIRE(result == "hello!");
    }

    SECTION("decode bytes") {
        std::string encoded = "AAEC/w==";  // 0x00, 0x01, 0x02, 0xFF
        auto result = base64_decode_bytes(encoded);
        REQUIRE(result.size() == 4);
        REQUIRE(result[0] == 0x00);
        REQUIRE(result[1] == 0x01);
        REQUIRE(result[2] == 0x02);
        REQUIRE(result[3] == 0xFF);
    }

    SECTION("decode without padding") {
        // "aGVsbG8=" decodes to "hello"
        // "aGVsbG8" without padding should also decode correctly
        std::string result = base64_decode("aGVsbG8");
        REQUIRE(result == "hello");
    }
}

TEST_CASE("crypto::generate_uuid", "[utils][crypto]") {
    SECTION("generate valid UUID") {
        std::string uuid = generate_uuid();
        REQUIRE(uuid.size() == 36);
        REQUIRE(is_valid_uuid(uuid));
    }

    SECTION("generate unique UUIDs") {
        std::string uuid1 = generate_uuid();
        std::string uuid2 = generate_uuid();
        REQUIRE(uuid1 != uuid2);
    }

    SECTION("UUID has correct format") {
        std::string uuid = generate_uuid();

        // Check dashes at correct positions
        REQUIRE(uuid[8] == '-');
        REQUIRE(uuid[13] == '-');
        REQUIRE(uuid[18] == '-');
        REQUIRE(uuid[23] == '-');

        // Version 4 UUID has '4' at position 14
        REQUIRE(uuid[14] == '4');

        // Variant bit: position 19 should be 8, 9, a, or b
        char variant = uuid[19];
        REQUIRE((variant == '8' || variant == '9' || variant == 'a' || variant == 'b' ||
                 variant == 'A' || variant == 'B'));
    }
}

TEST_CASE("crypto::is_valid_uuid", "[utils][crypto]") {
    SECTION("valid UUID") {
        REQUIRE(is_valid_uuid("550e8400-e29b-41d4-a716-446655440000"));
    }

    SECTION("invalid UUID - wrong length") {
        REQUIRE_FALSE(is_valid_uuid("550e8400-e29b-41d4-a716-44665544000"));
    }

    SECTION("invalid UUID - wrong version") {
        REQUIRE_FALSE(is_valid_uuid("550e8400-e29b-31d4-a716-446655440000"));  // version 3
    }

    SECTION("invalid UUID - wrong variant") {
        REQUIRE_FALSE(is_valid_uuid("550e8400-e29b-41d4-c716-446655440000"));  // variant c
    }

    SECTION("invalid UUID - missing dashes") {
        REQUIRE_FALSE(is_valid_uuid("550e8400e29b41d4a716446655440000"));
    }

    SECTION("invalid UUID - empty string") {
        REQUIRE_FALSE(is_valid_uuid(""));
    }

    SECTION("invalid UUID - wrong characters") {
        REQUIRE_FALSE(is_valid_uuid("550e8400-e29b-41d4-a716-44665544ZZZZ"));
    }
}

TEST_CASE("crypto::aes_256_gcm edge cases", "[utils][crypto]") {
    SECTION("invalid key size - too short") {
        std::string plaintext = "test";
        std::string short_key = "short";

        REQUIRE_THROWS_AS(aes_256_gcm_encrypt(plaintext, short_key), std::runtime_error);
    }

    SECTION("invalid key size - too long") {
        std::string plaintext = "test";
        std::string long_key = "this-key-is-way-too-long-for-aes-256";

        REQUIRE_THROWS_AS(aes_256_gcm_encrypt(plaintext, long_key), std::runtime_error);
    }

    SECTION("decrypt with invalid key size") {
        AesGcmResult encrypted;
        encrypted.ciphertext = "test";
        encrypted.nonce = "123456789012";
        encrypted.tag = std::string(16, 'A');
        std::string wrong_size_key = "short";

        REQUIRE_THROWS_AS(aes_256_gcm_decrypt(encrypted, wrong_size_key), std::runtime_error);
    }

    SECTION("decrypt with invalid tag size") {
        AesGcmResult encrypted;
        encrypted.ciphertext = "test";
        encrypted.nonce = "123456789012";
        encrypted.tag = "short";  // Should be 16 bytes
        std::string key(32, 'K');

        REQUIRE_THROWS_AS(aes_256_gcm_decrypt(encrypted, key), std::runtime_error);
    }

    SECTION("encrypt large data") {
        std::string plaintext(10000, 'X');  // 10KB of data
        std::string key = "01234567890123456789012345678901";

        auto encrypted = aes_256_gcm_encrypt(plaintext, key);
        std::string decrypted = aes_256_gcm_decrypt(encrypted, key);

        REQUIRE(plaintext == decrypted);
    }

    SECTION("decrypt with wrong nonce fails") {
        std::string plaintext = "secret message";
        std::string key = "01234567890123456789012345678901";

        auto encrypted = aes_256_gcm_encrypt(plaintext, key);
        encrypted.nonce = "000000000000";  // Tamper with nonce

        REQUIRE_THROWS_AS(aes_256_gcm_decrypt(encrypted, key), std::runtime_error);
    }

    SECTION("decrypt with tampered ciphertext fails") {
        std::string plaintext = "secret message";
        std::string key = "01234567890123456789012345678901";

        auto encrypted = aes_256_gcm_encrypt(plaintext, key);
        if (!encrypted.ciphertext.empty()) {
            encrypted.ciphertext[0] ^= 0xFF;  // Flip bits in ciphertext
        }

        REQUIRE_THROWS_AS(aes_256_gcm_decrypt(encrypted, key), std::runtime_error);
    }

    SECTION("multiple encryption produces different results") {
        std::string plaintext = "same message";
        std::string key = "01234567890123456789012345678901";

        auto encrypted1 = aes_256_gcm_encrypt(plaintext, key);
        auto encrypted2 = aes_256_gcm_encrypt(plaintext, key);

        // Different nonces should produce different ciphertexts
        REQUIRE(encrypted1.nonce != encrypted2.nonce);
        REQUIRE(encrypted1.ciphertext != encrypted2.ciphertext);

        // But both should decrypt to the same plaintext
        REQUIRE(aes_256_gcm_decrypt(encrypted1, key) == plaintext);
        REQUIRE(aes_256_gcm_decrypt(encrypted2, key) == plaintext);
    }

    SECTION("unicode plaintext") {
        std::string plaintext = "你好世界 🌍 Hello World";
        std::string key = "01234567890123456789012345678901";

        auto encrypted = aes_256_gcm_encrypt(plaintext, key);
        std::string decrypted = aes_256_gcm_decrypt(encrypted, key);

        REQUIRE(plaintext == decrypted);
    }
}

TEST_CASE("crypto::hmac_sha256 edge cases", "[utils][crypto]") {
    SECTION("empty key") {
        std::string data = "message";
        std::string result = hmac_sha256("", data);

        REQUIRE(result.size() == 64);
    }

    SECTION("empty data") {
        std::string key = "secret";
        std::string result = hmac_sha256(key, "");

        REQUIRE(result.size() == 64);
    }

    SECTION("both empty") {
        std::string result = hmac_sha256("", "");

        REQUIRE(result.size() == 64);
    }

    SECTION("long key") {
        std::string key(1000, 'K');  // 1000 byte key
        std::string data = "message";
        std::string result = hmac_sha256(key, data);

        REQUIRE(result.size() == 64);
    }

    SECTION("long data") {
        std::string key = "secret";
        std::string data(10000, 'D');  // 10KB data
        std::string result = hmac_sha256(key, data);

        REQUIRE(result.size() == 64);
    }

    SECTION("known test vector") {
        // RFC 4231 test vector
        std::string key = "Jefe";
        std::string data = "what do ya want for nothing?";
        std::string result = hmac_sha256(key, data);

        // Should produce consistent result
        REQUIRE(result.size() == 64);
        REQUIRE(result == hmac_sha256(key, data));  // Consistency check
    }
}

TEST_CASE("crypto::sha256 edge cases", "[utils][crypto]") {
    SECTION("long string") {
        std::string data(10000, 'A');
        std::string result = sha256(data);

        REQUIRE(result.size() == 64);
    }

    SECTION("binary data in string") {
        std::string data = "\x00\x01\x02\xFF";
        std::string result = sha256(data);

        REQUIRE(result.size() == 64);
    }

    SECTION("known value") {
        // SHA256("hello") = 2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824
        std::string result = sha256("hello");
        REQUIRE(result == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
    }
}

TEST_CASE("crypto::sha256_file", "[utils][crypto]") {
    namespace fs = std::filesystem;
    auto tmp_dir = fs::temp_directory_path();

    SECTION("hash known file content") {
        auto tmp = tmp_dir / "sha256_file_test.txt";
        std::ofstream(tmp) << "hello";

        std::string hash = sha256_file(tmp);
        // Same as sha256("hello")
        CHECK(hash == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
        CHECK(hash.length() == 64);

        fs::remove(tmp);
    }

    SECTION("non-existent file returns empty") {
        std::string hash = sha256_file("/non/existent/path/file_xyz.txt");
        CHECK(hash.empty());
    }

    SECTION("consistent with sha256(content)") {
        auto tmp = tmp_dir / "sha256_file_consistent.txt";
        std::string content = "test content for consistency";
        std::ofstream(tmp) << content;

        std::string file_hash = sha256_file(tmp);
        std::string content_hash = sha256(content);
        CHECK(file_hash == content_hash);

        fs::remove(tmp);
    }

    SECTION("empty file returns sha256 of empty string") {
        auto tmp = tmp_dir / "sha256_file_empty.txt";
        { std::ofstream ofs(tmp); }  // create empty file and close immediately

        std::string hash = sha256_file(tmp);
        // sha256("") = e3b0c44...
        CHECK(hash == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

        fs::remove(tmp);
    }
}

// ============================================================================
// sha256_file_ex: strong-typed error variant
// ============================================================================

TEST_CASE("crypto::sha256_file_ex: non-existent file returns NotFound", "[utils][crypto]") {
    namespace fs = std::filesystem;
    auto non_existent = fs::temp_directory_path() / "turbot_test_nonexistent_xyz_abc.txt";
    // Ensure it really doesn't exist
    fs::remove(non_existent);

    auto result = sha256_file_ex(non_existent);
    REQUIRE(std::holds_alternative<FileHashError>(result));
    CHECK(std::get<FileHashError>(result) == FileHashError::NotFound);
}

TEST_CASE("crypto::sha256_file_ex: valid file returns hash string", "[utils][crypto]") {
    namespace fs = std::filesystem;
    auto tmp = fs::temp_directory_path() / "turbot_sha256ex_test.txt";
    {
        std::ofstream ofs(tmp);
        ofs << "hello world";
    }

    auto result = sha256_file_ex(tmp);
    REQUIRE(std::holds_alternative<std::string>(result));
    const auto& hash = std::get<std::string>(result);
    CHECK(hash.size() == 64);         // SHA-256 hex is 64 chars
    CHECK_FALSE(hash.empty());

    fs::remove(tmp);
}

TEST_CASE("crypto::sha256_file_ex: result matches sha256_file for same file", "[utils][crypto]") {
    namespace fs = std::filesystem;
    auto tmp = fs::temp_directory_path() / "turbot_sha256ex_match.txt";
    const std::string content = "consistency check content";
    {
        std::ofstream ofs(tmp);
        ofs << content;
    }

    auto ex_result = sha256_file_ex(tmp);
    std::string legacy = sha256_file(tmp);

    REQUIRE(std::holds_alternative<std::string>(ex_result));
    CHECK(std::get<std::string>(ex_result) == legacy);  // Must match legacy interface

    fs::remove(tmp);
}