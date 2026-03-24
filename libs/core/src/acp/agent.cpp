#include "turbot/core/acp/agent.hpp"

#include "turbot/core/agent/agent.hpp"
#include "turbot/core/common/logger.hpp"
#include "turbot/core/common/version.hpp"
#include "turbot/core/session/session.hpp"
#include "turbot/core/session/session_loop.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace turbot::core::acp {

// ---------------------------------------------------------------------------
// to_tool_kind (aligned with OpenCode toToolKind)
// ---------------------------------------------------------------------------

std::string to_tool_kind(const std::string& tool_name) {
    if (tool_name == "bash") return "execute";
    if (tool_name == "webfetch") return "fetch";
    if (tool_name == "edit" || tool_name == "patch" || tool_name == "write")
        return "edit";
    if (tool_name == "grep" || tool_name == "glob") return "search";
    // Known context7 tools (aligned with OpenCode toToolKind exact matches)
    if (tool_name == "context7_resolve_library_id" ||
        tool_name == "context7_get_library_docs")
        return "search";
    if (tool_name == "list" || tool_name == "read") return "read";
    return "other";
}

// ---------------------------------------------------------------------------
// TurbotACPAgent constructor
// ---------------------------------------------------------------------------

TurbotACPAgent::TurbotACPAgent(const std::string& default_cwd)
    : default_cwd_(default_cwd) {}

// ---------------------------------------------------------------------------
// initialize (aligned with OpenCode Agent.initialize)
// ---------------------------------------------------------------------------

InitializeResponse TurbotACPAgent::initialize(const InitializeRequest& req) {
    InitializeResponse resp;
    resp.protocol_version = req.protocol_version;

    // Build auth method
    AuthMethod auth;
    auth.id          = "turbot-login";
    auth.name        = "Turbot Login";
    auth.description = "Run `turbot auth login` in the terminal";

    // If client supports terminal-auth capability, add meta
    if (req.client_capabilities.contains("_meta")) {
        const auto& meta = req.client_capabilities["_meta"];
        if (meta.is_object() && meta.value("terminal-auth", false) == true) {
            auth.meta = {
                {"terminal-auth",
                 {{"command", "turbot"}, {"args", {"auth", "login"}},
                  {"label", "Turbot Login"}}}};
        }
    }

    resp.auth_methods = {auth};
    resp.agent_info   = {"Turbot",
                         std::string(turbot::core::Version::string())};
    return resp;
}

// ---------------------------------------------------------------------------
// new_session (aligned with OpenCode Agent.newSession)
// ---------------------------------------------------------------------------

nlohmann::json TurbotACPAgent::new_session(
    const NewSessionRequest& req,
    std::function<void(const nlohmann::json&)> on_update) {
    // Create underlying Turbot session
    session::CreateParams cp;
    cp.directory = req.cwd;
    cp.slug      = "acp-session";
    cp.title     = "ACP Session";

    auto sess_opt = session::Session::create(cp);
    if (!sess_opt) {
        throw std::runtime_error("Failed to create Turbot session");
    }
    const auto& sess = *sess_opt;
    const std::string session_id = sess.id();

    // Register in ACP session manager
    session_manager_.create(session_id, req.cwd, req.mcp_servers,
                            std::nullopt, req.model_id);

    // Asynchronous available_commands_update (aligned with OpenCode setTimeout(0))
    if (on_update) {
        on_update(build_available_commands_update(session_id));
    }

    return {
        {"sessionId", session_id},
        {"models",    get_available_models(req.cwd)},
        {"modes",     get_available_modes()},
    };
}

// ---------------------------------------------------------------------------
// load_session (aligned with OpenCode Agent.loadSession)
// ---------------------------------------------------------------------------

nlohmann::json TurbotACPAgent::load_session(
    const LoadSessionRequest& req,
    std::function<void(const nlohmann::json&)> on_update) {
    // Verify session exists
    auto sess_opt = session::Session::get(req.session_id);
    if (!sess_opt) {
        throw std::invalid_argument("Session not found: " + req.session_id);
    }
    const auto& sess = *sess_opt;

    // Register in ACP session manager
    session_manager_.load(req.session_id, req.cwd, req.mcp_servers,
                          std::nullopt, req.model_id);

    // Replay session history
    const auto history = sess.messages();
    if (on_update) {
        for (const auto& msg : history) {
            try {
                process_message(msg, on_update);
            } catch (const std::exception& /* ex */) {
                // Ignore replay errors for individual messages (best-effort,
                // aligned with OpenCode log.error + continue pattern)
            }
        }
        // Asynchronous available_commands_update (aligned with OpenCode setTimeout(0))
        on_update(build_available_commands_update(req.session_id));
    }

    // Collect models/modes
    nlohmann::json result = {
        {"sessionId", req.session_id},
        {"models",    get_available_models(req.cwd)},
        {"modes",     get_available_modes()},
    };

    // Build SessionInfo
    result["session"] = session_info_from_state(
        session_manager_.get(req.session_id), sess);

    return result;
}

// ---------------------------------------------------------------------------
// resume_session (aligned with OpenCode Agent.unstable_resumeSession)
// ---------------------------------------------------------------------------

nlohmann::json TurbotACPAgent::resume_session(
    const ResumeSessionRequest& req) {
    auto sess_opt = session::Session::get(req.session_id);
    if (!sess_opt) {
        throw std::invalid_argument("Session not found: " + req.session_id);
    }

    // Register lightweight state (no history replay)
    session_manager_.load(req.session_id, req.cwd, req.mcp_servers);

    return {
        {"sessionId", req.session_id},
        {"models",    get_available_models(req.cwd)},
        {"modes",     get_available_modes()},
    };
}

// ---------------------------------------------------------------------------
// list_sessions (aligned with OpenCode Agent.unstable_listSessions)
// Pagination: limit=100, cursor=Unix timestamp (seconds), desc order
// ---------------------------------------------------------------------------

nlohmann::json TurbotACPAgent::list_sessions(
    const ListSessionsRequest& req) {
    constexpr int LIMIT = 100;

    // Get all sessions (empty project_id = all projects)
    const auto all = session::Session::list("");

    // Sort descending by time_updated
    std::vector<session::Session> sorted(all.begin(), all.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const session::Session& a, const session::Session& b) {
                  return a.info().time_updated > b.info().time_updated;
              });

    // Apply cursor filter
    std::optional<int64_t> cursor_ts;
    if (req.cursor) {
        try {
            cursor_ts = std::stoll(*req.cursor);
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("malformed cursor '{}': {}", *req.cursor, e.what());
        } catch (...) {
            TURBOT_LOG_DEBUG("malformed cursor '{}': unknown error", *req.cursor);
        }
    }

    std::vector<session::Session> filtered;
    for (const auto& s : sorted) {
        if (!cursor_ts || s.info().time_updated < *cursor_ts) {
            filtered.push_back(s);
        }
    }

    const auto page_end =
        filtered.size() > static_cast<size_t>(LIMIT)
            ? filtered.begin() + LIMIT
            : filtered.end();

    nlohmann::json entries = nlohmann::json::array();
    for (auto it = filtered.begin(); it != page_end; ++it) {
        const auto& info = it->info();
        nlohmann::json entry = {
            {"sessionId", info.id},
            {"cwd",       info.directory},
            {"title",     info.title},
            {"updatedAt", std::to_string(info.time_updated)},
        };
        entries.push_back(std::move(entry));
    }

    nlohmann::json result = {{"sessions", entries}};

    // Next cursor
    if (filtered.size() > static_cast<size_t>(LIMIT)) {
        const auto& last = (filtered.begin() + LIMIT - 1)->info();
        result["nextCursor"] = std::to_string(last.time_updated);
    }

    return result;
}

// ---------------------------------------------------------------------------
// fork_session (aligned with OpenCode Agent.unstable_forkSession)
// ---------------------------------------------------------------------------

ForkSessionResponse TurbotACPAgent::fork_session(
    const ForkSessionRequest& req) {
    auto sess_opt = session::Session::get(req.session_id);
    if (!sess_opt) {
        throw std::invalid_argument("Session not found: " + req.session_id);
    }

    // Fork the underlying session
    session::ForkParams fp;
    fp.parent_id = req.session_id;
    fp.slug      = "acp-fork";
    fp.title     = "ACP Fork";

    auto fork_opt = session::Session::fork(fp);
    if (!fork_opt) {
        throw std::runtime_error("Failed to fork session: " + req.session_id);
    }
    const auto& fork_sess  = *fork_opt;
    const std::string fid  = fork_sess.id();

    // Register forked session
    session_manager_.create(fid, req.cwd, req.mcp_servers);

    ForkSessionResponse resp;
    resp.session.session_id = fid;
    resp.session.cwd        = req.cwd;
    return resp;
}

// ---------------------------------------------------------------------------
// prompt (aligned with OpenCode Agent.prompt – streaming via SessionLoop)
// ---------------------------------------------------------------------------

nlohmann::json TurbotACPAgent::prompt(
    const PromptRequest& req,
    std::function<void(const nlohmann::json&)> on_update) {

    // Retrieve ACP session state
    ACPSessionState* state = session_manager_.try_get(req.session_id);
    if (!state) {
        throw std::invalid_argument("Session not found: " + req.session_id);
    }

    // Retrieve underlying Turbot session
    auto sess_opt = session::Session::get(req.session_id);
    if (!sess_opt) {
        throw std::invalid_argument(
            "Turbot session not found: " + req.session_id);
    }

    // Apply model/mode overrides if provided
    if (req.model) {
        std::string provider_id, model_id;
        std::optional<std::string> variant;
        parse_model_string(*req.model, provider_id, model_id, variant);
        session_manager_.set_model(req.session_id, provider_id, model_id);
        session_manager_.set_variant(req.session_id, variant);
    }
    if (req.mode) {
        session_manager_.set_mode(req.session_id, *req.mode);
    }

    // Extract text prompt from prompt array
    std::string user_text;
    for (const auto& part : req.prompt) {
        if (part.value("type", "") == "text") {
            user_text += part.value("text", "");
        }
    }

    // Run the session loop with streaming callbacks
    session::SessionLoop loop(*sess_opt);

    // Register this loop so cancel() can stop it
    {
        std::lock_guard<std::mutex> lk(active_loops_mutex_);
        active_loops_[req.session_id] = &loop;
    }
    // RAII guard: always deregister when prompt() exits (normal or exception)
    struct LoopGuard {
        TurbotACPAgent& agent;
        const std::string& session_id;
        ~LoopGuard() {
            std::lock_guard<std::mutex> lk(agent.active_loops_mutex_);
            agent.active_loops_.erase(session_id);
        }
    } loop_guard{*this, req.session_id};

    // Set stream event callback → agent_message_chunk / agent_thought_chunk
    loop.set_on_stream_event([&on_update, &req](
                                 const turbot::core::StreamEvent& ev) {
        if (ev.type == turbot::core::StreamEventType::TextDelta) {
            on_update({{"sessionUpdate", "agent_message_chunk"},
                       {"sessionId",    req.session_id},
                       {"content",
                        {{"type", "text"}, {"text", ev.delta}}}});
        } else if (ev.type == turbot::core::StreamEventType::ReasoningDelta) {
            on_update({{"sessionUpdate", "agent_thought_chunk"},
                       {"sessionId",    req.session_id},
                       {"content",
                        {{"type", "text"}, {"text", ev.delta}}}});
        }
    });

    // Tool call input cache: call_id → raw input JSON (for diff generation in result callback)
    auto tool_inputs = std::make_shared<std::unordered_map<std::string, nlohmann::json>>();

    // Tool call callbacks → tool_call + tool_call_update
    loop.set_on_tool_call([&on_update, &req, tool_inputs](
                              const std::string& tool_name,
                              const std::string& call_id,
                              const nlohmann::json& input) {
        // Cache input for diff generation in the result callback
        (*tool_inputs)[call_id] = input;
        on_update({{"sessionUpdate", "tool_call"},
                   {"sessionId",    req.session_id},
                   {"toolCallId",   call_id},
                   {"title",        tool_name},
                   {"kind",         to_tool_kind(tool_name)},
                   {"status",       "pending"},
                   {"rawInput",     input},
                   {"locations",    nlohmann::json::array()}});
    });

    loop.set_on_tool_result([&on_update, &req, tool_inputs](
                                const std::string& tool_name,
                                const std::string& call_id,
                                const tool::ToolResult& result) {
        const std::string status =
            result.is_error ? "failed" : "completed";
        const std::string kind = to_tool_kind(tool_name);

        // Look up the original input for diff generation (best-effort)
        nlohmann::json raw_input = nlohmann::json::object();
        {
            auto it = tool_inputs->find(call_id);
            if (it != tool_inputs->end()) {
                raw_input = it->second;
                tool_inputs->erase(it);  // release after use
            }
}

        // todowrite completed → emit plan notification first (aligned with OpenCode)
        if (tool_name == "todowrite" && !result.is_error) {
            try {
                const auto todos = nlohmann::json::parse(result.output);
                if (todos.is_array()) {
                    nlohmann::json entries = nlohmann::json::array();
                    for (const auto& todo : todos) {
                        const std::string raw_status = todo.value("status", "pending");
                        // cancelled → completed (aligned with OpenCode)
                        const std::string plan_status =
                            (raw_status == "cancelled") ? "completed" : raw_status;
                        entries.push_back(
                            {{"priority", "medium"},
                             {"status",   plan_status},
                             {"content",  todo.value("content", "")}});
                    }
                    on_update({{"sessionUpdate", "plan"},
                               {"sessionId",    req.session_id},
                               {"entries",      entries}});
                }
            } catch (const nlohmann::json::parse_error& e) {
                TURBOT_LOG_DEBUG("todowrite parse error: {}", e.what());
            } catch (const std::exception& e) {
                TURBOT_LOG_DEBUG("todowrite processing error: {}", e.what());
            } catch (...) {
                TURBOT_LOG_DEBUG("todowrite unknown error");
            }
        }
    
        nlohmann::json content = nlohmann::json::array();
        content.push_back({{"type", "content"},
                            {"content", {{"type", "text"}, {"text", result.output}}}});
    
        // Edit tool completed: add diff block with real path and content (aligned with OpenCode)
        if (kind == "edit" && !result.is_error) {
            // Extract file path and diff content from the cached tool call input
            const std::string file_path  = raw_input.value("filePath", "");
            const std::string old_string = raw_input.value("oldString", "");
            const std::string new_string = raw_input.value("newString", "");
            content.push_back({{"type",    "diff"},
                               {"path",    file_path},
                               {"oldText", old_string},
                               {"newText", new_string}});
        }
    
        nlohmann::json update = {
            {"sessionUpdate", "tool_call_update"},
            {"sessionId",     req.session_id},
            {"toolCallId",    call_id},
            {"status",        status},
            {"kind",          kind},
            {"title",         tool_name},
            {"rawInput",      nlohmann::json::object()},
            {"content",       content}};
    
        if (!result.is_error) {
            update["rawOutput"] = {{"output", result.output}};
        }
        on_update(update);
    });

    // Run the loop
    const auto loop_result = loop.run(user_text);
    (void)loop_result;

    // Build usage summary
    auto msgs = loop.messages();
    std::vector<nlohmann::json> msg_jsons;
    msg_jsons.reserve(msgs.size());
    for (const auto& m : msgs) {
        msg_jsons.push_back(m.to_json());
    }

    const auto usage = compute_usage(msg_jsons);

    // Emit usage_update notification
    on_update({{"sessionUpdate", "usage_update"},
               {"sessionId",    req.session_id},
               {"usage",        usage}});

    return {
        {"stopReason", "end_turn"},
        {"sessionId",  req.session_id},
        {"usage",      usage},
    };
}

// ---------------------------------------------------------------------------
// cancel (aligned with OpenCode Agent: calls session abort)
// ---------------------------------------------------------------------------

void TurbotACPAgent::cancel(const CancelNotification& notif) {
    // Locate the SessionLoop currently running for this session and request stop.
    // SessionLoop::stop() is thread-safe (sets an atomic flag), so we only need
    // the mutex to safely read the pointer — not to hold it during the stop call.
    session::SessionLoop* loop_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lk(active_loops_mutex_);
        auto it = active_loops_.find(notif.session_id);
        if (it != active_loops_.end()) {
            loop_ptr = it->second;
        }
    }

    if (loop_ptr) {
        loop_ptr->stop();
        TURBOT_LOG_DEBUG("TurbotACPAgent::cancel: stop requested for session '{}'",
                         notif.session_id);
    } else {
        TURBOT_LOG_DEBUG("TurbotACPAgent::cancel: no active loop found for session '{}' "
                         "(may have already completed)", notif.session_id);
    }
}

// ---------------------------------------------------------------------------
// set_mode
// ---------------------------------------------------------------------------

SetSessionModeResponse TurbotACPAgent::set_mode(
    const SetSessionModeRequest& req) {
    // Verify mode exists in available agents
    auto& registry = agent::AgentRegistry::instance();
    auto agents    = registry.list_primary();
    bool found     = false;
    for (const auto& a_ptr : agents) {
        if (a_ptr && a_ptr->info().name == req.mode_id) {
            found = true;
            break;
        }
    }
    if (!found) {
        throw std::invalid_argument("Unknown mode: " + req.mode_id);
    }

    session_manager_.set_mode(req.session_id, req.mode_id);
    return {req.mode_id};
}

// ---------------------------------------------------------------------------
// set_model
// ---------------------------------------------------------------------------

nlohmann::json TurbotACPAgent::set_model(
    const SetSessionModelRequest& req) {
    std::string provider_id, model_id;
    std::optional<std::string> variant;
    parse_model_string(req.model_id, provider_id, model_id, variant);

    session_manager_.set_model(req.session_id, provider_id, model_id);
    session_manager_.set_variant(req.session_id, variant);

    nlohmann::json result = {
        {"model",    model_id},
        {"provider", provider_id},
    };
    if (variant) result["variant"] = *variant;
    return result;
}

// ---------------------------------------------------------------------------
// authenticate (always throws – aligned with OpenCode behavior)
// ---------------------------------------------------------------------------

void TurbotACPAgent::authenticate(const nlohmann::json& /* req */) {
    throw std::runtime_error("authRequired");
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

nlohmann::json TurbotACPAgent::get_available_modes() const {
    auto& registry = agent::AgentRegistry::instance();
    auto agents    = registry.list_primary();

    nlohmann::json modes     = nlohmann::json::object();
    nlohmann::json available = nlohmann::json::array();

    for (const auto& a_ptr : agents) {
        if (!a_ptr) continue;
        const auto& info = a_ptr->info();
        if (info.hidden || info.disable) continue;
        nlohmann::json mode = {{"id", info.name}};
        if (info.description) mode["description"] = *info.description;
        available.push_back(mode);
    }

    modes["availableModes"] = available;
    if (!available.empty()) {
        modes["currentModeId"] = available[0]["id"];
    }
    return modes;
}

nlohmann::json TurbotACPAgent::get_available_models(
    const std::string& /* cwd */) const {
    nlohmann::json models = nlohmann::json::object();
    models["availableModels"] = nlohmann::json::array();
    models["currentModelId"]  = "";
    return models;
}

void TurbotACPAgent::process_message(
    const nlohmann::json& msg_json,
    const std::function<void(const nlohmann::json&)>& on_update) {

    const std::string role = msg_json.value("role", "");
    const auto& parts      = msg_json.value("parts", nlohmann::json::array());

    for (const auto& part : parts) {
        const std::string type = part.value("type", "");

        if (type == "text") {
            const std::string text = part.value("text", "");
            if (role == "user") {
                on_update({{"sessionUpdate", "user_message_chunk"},
                           {"content",
                            {{"type", "text"}, {"text", text},
                             {"annotations",
                              {{"audience", {"user"}}}}}}});
            } else {
                on_update({{"sessionUpdate", "agent_message_chunk"},
                           {"content",
                            {{"type", "text"}, {"text", text}}}});
            }

        } else if (type == "reasoning") {
            on_update(
                {{"sessionUpdate", "agent_thought_chunk"},
                 {"content",
                  {{"type", "text"}, {"text", part.value("text", "")}}}});

        } else if (type == "tool") {
            const std::string call_id  = part.value("call_id", "");
            const std::string tname    = part.value("tool_name", "");
            const std::string status   = part.value("status", "pending");
            const auto& args           = part.value("args", nlohmann::json::object());
            const std::string kind     = to_tool_kind(tname);

            // tool_call (pending)
            on_update({{"sessionUpdate", "tool_call"},
                       {"toolCallId",    call_id},
                       {"title",         tname},
                       {"kind",          kind},
                       {"status",        "pending"},
                       {"rawInput",      args},
                       {"locations",     nlohmann::json::array()}});

            // tool_call_update (completed/failed/in_progress)
            std::string acp_status =
                status == "completed" ? "completed"
                : status == "error"   ? "failed"
                                      : "in_progress";

            nlohmann::json update = {
                {"sessionUpdate", "tool_call_update"},
                {"toolCallId",    call_id},
                {"status",        acp_status},
                {"kind",          kind},
                {"title",         tname},
                {"rawInput",      args},
                {"locations",     nlohmann::json::array()},
                {"content",
                 nlohmann::json::array(
                     {{{"type", "content"},
                       {"content",
                        {{"type", "text"},
                         {"text", part.value("output", "")}}}}})}};

            if (acp_status == "completed") {
                update["rawOutput"] = {{"output", part.value("output", "")}};

                // Edit tool: add diff block
                if (kind == "edit") {
                    update["content"].push_back(
                        {{"type",    "diff"},
                         {"path",    args.value("filePath", "")},
                         {"oldText", args.value("oldString", "")},
                         {"newText", args.value("newString", "")}});
                }
            }
            on_update(update);
        }
    }
}

nlohmann::json TurbotACPAgent::compute_usage(
    const std::vector<nlohmann::json>& messages) const {
    int total_in = 0, total_out = 0, total = 0;
    for (const auto& msg : messages) {
        if (!msg.contains("token_usage")) continue;
        const auto& tu = msg["token_usage"];
        total_in  += tu.value("input_tokens", 0);
        total_out += tu.value("output_tokens", 0);
    }
    total = total_in + total_out;
    return {
        {"totalTokens",  total},
        {"inputTokens",  total_in},
        {"outputTokens", total_out},
    };
}

nlohmann::json TurbotACPAgent::build_available_commands_update(
    const std::string& session_id) {
    // Build the list of available slash commands (aligned with OpenCode loadSessionMode)
    // OpenCode always includes "compact"; add it here as the baseline.
    nlohmann::json commands = nlohmann::json::array();
    commands.push_back({{"name", "compact"}, {"description", "compact the session"}});

    return {{"sessionUpdate", "available_commands_update"},
            {"sessionId",    session_id},
            {"availableCommands", commands}};
}

nlohmann::json TurbotACPAgent::session_info_from_state(
    const ACPSessionState& state,
    const session::Session& sess) {
    nlohmann::json info = {
        {"sessionId", state.id},
        {"cwd",       state.cwd},
        {"mcpServers", state.mcp_servers},
        {"title",     sess.info().title},
        {"updatedAt", std::to_string(sess.info().time_updated)},
    };
    if (state.model_id) info["modelId"]  = *state.model_id;
    if (state.mode_id)  info["mode"]     = *state.mode_id;
    return info;
}

void TurbotACPAgent::parse_model_string(
    const std::string& model_str,
    std::string& provider_id,
    std::string& model_id,
    std::optional<std::string>& variant) {

    // Format: "providerID/modelID" or "providerID/modelID/variant"
    const auto first_slash = model_str.find('/');
    if (first_slash == std::string::npos) {
        // No slash: treat whole string as model_id
        provider_id = "";
        model_id    = model_str;
        variant     = std::nullopt;
        return;
    }

    provider_id = model_str.substr(0, first_slash);
    const auto rest = model_str.substr(first_slash + 1);
    const auto second_slash = rest.find('/');
    if (second_slash == std::string::npos) {
        model_id = rest;
        variant  = std::nullopt;
    } else {
        model_id = rest.substr(0, second_slash);
        variant  = rest.substr(second_slash + 1);
    }
}

}  // namespace turbot::core::acp
