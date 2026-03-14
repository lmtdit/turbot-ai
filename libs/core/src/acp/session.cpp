#include "turbot/core/acp/session.hpp"

#include <chrono>
#include <stdexcept>

namespace turbot::core::acp {

// ---------------------------------------------------------------------------
// ACPSessionManager::create
// ---------------------------------------------------------------------------

ACPSessionState ACPSessionManager::create(
    const std::string& session_id,
    const std::string& cwd,
    const std::vector<nlohmann::json>& mcp_servers,
    const std::optional<std::string>& provider_id,
    const std::optional<std::string>& model_id) {

    using namespace std::chrono;
    const int64_t now =
        duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

    ACPSessionState state;
    state.id          = session_id;
    state.cwd         = cwd;
    state.mcp_servers = mcp_servers;
    state.provider_id = provider_id;
    state.model_id    = model_id;
    state.created_at  = now;

    std::lock_guard<std::mutex> lk(mutex_);
    sessions_[session_id] = state;
    return state;
}

// ---------------------------------------------------------------------------
// ACPSessionManager::load
// ---------------------------------------------------------------------------

ACPSessionState ACPSessionManager::load(
    const std::string& session_id,
    const std::string& cwd,
    const std::vector<nlohmann::json>& mcp_servers,
    const std::optional<std::string>& provider_id,
    const std::optional<std::string>& model_id) {

    using namespace std::chrono;
    const int64_t now =
        duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

    ACPSessionState state;
    state.id          = session_id;
    state.cwd         = cwd;
    state.mcp_servers = mcp_servers;
    state.provider_id = provider_id;
    state.model_id    = model_id;
    state.created_at  = now;

    std::lock_guard<std::mutex> lk(mutex_);
    sessions_[session_id] = state;
    return state;
}

// ---------------------------------------------------------------------------
// ACPSessionManager::get
// ---------------------------------------------------------------------------

ACPSessionState ACPSessionManager::get(const std::string& session_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        throw std::invalid_argument("Session not found: " + session_id);
    }
    return it->second;  // return copy (safe after lock release)
}

// ---------------------------------------------------------------------------
// ACPSessionManager::try_get
// ---------------------------------------------------------------------------

ACPSessionState* ACPSessionManager::try_get(const std::string& session_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return nullptr;
    return &it->second;
}

// ---------------------------------------------------------------------------
// ACPSessionManager::set_model
// ---------------------------------------------------------------------------

void ACPSessionManager::set_model(
    const std::string& session_id,
    const std::string& provider_id,
    const std::string& model_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second.provider_id = provider_id;
        it->second.model_id    = model_id;
    }
}

// ---------------------------------------------------------------------------
// ACPSessionManager::set_mode
// ---------------------------------------------------------------------------

void ACPSessionManager::set_mode(
    const std::string& session_id, const std::string& mode_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second.mode_id = mode_id;
    }
}

// ---------------------------------------------------------------------------
// ACPSessionManager::set_variant
// ---------------------------------------------------------------------------

void ACPSessionManager::set_variant(
    const std::string& session_id,
    const std::optional<std::string>& variant) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second.variant = variant;
    }
}

// ---------------------------------------------------------------------------
// ACPSessionManager::get_variant
// ---------------------------------------------------------------------------

std::string ACPSessionManager::get_variant(const std::string& session_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end() || !it->second.variant) return "";
    return *it->second.variant;
}

// ---------------------------------------------------------------------------
// ACPSessionManager::get_mode_id
// ---------------------------------------------------------------------------

std::string ACPSessionManager::get_mode_id(const std::string& session_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end() || !it->second.mode_id) return "";
    return *it->second.mode_id;
}

// ---------------------------------------------------------------------------
// ACPSessionManager::list
// ---------------------------------------------------------------------------

std::vector<ACPSessionState> ACPSessionManager::list() const {
    std::lock_guard<std::mutex> lk(mutex_);
    std::vector<ACPSessionState> result;
    result.reserve(sessions_.size());
    for (const auto& [id, state] : sessions_) {
        result.push_back(state);
    }
    return result;
}

// ---------------------------------------------------------------------------
// ACPSessionManager::remove
// ---------------------------------------------------------------------------

void ACPSessionManager::remove(const std::string& session_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    sessions_.erase(session_id);
}

}  // namespace turbot::core::acp
