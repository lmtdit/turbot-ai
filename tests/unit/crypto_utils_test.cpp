#include <catch2/catch_test_macros.hpp>
#include <turbot/utils/crypto_utils.hpp>
#include <algorithm>

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
        std::string decrypted = aes_256_gcm_decrypt(encrypted, wrong_key);

        // Decryption should fail or produce different result
        REQUIRE(decrypted != plaintext);
    }

    SECTION("decrypt with wrong tag fails") {
        std::string plaintext = "secret message";
        std::string key = "01234567890123456789012345678901";

        auto encrypted = aes_256_gcm_encrypt(plaintext, key);
        encrypted.tag = "wrongtag";  // Tamper with the tag

        std::string decrypted = aes_256_gcm_decrypt(encrypted, key);

        // Decryption should fail or produce different result
        REQUIRE(decrypted != plaintext);
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

        // Check that string contains only alphanumeric characters
        for (char c : str) {
            REQUIRE((std::isalnum(c) != 0));
        }
    }
}