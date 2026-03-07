#include <turbot/core/event/event_bus.hpp>
#include <turbot/utils/string_utils.hpp>
#include <algorithm>
#include <set>

using namespace turbot::utils;

namespace turbot::core {

EventBus& EventBus::instance() noexcept {
    static EventBus instance;
    return instance;
}

void EventBus::unsubscribe(const std::string& name, const std::string& handler_id) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = handlers_.find(name);
    if (it != handlers_.end()) {
        // 移除指定的处理器
        it->second.erase(
            std::remove_if(it->second.begin(), it->second.end(),
                [&handler_id](const HandlerEntry& entry) {
                    return entry.id == handler_id;
                }),
            it->second.end()
        );

        // 如果该事件没有处理器了，删除事件条目
        if (it->second.empty()) {
            handlers_.erase(it);
        }
    }
}

size_t EventBus::subscriber_count(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = handlers_.find(name);
    if (it != handlers_.end()) {
        return it->second.size();
    }

    return 0;
}

std::vector<std::string> EventBus::list_events() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<std::string> events;
    events.reserve(handlers_.size());

    for (const auto& [name, handlers] : handlers_) {
        events.push_back(name);
    }

    // 按名称排序
    std::sort(events.begin(), events.end());

    return events;
}

void EventBus::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    handlers_.clear();
}

} // namespace turbot::core