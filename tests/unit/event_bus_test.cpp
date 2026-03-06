#include <catch2/catch_test_macros.hpp>
#include <turbot/core/event_bus.hpp>
#include <chrono>
#include <thread>

using namespace turbot::core;

TEST_CASE("EventBus::instance", "[core][event]") {
    SECTION("singleton pattern") {
        auto& bus1 = EventBus::instance();
        auto& bus2 = EventBus::instance();

        REQUIRE(&bus1 == &bus2);
    }
}

TEST_CASE("EventBus::publish and subscribe", "[core][event]") {
    auto& bus = EventBus::instance();

    SECTION("basic publish and subscribe") {
        bool called = false;
        std::string received_data;

        auto handler = [&called, &received_data](const Event<std::string>& event) {
            called = true;
            received_data = event.data;
        };

        bus.subscribe<std::string>("test.event", handler);
        bus.publish("test.event", std::string("test data"));

        REQUIRE(called);
        REQUIRE(received_data == "test data");
    }

    SECTION("multiple subscribers") {
        int call_count = 0;

        auto handler = [&call_count](const Event<int>& event) {
            call_count += event.data;
        };

        bus.subscribe<int>("test.multi", handler);
        bus.subscribe<int>("test.multi", handler);

        bus.publish("test.multi", 5);

        REQUIRE(call_count == 10);  // 5 + 5
    }

    SECTION("publish to non-existing event") {
        // Should not throw
        REQUIRE_NOTHROW(bus.publish("non.existing.event", std::string("data")));
    }
}

TEST_CASE("EventBus::unsubscribe", "[core][event]") {
    auto& bus = EventBus::instance();

    SECTION("unsubscribe removes handler") {
        int call_count = 0;

        auto handler = [&call_count](const Event<int>& event) {
            call_count += event.data;
        };

        std::string handler_id = bus.subscribe<int>("test.unsubscribe", handler);

        // First publish
        bus.publish("test.unsubscribe", 5);
        REQUIRE(call_count == 5);

        // Unsubscribe
        bus.unsubscribe("test.unsubscribe", handler_id);

        // Second publish - should not call handler
        bus.publish("test.unsubscribe", 5);
        REQUIRE(call_count == 5);  // Still 5, not 10
    }

    SECTION("unsubscribe non-existing handler") {
        // Should not throw
        REQUIRE_NOTHROW(bus.unsubscribe("test.event", "non_existing_id"));
    }
}

TEST_CASE("EventBus::subscriber_count", "[core][event]") {
    auto& bus = EventBus::instance();

    SECTION("count subscribers") {
        std::string event_name = "test.count";

        REQUIRE(bus.subscriber_count(event_name) == 0);

        auto handler = [](const Event<int>&) {};
        bus.subscribe<int>(event_name, handler);

        REQUIRE(bus.subscriber_count(event_name) == 1);

        bus.subscribe<int>(event_name, handler);

        REQUIRE(bus.subscriber_count(event_name) == 2);
    }

    SECTION("count non-existing event") {
        REQUIRE(bus.subscriber_count("non.existing.event") == 0);
    }
}

TEST_CASE("EventBus::list_events", "[core][event]") {
    auto& bus = EventBus::instance();

    SECTION("list all events") {
        bus.clear();  // Clear any existing subscriptions

        auto handler = [](const Event<int>&) {};

        bus.subscribe<int>("event1", handler);
        bus.subscribe<int>("event2", handler);
        bus.subscribe<int>("event3", handler);

        auto events = bus.list_events();

        REQUIRE(events.size() == 3);

        // Should be sorted
        REQUIRE(events[0] == "event1");
        REQUIRE(events[1] == "event2");
        REQUIRE(events[2] == "event3");
    }

    SECTION("empty list") {
        bus.clear();

        auto events = bus.list_events();

        REQUIRE(events.empty());
    }
}

TEST_CASE("EventBus::clear", "[core][event]") {
    auto& bus = EventBus::instance();

    SECTION("clear all subscriptions") {
        auto handler = [](const Event<int>&) {};

        bus.subscribe("event1", handler);
        bus.subscribe("event2", handler);

        REQUIRE(bus.subscriber_count("event1") == 1);
        REQUIRE(bus.subscriber_count("event2") == 1);

        bus.clear();

        REQUIRE(bus.subscriber_count("event1") == 0);
        REQUIRE(bus.subscriber_count("event2") == 0);
        REQUIRE(bus.list_events().empty());
    }
}

TEST_CASE("EventBus::event data", "[core][event]") {
    auto& bus = EventBus::instance();

    SECTION("event has timestamp") {
        int64_t received_timestamp = 0;

        auto handler = [&received_timestamp](const Event<int>& event) {
            received_timestamp = event.timestamp;
        };

        bus.subscribe("test.timestamp", handler);
        bus.publish("test.timestamp", 42);

        REQUIRE(received_timestamp > 0);
    }

    SECTION("event has source") {
        std::string received_source;

        auto handler = [&received_source](const Event<int>& event) {
            received_source = event.source;
        };

        bus.subscribe("test.source", handler);
        bus.publish("test.source", 42, "test_source");

        REQUIRE(received_source == "test_source");
    }

    SECTION("event has default source") {
        std::string received_source;

        auto handler = [&received_source](const Event<int>& event) {
            received_source = event.source;
        };

        bus.subscribe("test.default_source", handler);
        bus.publish("test.default_source", 42);

        REQUIRE(received_source == "turbot");
    }
}

TEST_CASE("EventBus::complex data types", "[core][event]") {
    auto& bus = EventBus::instance();

    SECTION("event with struct data") {
        struct TestData {
            int value;
            std::string text;
        };

        TestData received;

        auto handler = [&received](const Event<TestData>& event) {
            received = event.data;
        };

        TestData data{42, "test"};
        bus.subscribe("test.struct", handler);
        bus.publish("test.struct", data);

        REQUIRE(received.value == 42);
        REQUIRE(received.text == "test");
    }

    SECTION("event with JSON data") {
        nlohmann::json received;

        auto handler = [&received](const Event<nlohmann::json>& event) {
            received = event.data;
        };

        nlohmann::json data = {
            {"key1", "value1"},
            {"key2", 42}
        };

        bus.subscribe("test.json", handler);
        bus.publish("test.json", data);

        REQUIRE(received["key1"] == "value1");
        REQUIRE(received["key2"] == 42);
    }
}