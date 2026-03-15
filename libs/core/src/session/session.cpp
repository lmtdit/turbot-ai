#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_store.hpp>
#include <turbot/core/snapshot/snapshot.hpp>
#include <fmt/format.h>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace turbot::core::session {

// ─── RevertInfo implementation ───────────────────────────────────────────────

nlohmann::json RevertInfo::to_json() const {
    // Use opencode-compatible camelCase keys for cross-system compatibility
    nlohmann::json j;
    j["messageID"] = message_id;
    if (part_id)     j["partID"]   = *part_id;
    if (snapshot_id) j["snapshot"] = *snapshot_id;
    if (diff)        j["diff"]     = *diff;
    if (pre_patch)   j["prePatch"] = pre_patch->to_json();
    return j;
}

RevertInfo RevertInfo::from_json(const nlohmann::json& j) {
    RevertInfo info;

    // Accept both opencode camelCase ("messageID") and snake_case ("message_id") keys.
    if (j.contains("messageID") && !j["messageID"].is_null())
        info.message_id = j["messageID"].get<std::string>();
    else if (j.contains("message_id") && !j["message_id"].is_null())
        info.message_id = j["message_id"].get<std::string>();

    // partID / part_id
    if (j.contains("partID") && !j["partID"].is_null())
        info.part_id = j["partID"].get<std::string>();
    else if (j.contains("part_id") && !j["part_id"].is_null())
        info.part_id = j["part_id"].get<std::string>();

    // snapshot / snapshot_id
    if (j.contains("snapshot") && !j["snapshot"].is_null())
        info.snapshot_id = j["snapshot"].get<std::string>();
    else if (j.contains("snapshot_id") && !j["snapshot_id"].is_null())
        info.snapshot_id = j["snapshot_id"].get<std::string>();

    if (j.contains("diff") && !j["diff"].is_null())
        info.diff = j["diff"].get<std::string>();

    if (j.contains("prePatch") && !j["prePatch"].is_null())
        info.pre_patch = turbot::core::snapshot::PatchResult::from_json(j["prePatch"]);

    return info;
}

bool RevertInfo::operator==(const RevertInfo& other) const noexcept {
    return message_id == other.message_id &&
           part_id    == other.part_id    &&
           snapshot_id == other.snapshot_id &&
           diff       == other.diff;
    // pre_patch intentionally excluded (binary payload, equality checked by snapshot_id)
}

// ─── SessionState conversion functions ───────────────────────────────────────
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
    if (revert) {
        j["revert"] = revert->to_json();
    }
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
    
    if (j.contains("revert") && !j["revert"].is_null()) {
        info.revert = RevertInfo::from_json(j["revert"]);
    }
    
    return info;
}

bool SessionInfo::operator==(const SessionInfo& other) const noexcept {
    return id == other.id &&
           project_id == other.project_id &&
           parent_id == other.parent_id &&
           slug == other.slug &&
           directory == other.directory &&
           title == other.title &&
           version == other.version &&
           state == other.state &&
           time_created == other.time_created &&
           time_updated == other.time_updated &&
           revert == other.revert;
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

    // Persist to DB if store is initialised
    SessionStore::instance().save(session.info_);

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

    // Persist the new fork to DB
    SessionStore::instance().save(session.info_);

    return session;
}

std::optional<Session> Session::get(const std::string& id) {
    auto& store = SessionStore::instance();
    if (!store.is_initialized()) {
        // No DB configured — fall back to placeholder (not found)
        return std::nullopt;
    }
    auto row = store.find_by_id(id);
    if (!row) return std::nullopt;
    try {
        return Session(SessionInfo::from_json(*row));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::vector<Session> Session::list(const std::string& project_id) {
    auto& store = SessionStore::instance();
    if (!store.is_initialized()) return {};

    auto rows = store.find_all(project_id);
    std::vector<Session> sessions;
    sessions.reserve(rows.size());
    for (const auto& row : rows) {
        try {
            sessions.emplace_back(SessionInfo::from_json(row));
        } catch (const std::exception&) {
            // Skip corrupt rows
        }
    }
    return sessions;
}

bool Session::remove(const std::string& id) {
    auto& store = SessionStore::instance();
    if (!store.is_initialized()) return false;
    return store.remove(id);
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
    // Persist updated state
    SessionStore::instance().save(info_);
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

    auto& store = SessionStore::instance();
    if (!store.is_initialized()) return {};
    return store.list_messages(info_.id, limit, offset);
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

bool Session::revert(const RevertParams& params) {
    if (!mutex_) return false;
    std::lock_guard<std::mutex> lock(*mutex_);

    auto& sm = turbot::core::snapshot::SnapshotManager::instance();

    // Preserve the original snapshot when chaining reverts so that unrevert()
    // can always return to the original baseline from before the first revert.
    if (info_.revert && info_.revert->pre_patch) {
        // Already in a revert state — roll back the additional patches using
        // the existing pre_patch as the restore baseline, but update the target.
        for (auto it = params.patches.rbegin(); it != params.patches.rend(); ++it) {
            if (!sm.rollback_patch(*it)) {
                // Attempt to restore the previous revert state.
                sm.apply_patch(*info_.revert->pre_patch);
                return false;
            }
        }
        // Update the revert boundary but keep the original pre_patch.
        info_.revert->message_id  = params.message_id;
        info_.revert->part_id     = params.part_id;
        info_.time_updated        = current_timestamp();
        return true;
    }

    // New revert: capture the current file state BEFORE rolling back.
    // start_tracking records baseline hashes/contents.
    const std::string dir = info_.directory.empty() ? "." : info_.directory;
    turbot::core::snapshot::SnapshotOptions opts;
    opts.root_directory = dir;
    const std::string snap_id = sm.start_tracking(opts);

    // Roll back all patches in reverse order (newest → oldest).
    for (auto it = params.patches.rbegin(); it != params.patches.rend(); ++it) {
        if (!sm.rollback_patch(*it)) {
            // Partial rollback — cancel tracking and signal failure.
            sm.cancel_tracking(snap_id);
            return false;
        }
    }

    // stop_tracking AFTER rollback: the diff now records exactly "pre-revert → post-revert"
    // changes, which apply_patch() can re-apply during unrevert() to restore the files.
    const auto pre_patch = sm.stop_tracking(snap_id);

    // Record the revert info.
    RevertInfo ri;
    ri.message_id  = params.message_id;
    ri.part_id     = params.part_id;
    ri.snapshot_id = snap_id;
    ri.pre_patch   = pre_patch;
    // diff is populated externally after computing SessionSummary (opencode pattern).
    info_.revert       = std::move(ri);
    info_.time_updated = current_timestamp();

    return true;
}

bool Session::unrevert() {
    if (!mutex_) return false;
    std::lock_guard<std::mutex> lock(*mutex_);

    if (!info_.revert) {
        return true;  // Nothing to unrevert.
    }

    // Re-apply the pre-revert patch to restore the working directory.
    // pre_patch records "pre-revert state → post-revert state" (old→new).
    // To undo the revert we need to go back: rollback_patch restores old_content,
    // i.e. the state that existed before the original revert was executed.
    if (info_.revert->pre_patch && !info_.revert->pre_patch->empty()) {
        auto& sm = turbot::core::snapshot::SnapshotManager::instance();
        if (!sm.rollback_patch(*info_.revert->pre_patch)) {
            return false;  // Restoration failed; leave revert info intact for retry.
        }
    }

    // Clear the revert info.
    info_.revert       = std::nullopt;
    info_.time_updated = current_timestamp();

    return true;
}

bool Session::cleanup_revert() {
    if (!mutex_) return false;
    std::lock_guard<std::mutex> lock(*mutex_);

    // Simply discard the revert info without touching files.
    // Callers must have already truncated the message history in the database
    // and confirmed the user does not wish to unrevert.
    info_.revert       = std::nullopt;
    info_.time_updated = current_timestamp();

    return true;
}

} // namespace turbot::core::session
