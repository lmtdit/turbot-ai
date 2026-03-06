#include <catch2/catch_test_macros.hpp>
#include <turbot/network/http_client.hpp>
#include <future>

using namespace turbot::network;

TEST_CASE("HttpClient::get", "[network][http]") {
    HttpClient client;

    SECTION("sync get request") {
        auto response = client.get("https://example.com/api");

        REQUIRE(response.status_code == 200);
        REQUIRE_FALSE(response.body.empty());
        REQUIRE(response.content_type == "application/json");
    }
}

TEST_CASE("HttpClient::post", "[network][http]") {
    HttpClient client;

    SECTION("sync post request") {
        auto response = client.post("https://example.com/api", R"({"key": "value"})");

        REQUIRE(response.status_code == 201);
        REQUIRE_FALSE(response.body.empty());
    }
}

TEST_CASE("HttpClient::async_requests", "[network][http]") {
    HttpClient client;

    SECTION("async get request") {
        auto future = client.get_async("https://example.com/api");

        REQUIRE(future.valid());

        auto response = future.get();
        REQUIRE(response.status_code == 200);
    }

    SECTION("async post request") {
        auto future = client.post_async("https://example.com/api", R"({"test": true})");

        REQUIRE(future.valid());

        auto response = future.get();
        REQUIRE(response.status_code == 201);
    }
}

TEST_CASE("HttpClient::set_timeout", "[network][http]") {
    HttpClient client;

    SECTION("set timeout") {
        REQUIRE_NOTHROW(client.set_timeout(60));
        REQUIRE_NOTHROW(client.set_timeout(5));
    }
}

TEST_CASE("HttpClient::move_semantics", "[network][http]") {
    SECTION("move constructor") {
        HttpClient client1;
        HttpClient client2 = std::move(client1);

        auto response = client2.get("https://example.com/api");
        REQUIRE(response.status_code == 200);
    }

    SECTION("move assignment") {
        HttpClient client1;
        HttpClient client2;

        client2 = std::move(client1);

        auto response = client2.get("https://example.com/api");
        REQUIRE(response.status_code == 200);
    }
}
