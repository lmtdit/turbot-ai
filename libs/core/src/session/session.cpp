#include <turbot/core/session/session.hpp>
#include <fmt/format.h>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace turbot::core::session {

// SessionState conversion functions
std::string session_state_to_string(SessionState state) {
    switch (state) {
        case SessionState::Created: return "created";
        case SessionState::Active: return "active";
        case SessionState::Busy: return "busy";
        case SessionState::Compacting: return "compacting";
        case SessionState::Archived: return "archived";
    }
    throw std::invalid_argument(fmt::format("Invalid SessionState value: {}", static_cast<int>(state)));
}

SessionState string_to_session_state(const std::string& str) {
    if (str == "created") return SessionState::Created;
    if (str == "active") return SessionState::Active;
    if (str == "busy") return SessionState::Busy;
    if (str == "compacting") return SessionState::Compacting;
    if (str == "archived") return SessionState::Archived;
    throw std::invalid_argument(fmt::format("Invalid session state: {}", str));
}

// SessionInfo implementation
nlohmann::json SessionInfo::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["project_id"] = project_id;
    if (parent_id) {
        j["parent_id"] = *parent_id;
    }
    j["slug"] = slug;
    j["directory"] = directory;
    j["title"] = title;
    j["version"] = version;
    if (permission) {
        j["permission"] = *permission;
    }
    j["time_created"] = time_created;
    j["time_updated"] = time_updated;
    if (time_compacting) {
        j["time_compacting"] = *time_compacting;
    }
    if (time_archived) {
        j["time_archived"] = *time_archived;
    }
    j["state"] = session_state_to_string(state);
    return j;
}

SessionInfo SessionInfo::from_json(const nlohmann::json& j) {
    SessionInfo info;
    info.id = j.at("id").get<std::string>();
    info.project_id = j.at("project_id").get<std::string>();
    
    if (j.contains("parent_id") && !j["parent_id"].is_null()) {
        info.parent_id = j["parent_id"].get<std::string>();
    }
    
    info.slug = j.at("slug").get<std::string>();
    info.directory = j.at("directory").get<std::string>();
    info.title = j.at("title").get<std::string>();
    
    if (j.contains("version")) {
        info.version = j["version"].get<std::string>();
    }
    
    if (j.contains("permission") && !j["permission"].is_null()) {
        info.permission = j["permission"];
    }
    
    info.time_created = j.at("time_created").get<int64_t>();
    info.time_updated = j.at("time_updated").get<int64_t>();
    
    if (j.contains("time_compacting") && !j["time_compacting"].is_null()) {
        info.time_compacting = j["time_compacting"].get<int64_t>();
    }
    if (j.contains("time_archived") && !j["time_archived"].is_null()) {
        info.time_archived = j["time_archived"].get<int64_t>();
    }
    
    if (j.contains("state")) {
        info.state = string_to_session_state(j["state"].get<std::string>());
    }
    
    return info;
}

bool SessionInfo::operator==(const SessionInfo& other) const noexcept {
    return id == other.id &&
           project_id == other.project_id &&
           parent_id == other.parent_id &&
           slug == other.slug &&
           state == other.state;
}

// Session implementation
std::string Session::generate_id() {
    // Generate a unique ID using timestamp and random number
    const auto now = std::chrono::system_clock::now();
    const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999999);
    
    return fmt::format("sess_{:x}_{:06d}", timestamp, dis(gen));
}

int64_t Session::current_timestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::optional<Session> Session::create(const CreateParams& params) {
    SessionInfo info;
    info.id = generate_id();
    info.project_id = params.project_id;
    info.slug = params.slug;
    info.directory = params.directory;
    info.title = params.title;
    info.version = "1.0.0";
    info.permission = params.permission;
    info.time_created = current_timestamp();
    info.time_updated = info.time_created;
    info.state = SessionState::Created;
    
    Session session(std::move(info));
    session.mutex_ = std::make_shared<std::mutex>();
    return session;
}

std::optional<Session> Session::fork(const ForkParams& params) {
    // Get parent session
    auto parent_opt = get(params.parent_id);
    if (!parent_opt) {
        return std::nullopt;
    }
    
    const auto& parent = *parent_opt;
    
    SessionInfo info;
    info.id = generate_id();
    info.project_id = parent.info_.project_id;
    info.parent_id = params.parent_id;
    info.slug = params.slug;
    info.directory = parent.info_.directory;
    info.title = params.title;
    info.version = parent.info_.version;
    info.permission = parent.info_.permission;
    info.time_created = current_timestamp();
    info.time_updated = info.time_created;
    info.state = SessionState::Created;
    
    Session session(std::move(info));
    session.mutex_ = std::make_shared<std::mutex>();
    return session;
}

std::optional<Session> Session::get(const std::string& id) {
    // This is a placeholder - in a real implementation,
    // this would query the database
    // For now, return nullopt to indicate not found
    (void)id; // Suppress unused parameter warning
    return std::nullopt;
}

std::vector<Session> Session::list(const std::string& project_id) {
    // This is a placeholder - in a real implementation,
    // this would query the database
    (void)project_id; // Suppress unused parameter warning
    return {};
}

bool Session::remove(const std::string& id) {
    // This is a placeholder - in a real implementation,
    // this would delete from the database
    (void)id; // Suppress unused parameter warning
    return false;
}

bool Session::update(const UpdateParams& params) {
    if (!mutex_) return false;
    std::lock_guard<std::mutex> lock(*mutex_);
    if (params.title) {
        info_.title = *params.title;
    }
    if (params.permission) {
        info_.permission = *params.permission;
    }
    if (params.state) {
        info_.state = *params.state;
    }
    info_.time_updated = current_timestamp();
    return true;
}

bool Session::set_title(const std::string& title) {
    return update(UpdateParams{.title = title});
}

bool Session::set_permission(const nlohmann::json& permission) {
    return update(UpdateParams{.permission = permission});
}

std::vector<nlohmann::json> Session::messages(int limit, int offset) const {
    // Validate parameters
    if (limit < 0) limit = 50;
    if (offset < 0) offset = 0;
    
    // This is a placeholder - in a real implementation,
    // this would query messages from the database
    (void)limit;
    (void)offset;
    return {};
}

bool Session::compact() {
    if (!mutex_) return false;
    std::lock_guard<std::mutex> lock(*mutex_);
    if (info_.state == SessionState::Archived) {
        return false; // Cannot compact archived session
    }
    
    const int64_t now = current_timestamp();
    SessionState prev_state = info_.state;
    info_.state = SessionState::Compacting;
    info_.time_compacting = now;
    info_.time_updated = now;
    
    // Perform compaction logic here
    // For now, just restore to previous state
    info_.state = prev_state;
    
    return true;
}

bool Session::archive() {
    if (!mutex_) return false;
    std::lock_guard<std::mutex> lock(*mutex_);
    if (info_.state == SessionState::Archived) {
        return false; // Already archived
    }
    
    const int64_t now = current_timestamp();
    info_.state = SessionState::Archived;
    info_.time_archived = now;
    info_.time_updated = now;
    
    return true;
}

bool Session::restore() {
    if (!mutex_) return false;
    std::lock_guard<std::mutex> lock(*mutex_);
    if (info_.state != SessionState::Archived) {
        return false; // Not archived
    }
    
    info_.state = SessionState::Active;
    info_.time_archived = std::nullopt;
    info_.time_updated = current_timestamp();
    
    return true;
}

} // namespace turbot::core::session
