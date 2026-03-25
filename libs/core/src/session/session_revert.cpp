// session_revert.cpp
// Aligned with: opencode/packages/opencode/src/session/revert.ts
//
// Provides the three high-level SessionRevert operations:
//   revert()   — roll back file changes and record the revert boundary
//   unrevert() — undo a previous revert
//   cleanup()  — commit the revert by deleting messages/parts after the boundary

#include <turbot/core/session/session_revert.hpp>
#include <turbot/core/session/session_store.hpp>
#include <turbot/core/session/session_events.hpp>
#include <turbot/core/session/session_summary.hpp>
#include <turbot/core/snapshot/snapshot.hpp>
#include <turbot/core/event/event_bus.hpp>
#include <turbot/core/common/logger.hpp>
#include <algorithm>
#include <string>
#include <vector>

namespace turbot::core::session {
namespace SessionRevert {

// ---------------------------------------------------------------------------
// revert() — mirrors OpenCode SessionRevert.revert(input)
// ---------------------------------------------------------------------------

std::optional<SessionInfo> revert(const RevertInput& input) {
    auto& store = SessionStore::instance();
    if (!store.is_initialized()) return std::nullopt;

    // 1. Load the session.
    auto session_json = store.find_by_id(input.session_id);
    if (!session_json) {
        TURBOT_LOG_WARN("SessionRevert::revert: session not found: {}", input.session_id);
        return std::nullopt;
    }
    SessionInfo session_info;
    try {
        session_info = SessionInfo::from_json(*session_json);
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionRevert::revert: failed to parse session {}: {}",
                         input.session_id, e.what());
        return std::nullopt;
    }

    // 2. Walk all messages, find the revert boundary and collect patches after it.
    //    Mirrors the for-loop in revert.ts lines 32-55.
    auto all_messages = store.list_messages(input.session_id, 0 /*unlimited*/, 0);

    bool revert_found = false;
    std::string revert_message_id;
    std::optional<std::string> revert_part_id;
    std::vector<nlohmann::json> patches;  // patch-type parts collected after the boundary

    // Track the last user message seen (for boundary messageID correction in revert.ts L48)
    std::string last_user_message_id;

    for (const auto& msg_json : all_messages) {
        if (!msg_json.is_object()) continue;

        const std::string msg_id = msg_json.value("id", std::string{});
        const std::string role   = msg_json.value("role", std::string{});

        if (role == "user") {
            last_user_message_id = msg_id;
        }

        // Collect parts from the message JSON.
        const auto& parts_val = msg_json.contains("parts") ? msg_json["parts"]
                                                            : nlohmann::json::array();
        const std::vector<nlohmann::json> parts =
            parts_val.is_array() ? parts_val.get<std::vector<nlohmann::json>>()
                                 : std::vector<nlohmann::json>{};

        if (revert_found) {
            // After the boundary: collect patch-type parts (mirrors revert.ts L37-41)
            for (const auto& part : parts) {
                if (part.value("type", std::string{}) == "patch") {
                    patches.push_back(part);
                }
            }
            continue;
        }

        // Before/at the boundary: look for the revert point.
        std::vector<nlohmann::json> remaining;  // meaningful parts seen before the boundary
        for (const auto& part : parts) {
            const std::string type   = part.value("type", std::string{});
            const std::string pid    = part.value("id", std::string{});

            if (!revert_found) {
                // Check if this part/message is the revert target (revert.ts L43-54)
                bool whole_msg_match = (msg_id == input.message_id && !input.part_id);
                bool part_match      = (input.part_id && pid == *input.part_id);

                if (whole_msg_match || part_match) {
                    // Decide the effective partID: if no useful parts remain in the
                    // message we treat it as a whole-message revert (partID = nullopt).
                    bool has_useful = false;
                    for (const auto& r : remaining) {
                        const std::string rt = r.value("type", std::string{});
                        if (rt == "text" || rt == "tool") {
                            has_useful = true;
                            break;
                        }
                    }
                    std::optional<std::string> eff_part_id =
                        (has_useful && input.part_id) ? input.part_id : std::optional<std::string>{};

                    // If partID is nullopt (whole-message revert) and we have a last
                    // user message, the boundary becomes the last user message
                    // (mirrors revert.ts L47-48).
                    if (!eff_part_id && !last_user_message_id.empty()) {
                        revert_message_id = last_user_message_id;
                    } else {
                        revert_message_id = msg_id;
                    }
                    revert_part_id = eff_part_id;
                    revert_found   = true;
                }
                remaining.push_back(part);
            }
        }
    }

    if (!revert_found) {
        TURBOT_LOG_WARN("SessionRevert::revert: revert point not found in session {}",
                        input.session_id);
        return session_info;  // Nothing to revert (matches revert.ts L79)
    }

    // 3. Roll back file changes collected in patches (mirrors Snapshot.revert(patches)).
    if (!patches.empty()) {
        auto& sm = turbot::core::snapshot::SnapshotManager::instance();
        // Convert JSON patch parts to PatchResult objects and roll back each one.
        // Patch parts have "type":"patch" and a "patch" field with the PatchResult JSON.
        for (const auto& patch_part : patches) {
            if (!patch_part.contains("patch")) continue;
            try {
                auto pr = turbot::core::snapshot::PatchResult::from_json(patch_part["patch"]);
                sm.rollback_patch(pr);
            } catch (const std::exception& e) {
                TURBOT_LOG_WARN("SessionRevert::revert: rollback_patch failed: {}", e.what());
            }
        }
    }

    // 6. Compute per-file diff stats for messages in the reverted range.
    //    Mirrors SessionSummary.computeDiff({ messages: rangeMessages }) in revert.ts L63.
    auto& summary_svc = SessionSummaryService::instance();

    // rangeMessages = all messages with id >= revert_message_id
    std::vector<nlohmann::json> range_messages;
    for (const auto& m : all_messages) {
        if (m.value("id", std::string{}) >= revert_message_id) {
            range_messages.push_back(m);
        }
    }
    const auto diffs = summary_svc.compute_diff(range_messages);

    // 7. Build RevertParams and call Session::revert() to persist (mirrors Session.setRevert).
    //    We convert patch-parts back to PatchResult for pre_patch storage.
    //    The pre_patch is used by unrevert() to restore the working directory.
    //    Here we store the first patch as a representative (full rollback already done).
    std::vector<turbot::core::snapshot::PatchResult> patch_results;
    for (const auto& pp : patches) {
        if (!pp.contains("patch")) continue;
        try {
            patch_results.push_back(
                turbot::core::snapshot::PatchResult::from_json(pp["patch"]));
        } catch (...) {}
    }

    RevertParams rp;
    rp.message_id = revert_message_id;
    rp.part_id    = revert_part_id;
    rp.patches    = patch_results;

    auto session_opt = Session::get(input.session_id);
    if (!session_opt) {
        TURBOT_LOG_WARN("SessionRevert::revert: session disappeared: {}", input.session_id);
        return std::nullopt;
    }

    // Apply revert on the Session object (persists RevertInfo + summary).
    session_opt->revert(rp);

    // Also update snapshot_id and diff in the session's revert info if available.
    // (Session::revert() stores basic params; we patch the extra fields here.)
    // Re-load session after revert() call to get the persisted state.
    auto updated_json = store.find_by_id(input.session_id);
    if (!updated_json) return std::nullopt;

    SessionInfo updated_info;
    try {
        updated_info = SessionInfo::from_json(*updated_json);
    } catch (...) {
        return std::nullopt;
    }

    // 8. Publish SessionDiffEvent (mirrors Bus.publish(Session.Event.Diff, ...) in revert.ts L65).
    turbot::core::EventBus::instance().publish(
        SessionDiffEvent::kEventName,
        SessionDiffEvent{input.session_id, diffs});

    // 9. Also publish SessionInfoUpdatedEvent so UI/ACP subscribers see the new revert state.
    turbot::core::EventBus::instance().publish(
        SessionInfoUpdatedEvent::kEventName,
        SessionInfoUpdatedEvent{updated_info.to_json()});

    return updated_info;
}

// ---------------------------------------------------------------------------
// unrevert() — mirrors OpenCode SessionRevert.unrevert({ sessionID })
// ---------------------------------------------------------------------------

std::optional<SessionInfo> unrevert(const std::string& session_id) {
    auto& store = SessionStore::instance();
    if (!store.is_initialized()) return std::nullopt;

    // 1. Load the session.
    auto session_json = store.find_by_id(session_id);
    if (!session_json) return std::nullopt;

    SessionInfo info;
    try {
        info = SessionInfo::from_json(*session_json);
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionRevert::unrevert: failed to parse session {}: {}",
                         session_id, e.what());
        return std::nullopt;
    }

    // 2. Nothing to unrevert (mirrors revert.ts L86).
    if (!info.revert) return info;

    // 3. Restore file state: Session::unrevert() already handles rollback_patch()
    //    internally (using the pre_patch stored in RevertInfo).  We just delegate
    //    rather than duplicating the logic here.
    //    (Mirrors Snapshot.restore(revert.snapshot) in revert.ts L87.)

    // 4. Clear revert info (mirrors Session.clearRevert(sessionID) in revert.ts L88).
    auto session_opt = Session::get(session_id);
    if (!session_opt) return std::nullopt;

    session_opt->unrevert();  // clears info_.revert and saves

    // Reload and return updated info.
    auto updated = store.find_by_id(session_id);
    if (!updated) return std::nullopt;

    SessionInfo result;
    try {
        result = SessionInfo::from_json(*updated);
    } catch (...) {
        return std::nullopt;
    }

    turbot::core::EventBus::instance().publish(
        SessionInfoUpdatedEvent::kEventName,
        SessionInfoUpdatedEvent{result.to_json()});

    return result;
}

// ---------------------------------------------------------------------------
// cleanup() — mirrors OpenCode SessionRevert.cleanup(session)
// ---------------------------------------------------------------------------

void cleanup(const SessionInfo& session) {
    if (!session.revert) return;  // No pending revert — no-op (revert.ts L92)

    const std::string& session_id  = session.id;
    const std::string& message_id  = session.revert->message_id;
    const auto&        part_id_opt = session.revert->part_id;

    auto& store = SessionStore::instance();
    if (!store.is_initialized()) return;

    // Load all messages for the session.
    auto all_messages = store.list_messages(session_id, 0, 0);

    // Categorise messages: preserve (< messageID), target (== messageID), remove (> messageID).
    // Mirrors the for-loop in revert.ts lines 99-114.
    std::vector<std::string> remove_msg_ids;
    std::string              target_msg_id;

    // We also need the target message's parts list for partial (partID) cleanup.
    nlohmann::json target_msg_json;

    for (const auto& msg : all_messages) {
        const std::string mid = msg.value("id", std::string{});
        if (mid < message_id) {
            // preserve — nothing to do
        } else if (mid > message_id) {
            remove_msg_ids.push_back(mid);
        } else {
            // mid == message_id
            if (!part_id_opt) {
                // Whole-message revert: the boundary message itself is also removed.
                remove_msg_ids.push_back(mid);
            } else {
                // Partial revert: boundary message is preserved but parts after partID are removed.
                target_msg_id  = mid;
                target_msg_json = msg;
            }
        }
    }

    // 1. Delete messages after the boundary + publish MessageRemovedEvent.
    if (!remove_msg_ids.empty()) {
        store.delete_messages_by_ids(session_id, remove_msg_ids);
        for (const auto& mid : remove_msg_ids) {
            turbot::core::EventBus::instance().publish(
                MessageRemovedEvent::kEventName,
                MessageRemovedEvent{session_id, mid});
        }
    }

    // 2. For partial revert: remove parts starting from partID in the boundary message.
    //    Mirrors revert.ts lines 119-134.
    if (part_id_opt && !target_msg_id.empty() && target_msg_json.is_object()) {
        const std::string& target_part_id = *part_id_opt;
        auto& parts_val = target_msg_json["parts"];
        if (parts_val.is_array()) {
            const auto& parts = parts_val;

            // Find the index of the target part.
            int remove_start = -1;
            for (int i = 0; i < static_cast<int>(parts.size()); ++i) {
                if (parts[static_cast<size_t>(i)].value("id", std::string{}) == target_part_id) {
                    remove_start = i;
                    break;
                }
            }

            if (remove_start >= 0) {
                std::vector<std::string> remove_part_ids;
                for (int i = remove_start; i < static_cast<int>(parts.size()); ++i) {
                    const std::string pid = parts[static_cast<size_t>(i)].value("id", std::string{});
                    if (!pid.empty()) remove_part_ids.push_back(pid);
                }

                if (!remove_part_ids.empty()) {
                    store.delete_parts_by_ids(remove_part_ids);
                    for (const auto& pid : remove_part_ids) {
                        turbot::core::EventBus::instance().publish(
                            PartRemovedEvent::kEventName,
                            PartRemovedEvent{session_id, target_msg_id, pid});
                    }
                }
            }
        }
    }

    // 3. Clear the revert info (mirrors Session.clearRevert(sessionID) in revert.ts L136).
    auto session_opt = Session::get(session_id);
    if (session_opt) {
        session_opt->cleanup_revert();
    }
}

} // namespace SessionRevert
} // namespace turbot::core::session
