#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/string_utils.hpp>
#include <nlohmann/json.hpp>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <any>

namespace turbot::core {

/**
 * @brief 事件数据基类
 */
struct TURBOT_CORE_API EventData {
    virtual ~EventData() = default;
    virtual std::string to_json() const = 0;
};

/**
 * @brief 通用事件
 * @tparam T 事件数据类型
 */
template<typename T>
struct Event {
    std::string name;
    T data;
    int64_t timestamp;
    std::string source;

    Event()
        : timestamp(std::chrono::system_clock::now().time_since_epoch().count()) {}

    Event(std::string name, T data, std::string source = "turbot")
        : name(std::move(name))
        , data(std::move(data))
        , timestamp(std::chrono::system_clock::now().time_since_epoch().count())
        , source(std::move(source)) {}
};

/**
 * @brief 事件处理器类型
 * @tparam T 事件数据类型
 */
template<typename T>
using EventHandler = std::function<void(const Event<T>&)>;

/**
 * @brief 异步事件处理器类型
 * @tparam T 事件数据类型
 */
template<typename T>
using AsyncEventHandler = std::function<std::future<void>(const Event<T>&)>;

/**
 * @brief 事件总线
 *
 * 提供发布-订阅模式的事件系统，支持同步和异步事件处理。
 */
class TURBOT_CORE_API EventBus {
public:
    /**
     * @brief 获取EventBus单例实例
     * @return EventBus实例引用
     */
    static EventBus& instance() noexcept;

    /**
     * @brief 发布事件（同步）
     * @tparam T 事件数据类型
     * @param name 事件名称
     * @param data 事件数据
     * @param source 事件源
     */
    template<typename T>
    void publish(const std::string& name, T data, const std::string& source = "turbot") {
        Event<T> event(name, std::move(data), source);

        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = handlers_.find(name);
        if (it != handlers_.end()) {
            for (auto& entry : it->second) {
                try {
                    // 执行处理器 - 使用简化的类型检查
                    auto* handler = std::any_cast<EventHandler<T>>(&entry.handler);
                    if (handler) {
                        (*handler)(event);
                    }
                } catch (const std::exception& e) {
                    // Log the error but continue delivering to other handlers
                    TURBOT_LOG_ERROR("EventBus: subscriber threw exception for event '{}': {}",
                                    name, e.what());
                }
            }
        }
    }

    /**
     * @brief 发布事件（异步）
     * @tparam T 事件数据类型
     * @param name 事件名称
     * @param data 事件数据
     * @param source 事件源
     * @return std::future<void> 可用于等待任务完成
     */
    template<typename T>
    std::future<void> publish_async(const std::string& name, T data, const std::string& source = "turbot") {
        Event<T> event(name, std::move(data), source);

        // 使用 std::async 代替 std::thread().detach() 避免资源泄漏
        return std::async(std::launch::async, [this, event]() mutable {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = handlers_.find(event.name);
            if (it != handlers_.end()) {
                for (auto& entry : it->second) {
                    try {
                        auto* handler = std::any_cast<AsyncEventHandler<T>>(&entry.handler);
                        if (handler) {
                            (*handler)(event).wait();
                        }
                    } catch (const std::exception& e) {
                        // Log the error but continue delivering to other handlers
                        TURBOT_LOG_ERROR("EventBus: async subscriber threw exception for event '{}': {}",
                                        event.name, e.what());
                    }
                }
            }
        });
    }

    /**
     * @brief 订阅事件（同步处理器）
     * @tparam T 事件数据类型
     * @param name 事件名称
     * @param handler 事件处理器
     * @return 订阅ID
     */
    template<typename T>
    std::string subscribe(const std::string& name, EventHandler<T> handler) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        std::string id = turbot::utils::generate_uuid();
        HandlerEntry entry;
        entry.id = id;
        entry.handler = handler;

        handlers_[name].push_back(std::move(entry));

        return id;
    }

    /**
     * @brief 订阅事件（同步处理器 - 任意可调用对象）
     * @tparam T 事件数据类型
     * @tparam F 可调用对象类型
     * @param name 事件名称
     * @param handler 事件处理器（lambda、函数指针等）
     * @return 订阅ID
     */
    template<typename T, typename F>
    std::string subscribe(const std::string& name, F&& handler) {
        return subscribe<T>(name, EventHandler<T>(std::forward<F>(handler)));
    }

    /**
     * @brief 订阅事件（异步处理器）
     * @tparam T 事件数据类型
     * @param name 事件名称
     * @param handler 事件处理器
     * @return 订阅ID
     */
    template<typename T>
    std::string subscribe_async(const std::string& name, AsyncEventHandler<T> handler) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        std::string id = turbot::utils::generate_uuid();
        HandlerEntry entry;
        entry.id = id;
        entry.handler = handler;

        handlers_[name].push_back(std::move(entry));

        return id;
    }

    /**
     * @brief 取消订阅
     * @param name 事件名称
     * @param handler_id 处理器ID
     */
    void unsubscribe(const std::string& name, const std::string& handler_id);

    /**
     * @brief 获取订阅者数量
     * @param name 事件名称
     * @return 订阅者数量
     */
    size_t subscriber_count(const std::string& name) const;

    /**
     * @brief 列出所有事件
     * @return 事件名称列表
     */
    std::vector<std::string> list_events() const;

    /**
     * @brief 清空所有订阅
     */
    void clear();

    // Delete copy and move
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;

private:
    EventBus() = default;
    ~EventBus() = default;

    /**
     * @brief 处理器条目
     */
    struct HandlerEntry {
        std::string id;
        std::any handler;
    };

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::vector<HandlerEntry>> handlers_;
};

} // namespace turbot::core