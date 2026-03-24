// session_cmd.cpp - CLI command to manage sessions
// Aligns with OpenCode `opencode session` command capability

#include <turbot/core/common/logger.hpp>
#include <turbot/core/session/session.hpp>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <iostream>
#include <string>
#include <chrono>

namespace turbot::cli {

/// Format timestamp to human-readable string
static std::string format_time(int64_t timestamp) {
    if (timestamp == 0) return "N/A";
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::from_time_t(timestamp);
    auto diff = std::chrono::duration_cast<std::chrono::hours>(now - time).count();
    
    if (diff < 24) {
        // Show time for today
        std::time_t t = timestamp;
        char buf[32];
        std::strftime(buf, sizeof(buf), "%H:%M", std::localtime(&t));
        return buf;
    } else if (diff < 24 * 7) {
        // Show day name for this week
        std::time_t t = timestamp;
        char buf[32];
        std::strftime(buf, sizeof(buf), "%a %H:%M", std::localtime(&t));
        return buf;
    } else {
        // Show date for older
        std::time_t t = timestamp;
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
        return buf;
    }
}

/// List all sessions
int list_sessions_cmd(int limit, bool json_format) {
    auto sessions = core::session::Session::list("");
    
    if (sessions.empty()) {
        fmt::print("No sessions found.\n\n");
        fmt::print("To create a session:\n");
        fmt::print("  turbot-cli run\n");
        return 0;
    }
    
    if (limit > 0 && static_cast<int>(sessions.size()) > limit) {
        sessions.resize(limit);
    }
    
    if (json_format) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& session : sessions) {
            const auto& info = session.info();
            arr.push_back({
                {"id", info.id},
                {"title", info.title},
                {"project_id", info.project_id},
                {"directory", info.directory},
                {"state", core::session::session_state_to_string(info.state)},
                {"created", info.time_created},
                {"updated", info.time_updated}
            });
        }
        fmt::print("{}\n", arr.dump(2));
    } else {
        // Calculate column widths
        size_t max_id_width = 20;
        size_t max_title_width = 30;
        
        for (const auto& session : sessions) {
            max_id_width = std::max(max_id_width, session.id().length() + 2);
            max_title_width = std::max(max_title_width, 
                std::min(session.info().title.length() + 2, size_t(40)));
        }
        
        fmt::print("{:<{}}  {:<{}}  {:<10}  {}\n", 
                   "Session ID", max_id_width, 
                   "Title", max_title_width, 
                   "State", "Updated");
        fmt::print("{}\n", std::string(max_id_width + max_title_width + 25, '-'));
        
        for (const auto& session : sessions) {
            const auto& info = session.info();
            std::string truncated_title = info.title.substr(0, max_title_width - 2);
            
            fmt::print("{:<{}}  {:<{}}  {:<10}  {}\n",
                info.id.substr(0, max_id_width - 1), max_id_width,
                truncated_title, max_title_width,
                core::session::session_state_to_string(info.state),
                format_time(info.time_updated));
        }
        
        fmt::print("\n{} session(s) found.\n", sessions.size());
    }
    
    return 0;
}

/// Show session details
int show_session(const std::string& session_id) {
    auto session_opt = core::session::Session::get(session_id);
    
    if (!session_opt) {
        fmt::print(stderr, "Session not found: {}\n", session_id);
        return 1;
    }
    
    const auto& session = *session_opt;
    const auto& info = session.info();
    
    fmt::print("Session: {}\n", info.id);
    fmt::print("{}\n", std::string(60, '-'));
    fmt::print("  Title:     {}\n", info.title);
    fmt::print("  Project:   {}\n", info.project_id);
    fmt::print("  Directory: {}\n", info.directory);
    fmt::print("  State:     {}\n", core::session::session_state_to_string(info.state));
    fmt::print("  Created:   {}\n", format_time(info.time_created));
    fmt::print("  Updated:   {}\n", format_time(info.time_updated));
    
    if (info.parent_id) {
        fmt::print("  Parent:    {}\n", *info.parent_id);
    }
    
    if (info.revert) {
        fmt::print("\nRevert Status:\n");
        fmt::print("  Message:   {}\n", info.revert->message_id);
        if (info.revert->part_id) {
            fmt::print("  Part:      {}\n", *info.revert->part_id);
        }
    }
    
    // Show message count
    auto messages = session.messages(1, 0);  // Just get count
    fmt::print("\n  Messages:  {}\n", messages.size());
    
    return 0;
}

/// Delete a session
int delete_session(const std::string& session_id) {
    // First check if session exists
    auto session_opt = core::session::Session::get(session_id);
    
    if (!session_opt) {
        fmt::print(stderr, "Session not found: {}\n", session_id);
        return 1;
    }
    
    const auto& session = *session_opt;
    std::string title = session.info().title;
    
    if (core::session::Session::remove(session_id)) {
        fmt::print("Session '{}' deleted.\n", session_id);
        fmt::print("  Title was: {}\n", title);
        return 0;
    } else {
        fmt::print(stderr, "Failed to delete session: {}\n", session_id);
        return 1;
    }
}

/// Archive a session
int archive_session(const std::string& session_id) {
    auto session_opt = core::session::Session::get(session_id);
    
    if (!session_opt) {
        fmt::print(stderr, "Session not found: {}\n", session_id);
        return 1;
    }
    
    auto& session = *session_opt;
    
    if (session.archive()) {
        fmt::print("Session '{}' archived.\n", session_id);
        return 0;
    } else {
        fmt::print(stderr, "Failed to archive session: {}\n", session_id);
        return 1;
    }
}

/// Restore a session
int restore_session(const std::string& session_id) {
    auto session_opt = core::session::Session::get(session_id);
    
    if (!session_opt) {
        fmt::print(stderr, "Session not found: {}\n", session_id);
        return 1;
    }
    
    auto& session = *session_opt;
    
    if (session.restore()) {
        fmt::print("Session '{}' restored.\n", session_id);
        return 0;
    } else {
        fmt::print(stderr, "Failed to restore session: {}\n", session_id);
        return 1;
    }
}

} // namespace turbot::cli
