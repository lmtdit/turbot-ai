/// config_bridge.cpp — MCP Config 集成桥接
///
/// 职责：
///   1. load_mcp_configs()   — 从 ConfigManager 读取 "mcp" 键，构造 MCPClientConfig 列表
///   2. initialize_mcp_from_config() — App 启动时调用，将配置传入 MCPManager::initialize()
///   3. register_mcp_config_watch() — 注册配置变更 callback，实现热重载
///
/// 对齐 OpenCode Instance.state 初始化模式：
///   - 并行连接所有 enabled=true 的服务器（MCPManager::initialize 已并行化）
///   - 配置变更对比新旧，增量 add/remove
///   - 事件通过 EventBus 发布（turbot::core::EventBus）

#include <turbot/core/mcp/manager.hpp>
#include <turbot/core/mcp/mcp.hpp>
#include <turbot/core/config/config_manager.hpp>
#include <turbot/core/event/event_bus.hpp>
#include <turbot/core/common/logger.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace turbot::core::mcp {

// ─── Internal helpers ─────────────────────────────────────────────────────────

namespace {

/// Parse a single MCP config entry from JSON
/// Supports: local (command array), remote (url), remote with headers/oauth
MCPClientConfig parse_mcp_entry(const std::string& name, const nlohmann::json& entry) {
    MCPClientConfig cfg;
    cfg.name = name;
    cfg.enabled = entry.value("enabled", true);

    const std::string type = entry.value("type", std::string{});
    cfg.type = type;

    if (type == "local") {
        if (entry.contains("command") && entry["command"].is_array()) {
            for (const auto& arg : entry["command"]) {
                if (arg.is_string()) cfg.command.push_back(arg.get<std::string>());
            }
        }
        if (entry.contains("environment") && entry["environment"].is_object()) {
            for (auto& [k, v] : entry["environment"].items()) {
                if (v.is_string()) cfg.environment[k] = v.get<std::string>();
            }
        }
        if (entry.contains("timeout")) cfg.timeout_ms = entry["timeout"].get<int>();
    } else if (type == "remote") {
        if (entry.contains("url") && entry["url"].is_string()) {
            cfg.url = entry["url"].get<std::string>();
        }
        if (entry.contains("headers") && entry["headers"].is_object()) {
            std::unordered_map<std::string, std::string> hdrs;
            for (auto& [k, v] : entry["headers"].items()) {
                if (v.is_string()) hdrs[k] = v.get<std::string>();
            }
            cfg.headers = std::move(hdrs);
        }
        // oauth: true | false | {clientId, clientSecret, scope}
        if (entry.contains("oauth")) {
            cfg.oauth = entry["oauth"];
        }
        if (entry.contains("timeout")) cfg.timeout_ms = entry["timeout"].get<int>();
    }

    return cfg;
}

/// Load all MCPClientConfig from ConfigManager "mcp" key
std::vector<MCPClientConfig> load_mcp_configs_internal() {
    std::vector<MCPClientConfig> configs;

    const auto mcp_json = turbot::core::ConfigManager::instance()
        .get<nlohmann::json>("mcp");
    if (!mcp_json || !mcp_json->is_object()) {
        TURBOT_LOG_DEBUG("config_bridge: no 'mcp' key in config");
        return configs;
    }

    for (auto& [name, entry] : mcp_json->items()) {
        if (!entry.is_object()) {
            TURBOT_LOG_WARN("config_bridge: skipping '{}' — not an object", name);
            continue;
        }
        if (!entry.contains("type")) {
            TURBOT_LOG_WARN("config_bridge: skipping '{}' — missing 'type' field", name);
            continue;
        }

        try {
            configs.push_back(parse_mcp_entry(name, entry));
        } catch (const std::exception& ex) {
            TURBOT_LOG_WARN("config_bridge: failed to parse '{}': {}", name, ex.what());
        }
    }

    return configs;
}

}  // namespace

// ─── Public API ───────────────────────────────────────────────────────────────

/// Load MCP configs from ConfigManager
std::vector<MCPClientConfig> load_mcp_configs() {
    return load_mcp_configs_internal();
}

/// App startup: load configs + initialize MCPManager
/// Called once during application initialization.
void initialize_mcp_from_config() {
    TURBOT_LOG_INFO("config_bridge: initializing MCP from config");
    const auto configs = load_mcp_configs_internal();
    TURBOT_LOG_INFO("config_bridge: found {} MCP server config(s)", configs.size());
    MCPManager::instance().initialize(configs);
}

/// Register config watch callback for hot-reload.
/// Compares new vs. old configs and calls add/disconnect/remove as needed.
void register_mcp_config_watch() {
    turbot::core::ConfigManager::instance().on_config_change(
        [](turbot::core::ConfigLevel /*level*/,
           const std::string& key,
           const nlohmann::json& /*new_value*/) {

            // Only respond to "mcp" key changes
            if (key != "mcp" && key.rfind("mcp.", 0) != 0) return;

            TURBOT_LOG_INFO("config_bridge: MCP config changed (key='{}')", key);

            // Reload full MCP config
            const auto new_configs = load_mcp_configs_internal();
            const auto current_status = MCPManager::instance().status();

            // Build lookup map for new configs
            std::unordered_map<std::string, MCPClientConfig> new_map;
            for (const auto& cfg : new_configs) {
                new_map[cfg.name] = cfg;
            }

            // Removed servers: present in current but not in new config
            for (const auto& [name, _] : current_status) {
                if (new_map.find(name) == new_map.end()) {
                    TURBOT_LOG_INFO("config_bridge: removing MCP server '{}'", name);
                    MCPManager::instance().remove(name);
                }
            }

            // Added or modified servers
            for (const auto& cfg : new_configs) {
                const bool exists = current_status.count(cfg.name) > 0;

                if (!exists) {
                    // New server
                    TURBOT_LOG_INFO("config_bridge: adding MCP server '{}'", cfg.name);
                    try {
                        MCPManager::instance().add(cfg.name, cfg).get();
                    } catch (const std::exception& ex) {
                        TURBOT_LOG_WARN("config_bridge: add '{}' failed: {}", cfg.name, ex.what());
                    }
                } else if (!cfg.enabled) {
                    // Disabled
                    TURBOT_LOG_INFO("config_bridge: disconnecting disabled MCP server '{}'", cfg.name);
                    try {
                        MCPManager::instance().disconnect(cfg.name).get();
                    } catch (const std::exception& ex) {
                        TURBOT_LOG_WARN("config_bridge: disconnect '{}' failed: {}", cfg.name, ex.what());
                    }
                } else {
                    // Modified: remove + re-add
                    TURBOT_LOG_INFO("config_bridge: reloading MCP server '{}'", cfg.name);
                    MCPManager::instance().remove(cfg.name);
                    try {
                        MCPManager::instance().add(cfg.name, cfg).get();
                    } catch (const std::exception& ex) {
                        TURBOT_LOG_WARN("config_bridge: reload '{}' failed: {}", cfg.name, ex.what());
                    }
                }
            }

            // Publish notification
            turbot::core::EventBus::instance().publish<nlohmann::json>(
                "mcp.config.reloaded",
                nlohmann::json{{"key", key}, {"server_count", new_configs.size()}}
            );
        }
    );

    TURBOT_LOG_INFO("config_bridge: MCP config watch registered");
}

}  // namespace turbot::core::mcp
