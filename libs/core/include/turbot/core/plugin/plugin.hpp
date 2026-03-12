#pragma once

#include <turbot/core/common/export.hpp>
#include <nlohmann/json.hpp>
#include <functional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace turbot::core::plugin {

// ============================================================================
// Well-known hook names (mirrors opencode Plugin.Hooks interface)
// ============================================================================

/// Called before each LLM stream call. Handlers may modify temperature,
/// topP, and provider-specific options.
/// Input:  { "session_id": str, "agent": str, "model": str }
/// Output: { "temperature": float, "top_p": float, "options": object }
inline constexpr const char* kHookChatParams     = "chat.params";

/// Called before each LLM stream call. Handlers may inject extra HTTP headers.
/// Input:  { "session_id": str, "agent": str, "model": str }
/// Output: { "headers": object }
inline constexpr const char* kHookChatHeaders    = "chat.headers";

/// Called after the system prompt array is assembled. Handlers may transform
/// the list of system-prompt strings (append, prepend, replace items).
/// Input:  { "session_id": str, "model": str }
/// Output: { "system": array<string> }
inline constexpr const char* kHookSystemTransform = "experimental.chat.system.transform";

/// Called after the final assistant text is available. Handlers may post-
/// process the text (e.g. strip artefacts, append disclaimers).
/// Input:  { "session_id": str }
/// Output: { "text": str }
inline constexpr const char* kHookTextComplete   = "experimental.text.complete";

/// Called before full compaction begins (after the prune step). Handlers may
/// inject extra context into the compaction prompt or adjust the config.
/// Input:  { "session_id": str }
/// Output: { "system_injection": str }
inline constexpr const char* kHookSessionCompacting = "experimental.session.compacting";

// ============================================================================
// Hook handler signature
//
// Each handler receives:
//   input  — read-only context supplied by the caller (JSON object)
//   output — mutable payload that the handler may modify in-place
//
// Handlers MUST NOT throw; any exception propagates to the caller and aborts
// the remaining handler chain for that trigger call.
// ============================================================================

/// Synchronous hook handler.
using HookHandler = std::function<void(const nlohmann::json& input, nlohmann::json& output)>;

// ============================================================================
// PluginManager — process-wide singleton
// ============================================================================

/**
 * @brief Process-wide plugin hook registry.
 *
 * Mirrors the opencode `Plugin.trigger` / register pattern in C++.
 * Hooks are keyed by name (string).  Multiple handlers may be registered for
 * the same hook; they are invoked in registration order.
 *
 * Thread safety: all public methods are thread-safe via a shared_mutex.
 * Handlers are invoked outside the lock (copy-on-read), so re-entrant calls
 * to register/clear from within a handler are safe but not recommended.
 *
 * Usage (caller side — SessionLoop):
 * @code
 *   nlohmann::json ctx  = {{"session_id", id}, {"agent", agent}};
 *   nlohmann::json out  = {{"temperature", 1.0}, {"top_p", 1.0}};
 *   PluginManager::instance().trigger(kHookChatParams, ctx, out);
 *   // out["temperature"] now reflects any handler modifications
 * @endcode
 *
 * Usage (plugin author side):
 * @code
 *   PluginManager::instance().register_hook(
 *       kHookChatParams,
 *       [](const nlohmann::json& ctx, nlohmann::json& out) {
 *           out["temperature"] = 0.3;   // lower creativity for this session
 *       }
 *   );
 * @endcode
 */
class TURBOT_CORE_API PluginManager {
public:
    /// Returns the process-wide singleton.
    static PluginManager& instance() noexcept;

    /**
     * @brief Register a handler for the named hook.
     *
     * Handlers are appended and called in registration order during trigger().
     *
     * @param hook_name  Hook identifier (use the kHook* constants above).
     * @param handler    Callable invoked with (input_ctx, output_payload).
     * @return           An opaque registration ID that can be passed to
     *                   unregister_hook() to remove this specific handler.
     */
    std::size_t register_hook(const std::string& hook_name, HookHandler handler);

    /**
     * @brief Unregister a specific handler by its registration ID.
     *
     * @param hook_name      Hook name used during registration.
     * @param registration_id  ID returned by register_hook().
     * @return true if the handler was found and removed.
     */
    bool unregister_hook(const std::string& hook_name, std::size_t registration_id);

    /**
     * @brief Invoke all handlers registered for @p hook_name in order.
     *
     * Each handler receives @p input (read-only context) and @p output
     * (mutable payload).  Handlers are copied before the lock is released so
     * that re-entrant registrations during trigger() are safe.
     *
     * @param hook_name  Hook to trigger.
     * @param input      Read-only context (caller-supplied, not modified).
     * @param output     Mutable payload; handlers may modify it in-place.
     *
     * @note If a handler throws, the exception propagates to the caller and
     *       remaining handlers for this trigger call are skipped.
     */
    void trigger(const std::string& hook_name,
                 const nlohmann::json& input,
                 nlohmann::json& output) const;

    /**
     * @brief Remove all handlers registered for @p hook_name.
     */
    void clear(const std::string& hook_name);

    /**
     * @brief Remove all registered handlers (all hooks).
     */
    void clear_all();

    /**
     * @brief Return the number of registered handlers for @p hook_name.
     */
    [[nodiscard]] std::size_t handler_count(const std::string& hook_name) const;

    // Non-copyable, non-movable (singleton)
    PluginManager(const PluginManager&)            = delete;
    PluginManager& operator=(const PluginManager&) = delete;
    PluginManager(PluginManager&&)                 = delete;
    PluginManager& operator=(PluginManager&&)      = delete;

private:
    PluginManager() = default;
    ~PluginManager() = default;

    struct Entry {
        std::size_t  id;
        HookHandler  handler;
    };

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::vector<Entry>> hooks_;
    std::size_t next_id_{0};
};

} // namespace turbot::core::plugin
