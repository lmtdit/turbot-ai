#include <turbot/core/plugin/plugin.hpp>
#include <algorithm>

namespace turbot::core::plugin {

// ============================================================================
// Singleton
// ============================================================================

PluginManager& PluginManager::instance() noexcept {
    // Guaranteed to be initialised exactly once (C++11 §6.7).
    static PluginManager s_instance;
    return s_instance;
}

// ============================================================================
// register_hook
// ============================================================================

std::size_t PluginManager::register_hook(const std::string& hook_name, HookHandler handler) {
    std::unique_lock lock(mutex_);
    const std::size_t id = next_id_++;
    hooks_[hook_name].push_back(Entry{id, std::move(handler)});
    return id;
}

// ============================================================================
// unregister_hook
// ============================================================================

bool PluginManager::unregister_hook(const std::string& hook_name,
                                    std::size_t registration_id) {
    std::unique_lock lock(mutex_);
    auto it = hooks_.find(hook_name);
    if (it == hooks_.end()) {
        return false;
    }
    auto& entries = it->second;
    const auto before = entries.size();
    entries.erase(
        std::remove_if(entries.begin(), entries.end(),
                       [registration_id](const Entry& e) {
                           return e.id == registration_id;
                       }),
        entries.end());
    return entries.size() < before;
}

// ============================================================================
// trigger
// ============================================================================

void PluginManager::trigger(const std::string& hook_name,
                            const nlohmann::json& input,
                            nlohmann::json& output) const {
    // Copy the handler list under a shared lock so that:
    // 1. Concurrent trigger() calls on different hooks do not block each other.
    // 2. Re-entrant register/unregister from within a handler does not deadlock.
    std::vector<Entry> local_entries;
    {
        std::shared_lock lock(mutex_);
        auto it = hooks_.find(hook_name);
        if (it == hooks_.end()) {
            return; // No handlers registered — nothing to do.
        }
        local_entries = it->second;
    }

    // Invoke handlers sequentially, outside the lock.
    for (const auto& entry : local_entries) {
        // Guard against empty std::function (e.g. a default-constructed handler
        // accidentally registered), which would throw std::bad_function_call.
        if (entry.handler) {
            entry.handler(input, output);
        }
    }
}

// ============================================================================
// clear / clear_all
// ============================================================================

void PluginManager::clear(const std::string& hook_name) {
    std::unique_lock lock(mutex_);
    hooks_.erase(hook_name);
}

void PluginManager::clear_all() {
    std::unique_lock lock(mutex_);
    hooks_.clear();
    // Note: next_id_ is intentionally NOT reset so that IDs remain
    // globally monotone even across clear_all() calls.  This prevents
    // any stale stored ID from matching a freshly registered handler.
}

// ============================================================================
// handler_count
// ============================================================================

std::size_t PluginManager::handler_count(const std::string& hook_name) const {
    std::shared_lock lock(mutex_);
    auto it = hooks_.find(hook_name);
    if (it == hooks_.end()) {
        return 0;
    }
    return it->second.size();
}

} // namespace turbot::core::plugin
