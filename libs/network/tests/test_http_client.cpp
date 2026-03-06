#include <catch2/catch_test_macros.hpp>
#include <turbot/network/http_client.hpp>

using namespace turbot::network;

TEST_CASE("HttpClient can be constructed", "[http_client]") {
    REQUIRE_NOTHROW(HttpClient());
}

TEST_CASE("HttpClient can make GET request", "[http_client]") {
    HttpClient client;
    auto response = client.get("https://api.example.com/test");

    REQUIRE(response.status_code == 200);
    REQUIRE(!response.body.empty());
}

TEST_CASE("HttpClient can make POST request", "[http_client]") {
    HttpClient client;
    auto response = client.post("https://api.example.com/test", R"({"data": "test"})");

    REQUIRE(response.status_code == 201);
    REQUIRE(!response.body.empty());
}

TEST_CASE("HttpClient timeout can be set", "[http_client]") {
    HttpClient client;
    REQUIRE_NOTHROW(client.set_timeout(60));
}
