// session_summary.cpp — OpenCode `session/summary.ts` C++ equivalent
//
// Aligned with: opencode/packages/opencode/src/session/summary.ts
// Key responsibilities:
//   1. summarize(session_id, message_id) — compute git diff stats after LLM run,
//      persist via Session::set_summary(), publish SessionDiffEvent.
//   2. diff(session_id) — read cached diff stats.
//   3. compute_diff(messages) — scan messages for step-start/finish snapshots, diff them.

#include <turbot/core/session/session_summary.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_store.hpp>
#include <turbot/core/event/event_bus.hpp>
#include <turbot/core/common/logger.hpp>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <sys/wait.h>

namespace turbot::core::session {

// ──────────────────────────────────────────────────────────────────────────────
// FileDiffStat
// ──────────────────────────────────────────────────────────────────────────────

nlohmann::json FileDiffStat::to_json() const {
    nlohmann::json j;
    j["file"]      = file;
    j["additions"] = additions;
    j["deletions"] = deletions;
    if (binary) j["binary"] = true;
    return j;
}

FileDiffStat FileDiffStat::from_json(const nlohmann::json& j) {
    FileDiffStat s;
    s.file      = j.value("file",      std::string{});
    s.additions = j.value("additions", 0);
    s.deletions = j.value("deletions", 0);
    s.binary    = j.value("binary",    false);
    return s;
}

// ──────────────────────────────────────────────────────────────────────────────
// SessionSummaryService — singleton
// ──────────────────────────────────────────────────────────────────────────────

SessionSummaryService& SessionSummaryService::instance() {
    static SessionSummaryService inst;
    return inst;
}

// ──────────────────────────────────────────────────────────────────────────────
// unquote_git_path — mirrors `unquoteGitPath()` in summary.ts
//
// Git wraps file paths that contain non-ASCII or special characters in double
// quotes with C-style escape sequences:
//   "dir/\303\274ber.txt"  →  "dir/über.txt"   (octal bytes → UTF-8)
//   "file with spaces"     →  unchanged          (no quoting)
// ──────────────────────────────────────────────────────────────────────────────

std::string SessionSummaryService::unquote_git_path(const std::string& input) {
    if (input.empty() || input.front() != '"' || input.back() != '"')
        return input;

    const std::string body = input.substr(1, input.size() - 2);
    std::string result;
    result.reserve(body.size());

    for (std::size_t i = 0; i < body.size(); ++i) {
        const char c = body[i];
        if (c != '\\') {
            result += c;
            continue;
        }
        // Escape sequence
        if (i + 1 >= body.size()) {
            result += '\\';
            continue;
        }
        const char next = body[i + 1];
        // Octal: \NNN (1-3 octal digits)
        if (next >= '0' && next <= '7') {
            std::size_t j = i + 1;
            unsigned val = 0;
            int count = 0;
            while (j < body.size() && body[j] >= '0' && body[j] <= '7' && count < 3) {
                val = val * 8 + static_cast<unsigned>(body[j] - '0');
                ++j;
                ++count;
            }
            result += static_cast<char>(val & 0xFF);
            i = j - 1;
            continue;
        }
        // Single-char escapes
        switch (next) {
            case 'n':  result += '\n'; break;
            case 'r':  result += '\r'; break;
            case 't':  result += '\t'; break;
            case 'b':  result += '\b'; break;
            case 'f':  result += '\f'; break;
            case 'v':  result += '\v'; break;
            case '\\': result += '\\'; break;
            case '"':  result += '"';  break;
            default:   result += next; break;
        }
        ++i;
    }
    return result;
}

// ──────────────────────────────────────────────────────────────────────────────
// diff_snapshots — run `git diff --numstat <from> <to>` inside the session dir
//
// Mirrors `Snapshot.diffFull(from, to)` from opencode/src/snapshot/index.ts.
// We use --numstat (machine-readable: "<added>\t<deleted>\t<file>") for binary
// safety and simple parsing.  Binary files are listed as "-\t-\t<file>".
// ──────────────────────────────────────────────────────────────────────────────

std::vector<FileDiffStat> SessionSummaryService::diff_snapshots(
        const std::string& from_snapshot,
        const std::string& to_snapshot) const {
    std::vector<FileDiffStat> result;
    if (from_snapshot.empty() || to_snapshot.empty()) return result;
    if (from_snapshot == to_snapshot) return result;

    // Build command: git diff --numstat <from> <to>
    // Snapshot IDs in Turbot correspond to git commit/tree hashes or
    // tracked snapshot names stored by SnapshotManager. If SnapshotManager
    // does not directly produce git object IDs, fall back to an empty result.
    // This implementation assumes the caller resolves real git refs externally
    // and passes them as from/to.  For in-process snapshots (PatchResult),
    // use compute_diff(messages) instead which scans part metadata.
    const std::string cmd = "git diff --numstat '" + from_snapshot + "' '" + to_snapshot + "' 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r"); // NOLINT(cert-env33-c)
    if (!pipe) return result;

    char buf[4096];
    std::string output;
    while (std::fgets(buf, sizeof(buf), pipe) != nullptr) {
        output += buf;
    }
    const int raw = pclose(pipe);
    const int exit_code = WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
    if (exit_code != 0) return result;

    // Parse --numstat lines: "<added>\t<deleted>\t<file>"
    std::istringstream ss(output);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        auto tab1 = line.find('\t');
        if (tab1 == std::string::npos) continue;
        auto tab2 = line.find('\t', tab1 + 1);
        if (tab2 == std::string::npos) continue;

        FileDiffStat stat;
        const std::string add_str = line.substr(0, tab1);
        const std::string del_str = line.substr(tab1 + 1, tab2 - tab1 - 1);
        stat.file      = unquote_git_path(line.substr(tab2 + 1));
        stat.binary    = (add_str == "-");
        stat.additions = stat.binary ? 0 : std::stoi(add_str);
        stat.deletions = stat.binary ? 0 : std::stoi(del_str);
        result.push_back(std::move(stat));
    }
    return result;
}

// ──────────────────────────────────────────────────────────────────────────────
// compute_diff — mirrors `SessionSummary.computeDiff({ messages })`
//
// Scans message JSON objects for assistant parts with type "step-start" (has
// a "snapshot" field) and "step-finish" (has a "snapshot" field).  Uses the
// earliest step-start snapshot as `from` and the latest step-finish snapshot
// as `to`, then calls diff_snapshots().
// ──────────────────────────────────────────────────────────────────────────────

std::vector<FileDiffStat> SessionSummaryService::compute_diff(
        const std::vector<nlohmann::json>& messages) const {
    std::string from_snapshot;
    std::string to_snapshot;

    for (const auto& msg : messages) {
        // Each message has a "parts" array
        if (!msg.contains("parts") || !msg["parts"].is_array()) continue;
        for (const auto& part : msg["parts"]) {
            if (!part.is_object()) continue;
            const std::string type = part.value("type", std::string{});

            if (type == "step-start" && from_snapshot.empty()) {
                const std::string snap = part.value("snapshot", std::string{});
                if (!snap.empty()) from_snapshot = snap;
            }

            if (type == "step-finish") {
                const std::string snap = part.value("snapshot", std::string{});
                if (!snap.empty()) to_snapshot = snap;
            }
        }
    }

    if (from_snapshot.empty() || to_snapshot.empty()) return {};
    return diff_snapshots(from_snapshot, to_snapshot);
}

// ──────────────────────────────────────────────────────────────────────────────
// diff — read cached diffs for a session (mirrors `SessionSummary.diff()`)
// ──────────────────────────────────────────────────────────────────────────────

std::vector<FileDiffStat> SessionSummaryService::diff(
        const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = diff_cache_.find(session_id);
    if (it == diff_cache_.end()) return {};
    return it->second;
}

// ──────────────────────────────────────────────────────────────────────────────
// summarize — mirrors `SessionSummary.summarize({ sessionID, messageID })`
//
// 1. Load all messages for the session.
// 2. Compute diff stats via compute_diff().
// 3. Persist via Session::set_summary().
// 4. Cache diffs.
// 5. Publish SessionDiffEvent on the EventBus.
// ──────────────────────────────────────────────────────────────────────────────

void SessionSummaryService::summarize(
        const std::string& session_id,
        const std::string& /*message_id*/) {
    // Load messages from the session store
    auto& store = SessionStore::instance();
    if (!store.is_initialized()) {
        TURBOT_LOG_WARN("SessionSummaryService::summarize: store not initialized "
                        "(session={})", session_id);
        return;
    }

    // Large limit to get all messages
    const auto msg_jsons = store.list_messages(session_id, 10'000, 0);

    // Compute diff
    const auto diffs = compute_diff(msg_jsons);

    // Persist summary statistics to the session row
    auto session_opt = Session::get(session_id);
    if (!session_opt) {
        TURBOT_LOG_WARN("SessionSummaryService::summarize: session not found "
                        "(session={})", session_id);
        return;
    }

    SessionSummary summary;
    for (const auto& d : diffs) {
        summary.additions += d.additions;
        summary.deletions += d.deletions;
        summary.files     += 1;
    }
    // Serialize diffs array into summary.diffs JSON
    if (!diffs.empty()) {
        nlohmann::json diffs_json = nlohmann::json::array();
        for (const auto& d : diffs) diffs_json.push_back(d.to_json());
        summary.diffs = std::move(diffs_json);
    }

    if (!session_opt->set_summary(summary)) {
        TURBOT_LOG_WARN("SessionSummaryService::summarize: set_summary failed "
                        "(session={})", session_id);
    }

    // Cache the diff result
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        diff_cache_[session_id] = diffs;
    }

    // Publish SessionDiffEvent (mirrors Bus.publish(Session.Event.Diff, {...}))
    turbot::core::EventBus::instance().publish(
        SessionDiffEvent::kEventName,
        SessionDiffEvent{session_id, diffs});

    TURBOT_LOG_INFO("SessionSummaryService::summarize: session={} "
                    "files={} additions={} deletions={}",
                    session_id, summary.files, summary.additions, summary.deletions);
}

} // namespace turbot::core::session
