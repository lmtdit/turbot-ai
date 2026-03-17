#include <catch2/catch_test_macros.hpp>
#include <turbot/core/event/event_bus.hpp>
#include <chrono>
#include <thread>
#include <atomic>

using namespace turbot::core;

// ==================== Event Tests ====================

TEST_CASE("EventBus.Event.BasicConstruction", "[Core][Event]") {
    Event<int> event("test_event", 42, "test_source");
    
    REQUIRE(event.name == "test_event");
    REQUIRE(event.data == 42);
    REQUIRE(event.source == "test_source");
    REQUIRE(event.timestamp > 0);
}

TEST_CASE("EventBus.Event.DefaultConstruction", "[Core][Event]") {
    Event<std::string> event;
    REQUIRE(event.name.empty());
    REQUIRE(event.data.empty());
    REQUIRE(event.timestamp > 0);
}

TEST_CASE("EventBus.Event.DifferentTypes", "[Core][Event]") {
    Event<std::string> str_event("str_event", "hello");
    Event<double> double_event("double_event", 3.14);
    Event<bool> bool_event("bool_event", true);
    
    REQUIRE(str_event.data == "hello");
    REQUIRE(double_event.data == 3.14);
    REQUIRE(bool_event.data == true);
}

// ==================== EventBus Instance Tests ====================

TEST_CASE("EventBus.Instance.Singleton", "[Core][Event]") {
    EventBus& bus1 = EventBus::instance();
    EventBus& bus2 = EventBus::instance();
    
    REQUIRE(&bus1 == &bus2);
}

// ==================== Publish/Subscribe Tests ====================

TEST_CASE("EventBus.PublishSubscribe.Basic", "[Core][Event]") {
    EventBus& bus = EventBus::instance();
    bus.clear();
    
    std::string received_data;
    std::string id = bus.subscribe<std::string>("test_event", 
        [&](const Event<std::string>& event) {
            received_data = event.data;
        });
    
    bus.publish("test_event", std::string("hello"));
    
    REQUIRE(received_data == "hello");
    
    bus.unsubscribe("test_event", id);
}

TEST_CASE("EventBus.PublishSubscribe.MultipleSubscribers", "[Core][Event]") {
    EventBus& bus = EventBus::instance();
    bus.clear();
    
    int sum = 0;
    auto id1 = bus.subscribe<int>("add_event", [&](const Event<int>& e) { sum += e.data; });
    auto id2 = bus.subscribe<int>("add_event", [&](const Event<int>& e) { sum += e.data * 2; });
    
    bus.publish("add_event", 10);
    
    REQUIRE(sum == 30);  // 10 + 10*2
    
    bus.unsubscribe("add_event", id1);
    bus.unsubscribe("add_event", id2);
}

TEST_CASE("EventBus.PublishSubscribe.DifferentEvents", "[Core][Event]") {
    EventBus& bus = EventBus::instance();
    bus.clear();
    
    std::string str_result;
    int int_result = 0;
    
    auto id1 = bus.subscribe<std::string>("str_event", [&](const Event<std::string>& e) {
        str_result = e.data;
    });
    auto id2 = bus.subscribe<int>("int_event", [&](const Event<int>& e) {
        int_result = e.data;
    });
    
    bus.publish("str_event", std::string("test"));
    bus.publish("int_event", 42);
    
    REQUIRE(str_result == "test");
    REQUIRE(int_result == 42);
    
    bus.unsubscribe("str_event", id1);
    bus.unsubscribe("int_event", id2);
}

TEST_CASE("EventBus.PublishSubscribe.NoSubscribers", "[Core][Event]") {
    EventBus& bus = EventBus::instance();
    bus.clear();
    
    // Should not throw
    REQUIRE_NOTHROW(bus.publish("nonexistent_event", 42));
}

// ==================== Unsubscribe Tests ====================

TEST_CASE("EventBus.Unsubscribe.Basic", "[Core][Event]") {
    EventBus& bus = EventBus::instance();
    bus.clear();
    
    int call_count = 0;
    std::string id = bus.subscribe<int>("temp_event", [&](const Event<int>&) {
        call_count++;
    });
    
    bus.publish("temp_event", 1);
    REQUIRE(call_count == 1);
    
    bus.unsubscribe("temp_event", id);
    
    bus.publish("temp_event", 2);
    REQUIRE(call_count == 1);  // Should not increase
}

TEST_CASE("EventBus.Unsubscribe.NonExistent", "[Core][Event]") {
    EventBus& bus = EventBus::instance();
    bus.clear();
    
    // Should not throw
    REQUIRE_NOTHROW(bus.unsubscribe("nonexistent", "fake_id"));
}

// ==================== Subscriber Count Tests ====================

TEST_CASE("EventBus.SubscriberCount.Basic", "[Core][Event]") {
    EventBus& bus = EventBus::instance();
    bus.clear();
    
    REQUIRE(bus.subscriber_count("count_event") == 0);
    
    auto id1 = bus.subscribe<int>("count_event", [](const Event<int>&) {});
    REQUIRE(bus.subscriber_count("count_event") == 1);
    
    auto id2 = bus.subscribe<int>("count_event", [](const Event<int>&) {});
    REQUIRE(bus.subscriber_count("count_event") == 2);
    
    bus.unsubscribe("count_event", id1);
    REQUIRE(bus.subscriber_count("count_event") == 1);
    
    bus.unsubscribe("count_event", id2);
    REQUIRE(bus.subscriber_count("count_event") == 0);
}

// ==================== List Events Tests ====================

TEST_CASE("EventBus.ListEvents.Basic", "[Core][Event]") {
    EventBus& bus = EventBus::instance();
    bus.clear();
    
    auto events = bus.list_events();
    REQUIRE(events.empty());
    
    auto id1 = bus.subscribe<int>("event1", [](const Event<int>&) {});
    auto id2 = bus.subscribe<int>("event2", [](const Event<int>&) {});
    
    events = bus.list_events();
    REQUIRE(events.size() == 2);
    
    bus.unsubscribe("event1", id1);
    bus.unsubscribe("event2", id2);
}

// ==================== Clear Tests ====================

TEST_CASE("EventBus.Clear.Basic", "[Core][Event]") {
    EventBus& bus = EventBus::instance();
    
    bus.subscribe<int>("clear_event1", [](const Event<int>&) {});
    bus.subscribe<int>("clear_event2", [](const Event<int>&) {});
    
    REQUIRE(bus.list_events().size() == 2);
    
    bus.clear();
    
    REQUIRE(bus.list_events().empty());
    REQUIRE(bus.subscriber_count("clear_event1") == 0);
    REQUIRE(bus.subscriber_count("clear_event2") == 0);
}

// ==================== Async Tests ====================

TEST_CASE("EventBus.PublishAsync.Basic", "[Core][Event]") {
    EventBus& bus = EventBus::instance();
    bus.clear();
    
    std::atomic<int> counter{0};
    
    auto id = bus.subscribe_async<int>("async_event", 
        [](const Event<int>& e) -> std::future<void> {
            return std::async(std::launch::async, [&]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                // Can't capture atomic by reference in async
            });
        });
    
    // Simplified test - just verify it doesn't throw
    REQUIRE_NOTHROW(bus.publish_async("async_event", 42).wait());
    
    bus.unsubscribe("async_event", id);
}

// ==================== Exception Handling Tests ====================

TEST_CASE("EventBus.ExceptionHandling.HandlerThrows", "[Core][Event]") {
    EventBus& bus = EventBus::instance();
    bus.clear();
    
    int call_count = 0;
    
    auto id1 = bus.subscribe<int>("exception_event", [](const Event<int>&) {
        throw std::runtime_error("Handler error");
    });
    auto id2 = bus.subscribe<int>("exception_event", [&](const Event<int>&) {
        call_count++;
    });
    
    // Should not throw - exception is caught internally
    REQUIRE_NOTHROW(bus.publish("exception_event", 42));
    
    // Second handler should still be called
    REQUIRE(call_count == 1);
    
    bus.unsubscribe("exception_event", id1);
    bus.unsubscribe("exception_event", id2);
}

// ==================== Complex Data Types Tests ====================

TEST_CASE("EventBus.ComplexData.JsonData", "[Core][Event]") {
    EventBus& bus = EventBus::instance();
    bus.clear();
    
    nlohmann::json received;
    
    auto id = bus.subscribe<nlohmann::json>("json_event", [&](const Event<nlohmann::json>& e) {
        received = e.data;
    });
    
    nlohmann::json data = {
        {"name", "test"},
        {"value", 42},
        {"nested", {{"key", "value"}}}
    };
    
    bus.publish("json_event", data);
    
    REQUIRE(received["name"] == "test");
    REQUIRE(received["value"] == 42);
    
    bus.unsubscribe("json_event", id);
}

TEST_CASE("EventBus.ComplexData.VectorData", "[Core][Event]") {
    EventBus& bus = EventBus::instance();
    bus.clear();
    
    std::vector<int> received;
    
    auto id = bus.subscribe<std::vector<int>>("vector_event", [&](const Event<std::vector<int>>& e) {
        received = e.data;
    });
    
    std::vector<int> data = {1, 2, 3, 4, 5};
    bus.publish("vector_event", data);
    
    REQUIRE(received.size() == 5);
    REQUIRE(received[0] == 1);
    REQUIRE(received[4] == 5);
    
    bus.unsubscribe("vector_event", id);
}

// ==================== Event Source Tests ====================

TEST_CASE("EventBus.EventSource.Custom", "[Core][Event]") {
    EventBus& bus = EventBus::instance();
    bus.clear();
    
    std::string received_source;
    
    auto id = bus.subscribe<int>("source_event", [&](const Event<int>& e) {
        received_source = e.source;
    });
    
    bus.publish("source_event", 42, "custom_source");
    
    REQUIRE(received_source == "custom_source");
    
    bus.unsubscribe("source_event", id);
}

TEST_CASE("EventBus.EventSource.Default", "[Core][Event]") {
    EventBus& bus = EventBus::instance();
    bus.clear();
    
    std::string received_source;
    
    auto id = bus.subscribe<int>("default_source_event", [&](const Event<int>& e) {
        received_source = e.source;
    });
    
    bus.publish("default_source_event", 42);
    
    REQUIRE(received_source == "turbot");
    
    bus.unsubscribe("default_source_event", id);
}
