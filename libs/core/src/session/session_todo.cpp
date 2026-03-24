#include <turbot/core/session/session_todo.hpp>
#include <turbot/core/session/session_store.hpp>
#include <turbot/core/event/event_bus.hpp>
#include <turbot/core/common/logger.hpp>
#include <nlohmann/json.hpp>

namespace turbot::core::session {

// ---------------------------------------------------------------------------
// TodoInfo serialisation
// ---------------------------------------------------------------------------
nlohmann::json TodoInfo::to_json() const {
    return nlohmann::json{
        {"content",  content},
        {"status",   status},
        {"priority", priority},
    };
}

/*static*/ TodoInfo TodoInfo::from_json(const nlohmann::json& j) {
    TodoInfo t;
    t.content  = j.value("content",  "");
    t.status   = j.value("status",   "pending");
    t.priority = j.value("priority", "medium");
    return t;
}

// ---------------------------------------------------------------------------
// Todo::update() — mirrors OpenCode Todo.update()
// ---------------------------------------------------------------------------
namespace Todo {

void update(const std::string& session_id, const std::vector<TodoInfo>& todos) {
    auto& store = SessionStore::instance();
    if (!store.is_initialized()) {
        TURBOT_LOG_WARN("Todo::update: SessionStore not initialised — skipping persistence");
        return;
    }

    store.save_todos(session_id, todos);

    // Publish EventBus event
    turbot::core::EventBus::instance().publish(
        TodoUpdatedEvent::kEventName,
        TodoUpdatedEvent{session_id, todos});

    TURBOT_LOG_INFO("Todo::update: session={} count={}", session_id, todos.size());
}

// ---------------------------------------------------------------------------
// Todo::get() — mirrors OpenCode Todo.get()
// ---------------------------------------------------------------------------
std::vector<TodoInfo> get(const std::string& session_id) {
    auto& store = SessionStore::instance();
    if (!store.is_initialized()) return {};
    return store.get_todos(session_id);
}

} // namespace Todo

} // namespace turbot::core::session
