#include <turbot/core/common/logger.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_loop.hpp>
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/build_agent.hpp>
#include <turbot/core/agent/builtin/plan_agent.hpp>
#include <turbot/core/agent/builtin/explore_agent.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/acp/server.hpp>
#include <turbot/core/acp/agent.hpp>
#include <turbot/core/provider/provider_manager.hpp>
#include <turbot/core/provider/impl/bailian_provider.hpp>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace turbot::cli {

/// Load providers from a turbot.json config file and register them in ProviderManager.
/// Currently supports "bailian" (Alibaba DashScope) provider type.
/// Safe to call multiple times — duplicate registrations are silently skipped by ProviderManager.
static void init_provider_from_config(const std::string& config_path) {
    std::ifstream f(config_path);
    if (!f.is_open()) {
        TURBOT_LOG_DEBUG("init_provider_from_config: config not found at '{}'", config_path);
        return;
    }

    nlohmann::json cfg;
    try {
        cfg = nlohmann::json::parse(f);
    } catch (const std::exception& e) {
        TURBOT_LOG_WARN("init_provider_from_config: failed to parse '{}': {}", config_path, e.what());
        return;
    }

    auto& pm = core::provider::ProviderManager::instance();
    for (const auto& p : cfg.value("providers", nlohmann::json::array())) {
        const auto type = p.value("type", "");
        if (type != "bailian") continue;  // Only bailian supported in CLI for now

        const auto provider_id = p.value("name", "bailian");
        if (pm.has_provider(provider_id)) {
            TURBOT_LOG_DEBUG("init_provider_from_config: provider '{}' already registered", provider_id);
            continue;
        }

        core::provider::ProviderConfig config;
        config.api_key  = p.value("api_key", "");
        config.base_url = p.value("base_url", "");

        if (config.api_key.empty()) {
            TURBOT_LOG_WARN("init_provider_from_config: provider '{}' has no api_key, skipping", provider_id);
            continue;
        }

        auto bailian = std::make_shared<core::provider::BailianProvider>(config);
        pm.register_provider(bailian);
        pm.set_default_provider(provider_id);
        TURBOT_LOG_INFO("init_provider_from_config: registered provider '{}'", provider_id);
    }
}

/// Initialize default agents
void init_agents() {
    auto& registry = core::agent::AgentRegistry::instance();
    registry.clear();
    registry.register_agent(std::make_shared<core::agent::BuildAgent>());
    registry.register_agent(std::make_shared<core::agent::PlanAgent>());
    registry.register_agent(std::make_shared<core::agent::ExploreAgent>());
}

/// Run an interactive session
int run_session(const std::string& session_id) {
    // Initialize agents
    init_agents();

    // Load provider configuration from .turbot/turbot.json (relative to CWD)
    init_provider_from_config(".turbot/turbot.json");
    
    // Create or resume session
    core::session::Session session;
    bool is_new = session_id.empty();
    
    if (is_new) {
        core::session::CreateParams params;
        params.project_id = "cli";
        params.slug = "interactive";
        params.directory = std::filesystem::current_path().string();
        params.title = "Interactive CLI Session";
        
        auto created = core::session::Session::create(params);
        if (!created) {
            std::cerr << "Failed to create session\n";
            return 1;
        }
        session = std::move(*created);
        fmt::print("Created new session: {}\n", session.id());
    } else {
        auto existing = core::session::Session::get(session_id);
        if (!existing) {
            fmt::print(stderr, "Session not found: {}\n", session_id);
            return 1;
        }
        session = std::move(*existing);
        fmt::print("Resumed session: {}\n", session.id());
    }
    
    // Create session loop
    core::session::SessionLoop loop(session);
    
    // Set up the default agent
    auto agent = core::agent::AgentRegistry::instance().get("build");
    if (agent) {
        loop.set_agent(agent);
    }

    // Inject provider and model into the session loop
    {
        auto& pm = core::provider::ProviderManager::instance();
        auto prov_opt = pm.get_default_provider();
        if (prov_opt) {
            loop.set_provider(prov_opt->get());
            // Use the default_model from config, or fall back to the first available model
            auto models = (*prov_opt)->list_models();
            const std::string default_model = models.empty() ? "" : models.front().id;
            loop.set_model(default_model);
            TURBOT_LOG_INFO("run_session: using provider '{}' with model '{}'",
                            (*prov_opt)->id(), default_model);
        } else {
            TURBOT_LOG_WARN("run_session: no provider configured — LLM calls will fail");
            fmt::print(stderr, "[warn] No LLM provider configured. Set up .turbot/turbot.json with a valid API key.\n");
        }
    }
    
    // Set callbacks
    loop.set_on_message([](const core::Message& msg) {
        fmt::print("[{}] ", core::role_to_string(msg.role()));
        for (const auto& part : msg.parts()) {
            if (part.type == core::PartType::Text) {
                fmt::print("{}", part.data.value("text", ""));
            }
        }
        fmt::print("\n");
    });
    
    loop.set_on_tool_call([](const std::string& tool_name, const std::string& /*call_id*/, const nlohmann::json& /*input*/) {
        fmt::print("[tool] {} called\n", tool_name);
    });
    
    loop.set_on_tool_result([](const std::string& tool_name, const std::string& /*call_id*/, const core::tool::ToolResult& result) {
        if (result.is_error) {
            fmt::print("[tool] {} error: {}\n", tool_name, result.output);
        } else {
            fmt::print("[tool] {} completed\n", tool_name);
        }
    });
    
    loop.set_on_error([](const std::string& error, const std::string& code) {
        fmt::print(stderr, "[error] {} ({})\n", error, code);
    });
    
    // Interactive loop
    fmt::print("\nEnter messages (type 'exit' to quit, 'help' for commands):\n\n");
    
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        
        if (line == "exit" || line == "quit") {
            break;
        }
        
        if (line == "help") {
            fmt::print("Commands:\n");
            fmt::print("  exit, quit  - Exit the session\n");
            fmt::print("  help        - Show this help\n");
            fmt::print("  status      - Show session status\n");
            fmt::print("  agents      - List available agents\n");
            continue;
        }
        
        if (line == "status") {
            fmt::print("Session ID: {}\n", session.id());
            fmt::print("State: {}\n", core::session::session_state_to_string(loop.session().state()));
            fmt::print("Messages: {}\n", loop.messages().size());
            fmt::print("Tokens: {}\n", loop.token_count());
            continue;
        }
        
        if (line == "agents") {
            auto agents = core::agent::AgentRegistry::instance().list();
            fmt::print("Available agents:\n");
            for (const auto& a : agents) {
                fmt::print("  - {} ({})\n", a->name(), 
                    core::agent::agent_mode_to_string(a->info().mode));
            }
            continue;
        }
        
        // Process user message
        auto result = loop.run(line);
        fmt::print("\n[loop result: {}]\n", core::session::loop_result_to_string(result));
    }
    
    fmt::print("\nSession ended: {}\n", session.id());
    return 0;
}

/// List all sessions
int list_sessions() {
    fmt::print("Listing sessions...\n\n");

    auto sessions = core::session::Session::list("cli");
    if (sessions.empty()) {
        fmt::print("(No sessions found for project 'cli')\n");
        fmt::print("\nTo create a new session, use: turbot-cli run\n");
    } else {
        fmt::print("{:<38}  {:<30}  {}\n", "ID", "Title", "State");
        fmt::print("{}\n", std::string(80, '-'));
        for (const auto& s : sessions) {
            const auto& info = s.info();
            fmt::print("{:<38}  {:<30}  {}\n",
                info.id,
                info.title.substr(0, 29),
                core::session::session_state_to_string(info.state));
        }
        fmt::print("\n{} session(s) found.\n", sessions.size());
    }
    return 0;
}

/// Start ACP (Agent Client Protocol) server for IDE integration
/// Aligned with OpenCode `opencode acp` command implementation
int run_acp(const std::string& cwd) {
    // Initialize agents before starting ACP server
    init_agents();

    // Load provider configuration
    const std::string config_base = cwd.empty() ? std::filesystem::current_path().string() : cwd;
    init_provider_from_config((std::filesystem::path(config_base) / ".turbot" / "turbot.json").string());

    // Install SIGTERM/SIGINT handlers for graceful shutdown
    // (aligned with OpenCode: process.stdin.on("end", resolve))
    std::signal(SIGTERM, [](int) { core::acp::ACPServer::stop(); });
    std::signal(SIGINT,  [](int) { core::acp::ACPServer::stop(); });

    const std::string work_dir =
        cwd.empty() ? std::filesystem::current_path().string() : cwd;

    // Start ACP server (blocking until stdin EOF or stop())
    // Note: available_commands_update is pushed synchronously before session/new
    // and session/load results, which differs from OpenCode's async setTimeout(0)
    // pattern. ACP clients should be tolerant of notification order.
    try {
        core::acp::ACPServer::start(
            [work_dir]() {  // by-value capture to avoid dangling reference
                return std::make_unique<core::acp::TurbotACPAgent>(work_dir);
            },
            work_dir);
    } catch (const std::exception& ex) {
        fmt::print(stderr, "[acp] fatal error: {}\n", ex.what());
        return 1;
    }

    return 0;
}

} // namespace turbot::cli
