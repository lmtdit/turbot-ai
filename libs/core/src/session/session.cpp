#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_store.hpp>
#include <turbot/core/snapshot/snapshot.hpp>
#include <turbot/core/common/logger.hpp>
#include <fmt/format.h>
#include <chrono>
#include <mutex>
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
    // Generate an OpenCode-compatible session ID.
    //
    // Format: "ses_" + 12 hex chars (6 bytes big-endian timestamp) + 14 base62 chars
    //
    // The timestamp portion is  ~(ms_timestamp * 0x1000 + per-ms counter) which
    // gives descending sort order (newest sessions sort first), matching OpenCode's
    // Identifier.descending("session") scheme.
    using namespace std::chrono;

    // Monotonic counter per millisecond — prevents identical IDs within the same ms.
    static std::mutex id_mutex_;
    static int64_t   last_ts_{0};
    static int32_t   counter_{0};

    int64_t ts;
    int32_t cnt;
    {
        std::lock_guard<std::mutex> lock(id_mutex_);
        ts = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        if (ts != last_ts_) {
            last_ts_ = ts;
            counter_ = 0;
        }
        cnt = ++counter_;
    }

    // Combine timestamp + counter, then bitwise-NOT for descending order.
    uint64_t val = static_cast<uint64_t>(ts) * 0x1000ULL + static_cast<uint64_t>(cnt);
    val = ~val;

    // Extract 6 bytes big-endian → 12 hex chars.
    uint8_t bytes[6];
    for (int i = 0; i < 6; ++i) {
        bytes[i] = static_cast<uint8_t>((val >> (40 - 8 * i)) & 0xFF);
    }
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 6; ++i) {
        oss << std::setw(2) << static_cast<int>(bytes[i]);
    }

    // 14 base62 random chars — use std::random_device directly (closer to
    // OpenCode's crypto.randomBytes) rather than seeding a deterministic PRNG.
    static constexpr const char kBase62[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static thread_local std::random_device s_rd;  // reuse across calls
    std::uniform_int_distribution<int> dis(0, 61);

    std::string rand_part;
    rand_part.reserve(14);
    for (int i = 0; i < 14; ++i) {
        rand_part += kBase62[dis(s_rd)];
    }

    return "ses_" + oss.str() + rand_part;
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
    
    // Perform compaction logic here.
    // TODO(sub-03): Implement actual context-window compaction. Until then return
    // false so callers know compaction did not occur, rather than silently succeeding.
    info_.state = prev_state;  // restore — compaction not yet implemented
    return false;
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
