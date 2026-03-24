#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_store.hpp>
#include <turbot/core/session/session_compaction.hpp>
#include <turbot/core/session/session_events.hpp>
#include <turbot/core/snapshot/snapshot.hpp>
#include <turbot/core/message/message.hpp>
#include <turbot/core/event/event_bus.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/core/id/id.hpp>
#include <fmt/format.h>
#include <chrono>
#include <mutex>
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
    // Produce OpenCode-compatible wire format:
    //   - camelCase field names
    //   - timestamps nested under "time": { created, updated, compacting?, archived? }
    //   - "state" intentionally omitted (managed in-memory by SessionStatus)
    nlohmann::json j;
    j["id"]        = id;
    j["slug"]      = slug;
    j["projectID"] = project_id;
    if (workspace_id) j["workspaceID"] = *workspace_id;
    j["directory"] = directory;
    if (parent_id)  j["parentID"]  = *parent_id;
    j["title"]   = title;
    j["version"] = version;

    // Nested time object.
    nlohmann::json time_obj;
    time_obj["created"] = time_created;
    time_obj["updated"] = time_updated;
    if (time_compacting) time_obj["compacting"] = *time_compacting;
    if (time_archived)   time_obj["archived"]   = *time_archived;
    j["time"] = std::move(time_obj);

    if (permission) j["permission"] = *permission;

    // Revert: delegate to RevertInfo::to_json() and strip the Turbot-internal
    // prePatch field — that binary payload is not part of the OpenCode wire format.
    if (revert) {
        auto rev = revert->to_json();
        rev.erase("prePatch");  // prePatch is Turbot-internal; omit from wire format
        j["revert"] = std::move(rev);
    }

    // Summary (git-diff stats, populated after session completes).
    if (summary) {
        nlohmann::json sum;
        sum["additions"] = summary->additions;
        sum["deletions"] = summary->deletions;
        sum["files"]     = summary->files;
        if (summary->diffs) sum["diffs"] = *summary->diffs;
        j["summary"] = std::move(sum);
    }

    // Share URL (enterprise).
    if (share) {
        j["share"] = {{"url", share->url}};
    }

    return j;
}

SessionInfo SessionInfo::from_json(const nlohmann::json& j) {
    // Accept both camelCase (API/wire) and snake_case (SQLite DB column) key variants.
    // Helper lambdas pick the first key that exists.
    auto get_str = [&](const char* camel, const char* snake,
                       const std::string& def = {}) -> std::string {
        if (j.contains(camel) && !j[camel].is_null())
            return j[camel].get<std::string>();
        if (j.contains(snake) && !j[snake].is_null())
            return j[snake].get<std::string>();
        return def;
    };
    auto get_opt_str = [&](const char* camel, const char* snake)
            -> std::optional<std::string> {
        if (j.contains(camel) && !j[camel].is_null())
            return j[camel].get<std::string>();
        if (j.contains(snake) && !j[snake].is_null())
            return j[snake].get<std::string>();
        return std::nullopt;
    };
    auto get_opt_i64 = [&](const char* camel, const char* snake)
            -> std::optional<int64_t> {
        if (j.contains(camel) && !j[camel].is_null())
            return j[camel].get<int64_t>();
        if (j.contains(snake) && !j[snake].is_null())
            return j[snake].get<int64_t>();
        return std::nullopt;
    };

    SessionInfo info;
    info.id           = j.at("id").get<std::string>();
    info.slug         = j.value("slug", std::string{});
    info.project_id   = get_str("projectID",   "project_id");
    info.workspace_id = get_opt_str("workspaceID", "workspace_id");
    info.parent_id    = get_opt_str("parentID",    "parent_id");
    info.directory    = j.value("directory", std::string{});
    info.title        = j.value("title", std::string{});
    info.version      = j.value("version", std::string{"1.0.0"});

    // Time: nested "time" object (wire format) OR flat columns (DB format).
    if (j.contains("time") && j["time"].is_object()) {
        const auto& t   = j["time"];
        info.time_created   = t.value("created",   int64_t{0});
        info.time_updated   = t.value("updated",   int64_t{0});
        if (t.contains("compacting") && !t["compacting"].is_null())
            info.time_compacting = t["compacting"].get<int64_t>();
        if (t.contains("archived") && !t["archived"].is_null())
            info.time_archived = t["archived"].get<int64_t>();
    } else {
        info.time_created = j.value("time_created", int64_t{0});
        info.time_updated = j.value("time_updated", int64_t{0});
        // DB flat columns only have snake_case; these time fields have no camelCase
        // variant in the flat-column path (they live inside nested "time" in wire format).
        info.time_compacting = get_opt_i64("time_compacting", "time_compacting");
        info.time_archived   = get_opt_i64("time_archived",   "time_archived");
    }

    // Reconstruct internal state from time fields (state is not in the wire format).
    if (info.time_archived)    info.state = SessionState::Archived;
    else if (info.time_compacting) info.state = SessionState::Compacting;
    else                       info.state = SessionState::Active;
    // DB rows may carry an explicit "state" column — honour it for backward compat.
    if (j.contains("state") && j["state"].is_string()) {
        try { info.state = string_to_session_state(j["state"].get<std::string>()); }
        catch (const std::exception&) { /* ignore invalid state strings from old rows */ }
    }

    if (j.contains("permission") && !j["permission"].is_null())
        info.permission = j["permission"];

    // Revert: accept both camelCase and existing snake_case variants.
    if (j.contains("revert") && !j["revert"].is_null()) {
        info.revert = RevertInfo::from_json(j["revert"]);
    }

    // Share URL.
    if (j.contains("share") && !j["share"].is_null()) {
        const auto& sh = j["share"];
        if (sh.contains("url") && sh["url"].is_string()) {
            SessionShare share;
            share.url = sh["url"].get<std::string>();
            if (!share.url.empty())
                info.share = std::move(share);
        }
    } else if (j.contains("share_url") && !j["share_url"].is_null()) {
        // Flat DB column form
        std::string url = j["share_url"].get<std::string>();
        if (!url.empty()) {
            SessionShare share;
            share.url = std::move(url);
            info.share = std::move(share);
        }
    }

    // Summary — nested "summary" (wire format) or flat columns (DB form).
    if (j.contains("summary") && !j["summary"].is_null()) {
        const auto& s = j["summary"];
        SessionSummary sum;
        sum.additions = s.value("additions", 0);
        sum.deletions = s.value("deletions", 0);
        sum.files     = s.value("files",     0);
        if (s.contains("diffs") && !s["diffs"].is_null())
            sum.diffs = s["diffs"];
        info.summary = std::move(sum);
    } else if (j.contains("summary_additions") && !j["summary_additions"].is_null()) {
        // Flat DB columns form
        SessionSummary sum;
        sum.additions = j.value("summary_additions", 0);
        sum.deletions = j.value("summary_deletions", 0);
        sum.files     = j.value("summary_files",     0);
        if (j.contains("summary_diffs") && !j["summary_diffs"].is_null()) {
            try {
                sum.diffs = nlohmann::json::parse(j["summary_diffs"].get<std::string>());
            } catch (const std::exception&) { /* ignore malformed JSON */ }
        }
        info.summary = std::move(sum);
    }

    return info;
}

bool SessionInfo::operator==(const SessionInfo& other) const noexcept {
    return id == other.id &&
           project_id    == other.project_id    &&
           workspace_id  == other.workspace_id  &&
           parent_id     == other.parent_id     &&
           slug          == other.slug          &&
           directory     == other.directory     &&
           title         == other.title         &&
           version       == other.version       &&
           state         == other.state         &&
           time_created  == other.time_created  &&
           time_updated  == other.time_updated  &&
           revert        == other.revert        &&
           summary       == other.summary       &&
           share         == other.share;
}

// Session implementation

std::string Session::generate_id() {
    // Delegate to the shared ID module, mirrors OpenCode's Identifier.descending("session").
    return turbot::core::id::session_id();
}

int64_t Session::current_timestamp() {
    // Return milliseconds since epoch — aligns with OpenCode's Date.now() convention.
    return std::chrono::duration_cast<std::chrono::milliseconds>(
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

    // T1: Emit session.created + session.updated (mirrors OpenCode createNext()).
    const nlohmann::json wire = session.info_.to_json();
    turbot::core::EventBus::instance().publish(
        SessionCreatedEvent::kEventName, SessionCreatedEvent{wire});
    turbot::core::EventBus::instance().publish(
        SessionInfoUpdatedEvent::kEventName, SessionInfoUpdatedEvent{wire});

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

    // Copy message history from the parent session.
    //
    // Aligned with OpenCode Session.fork(sessionID, messageID?) which clones all
    // messages up to the specified cutoff.  Since we currently use an INTEGER seq
    // rather than a string MessageID, the cutoff is expressed as a seq number.
    //
    // NOTE: We emit session.created AFTER copy_messages succeeds so that
    // subscribers never see a forked session that is subsequently rolled back.
    auto& store = SessionStore::instance();
    if (store.is_initialized()) {
        int copied = store.copy_messages(
            params.parent_id,
            session.info_.id,
            params.message_seq_cutoff  // nullopt → copy all messages
        );
        if (copied < 0) {
            TURBOT_LOG_WARN("Session::fork: copy_messages failed (parent={} fork={}); "
                            "rolling back fork session",
                            params.parent_id, session.info_.id);
            // Rollback: remove the orphaned fork session so the DB stays consistent.
            store.remove(session.info_.id);
            return std::nullopt;
        }
        TURBOT_LOG_DEBUG("Session::fork: copied {} messages from {} to {}",
                         copied, params.parent_id, session.info_.id);
    }

    // T1: Emit session.created + session.updated for the forked session.
    // Emitted AFTER copy_messages so subscribers only see successfully-created forks.
    {
        const nlohmann::json wire = session.info_.to_json();
        turbot::core::EventBus::instance().publish(
            SessionCreatedEvent::kEventName, SessionCreatedEvent{wire});
        turbot::core::EventBus::instance().publish(
            SessionInfoUpdatedEvent::kEventName, SessionInfoUpdatedEvent{wire});
    }

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

std::vector<Session> Session::list(const ListParams& params) {
    auto& store = SessionStore::instance();
    if (!store.is_initialized()) return {};

    auto rows = store.find_all(params);
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

std::vector<Session> Session::list(const std::string& project_id) {
    return list(ListParams{.project_id = project_id});
}

bool Session::remove(const std::string& id) {
    auto& store = SessionStore::instance();
    if (!store.is_initialized()) return false;

    // T1: Emit session.deleted BEFORE the row is removed so consumers can still
    // read the session info from the payload (matches OpenCode ordering).
    auto row = store.find_by_id(id);
    if (row) {
        try {
            turbot::core::EventBus::instance().publish(
                SessionDeletedEvent::kEventName,
                SessionDeletedEvent{SessionInfo::from_json(*row).to_json()});
        } catch (const std::exception& ex) {
            TURBOT_LOG_WARN("Session::remove: failed to build delete event for session '{}': {}",
                            id, ex.what());
        }
    }

    return store.remove(id);
}

bool Session::update(const UpdateParams& params) {
    if (!mutex_) return false;
    // Serialize info_ under the lock, then publish the event outside the lock.
    // Calling EventBus::publish() while holding mutex_ risks a deadlock if any
    // subscriber calls session.update() on the same Session object.
    nlohmann::json wire;
    {
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
        wire = info_.to_json();
    }  // lock released here

    // T1: Emit session.updated so ACP / UI consumers stay in sync.
    turbot::core::EventBus::instance().publish(
        SessionInfoUpdatedEvent::kEventName,
        SessionInfoUpdatedEvent{wire});

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

    auto& store = SessionStore::instance();
    if (!store.is_initialized()) {
        TURBOT_LOG_WARN("Session::compact: store not initialised, skipping");
        return false;
    }

    // ── Step 1: load all messages from DB as JSON ──────────────────────────
    // Use a large limit to ensure we get all messages; 0 offset = from the start.
    auto msg_jsons = store.list_messages(info_.id, 10'000, 0);
    if (msg_jsons.empty()) {
        TURBOT_LOG_DEBUG("Session::compact: no messages to compact for session {}",
                         info_.id);
        return false;
    }

    // ── Step 2: deserialise JSON → Message objects ─────────────────────────
    // db=nullptr is fine here: we only read data, no persistence needed.
    std::vector<turbot::core::Message> messages;
    messages.reserve(msg_jsons.size());
    for (const auto& j : msg_jsons) {
        try {
            messages.push_back(turbot::core::Message::from_json(j, nullptr));
        } catch (const std::exception& e) {
            TURBOT_LOG_WARN("Session::compact: skipping malformed message JSON: {}",
                            e.what());
        }
    }
    if (messages.empty()) return false;

    // ── Step 3: mark session as Compacting ────────────────────────────────
    const int64_t now = current_timestamp();
    const SessionState prev_state = info_.state;
    info_.state = SessionState::Compacting;
    info_.time_compacting = now;
    info_.time_updated    = now;

    // ── Step 4: run the compaction algorithm ──────────────────────────────
    // Guard: if compact() throws, restore prev_state so the session is not
    // permanently stuck in Compacting.
    SessionCompaction sc;
    CompactionConfig cfg;  // defaults: overflow_threshold=0.9, target_ratio=0.5
    CompactionResult result;
    try {
        result = sc.compact(messages, cfg);
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR(
            "Session::compact: compaction algorithm threw an exception for session {}: {}; "
            "aborting compaction", info_.id, e.what());
        info_.state        = prev_state;
        info_.time_updated = current_timestamp();
        return false;
    }

    if (result.removed_ids.empty()) {
        // Nothing was compacted (too few messages or already compact).
        TURBOT_LOG_DEBUG("Session::compact: nothing to remove for session {}",
                         info_.id);
        info_.state         = prev_state;
        info_.time_updated  = current_timestamp();
        return false;
    }

    TURBOT_LOG_INFO(
        "Session::compact: removing {} messages, retaining {} "
        "(original_tokens={}, compressed_tokens={}, ratio={:.2f}) for session {}",
        result.removed_ids.size(), result.retained_ids.size(),
        result.original_tokens, result.compressed_tokens,
        result.compression_ratio, info_.id);

    // ── Step 5: delete removed messages from the DB ───────────────────────
    if (!store.delete_messages_by_ids(info_.id, result.removed_ids)) {
        TURBOT_LOG_ERROR(
            "Session::compact: failed to delete messages for session {}; "
            "aborting compaction", info_.id);
        info_.state        = prev_state;
        info_.time_updated = current_timestamp();
        return false;
    }

    // ── Step 6: insert summary message ────────────────────────────────────
    if (!result.summary.empty()) {
        nlohmann::json summary_msg = {
            {"id",          fmt::format("cmp_{}", now)},
            {"session_id",  info_.id},
            {"role",        "user"},
            {"time_created", now},
            {"time_updated", now},
            {"agent",       "compaction"},
            {"model_id",    ""},
            {"provider_id", ""},
            {"cost",        0.0},
            {"tokens",      {{"input",0},{"output",0},
                             {"cache_read",0},{"cache_create",0}}},
            {"summary",     true},
            {"parts", nlohmann::json::array({
                {{"type", "text"}, {"content", result.summary}}
            })}
        };
        if (!store.save_message(info_.id, summary_msg)) {
            TURBOT_LOG_WARN(
                "Session::compact: failed to persist summary message "
                "for session {}; compaction still counts as successful",
                info_.id);
        }
    }

    // ── Step 7: persist updated session state ─────────────────────────────
    info_.state        = SessionState::Active;   // compaction complete
    info_.time_updated = current_timestamp();
    if (!store.save(info_)) {
        TURBOT_LOG_WARN(
            "Session::compact: failed to persist session state for {}",
            info_.id);
    }

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

bool Session::set_summary(const SessionSummary& summary) {
    if (!mutex_) return false;
    std::lock_guard<std::mutex> lock(*mutex_);

    info_.summary      = summary;
    info_.time_updated = current_timestamp();

    // Persist to DB — aligned with OpenCode Session.setSummary(sessionID, summary).
    // Return the save result so callers know if persistence succeeded.
    return SessionStore::instance().save(info_);
}

} // namespace turbot::core::session
