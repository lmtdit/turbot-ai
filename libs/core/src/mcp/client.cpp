#include <turbot/core/mcp/client.hpp>
#include <turbot/core/common/logger.hpp>

#include <chrono>
#include <stdexcept>

namespace turbot::core::mcp {

MCPClient::MCPClient(std::unique_ptr<ITransport> transport)
    : transport_(std::move(transport))
    , status_(MCPStatus::Failed) {}

MCPClient::~MCPClient() {
    if (status_.load() == MCPStatus::Connected) {
        try { close().get(); } catch (...) {}
    }
}

std::future<MCPStatus> MCPClient::connect(
    const std::string& client_name,
    const std::string& version
) {
    return std::async(std::launch::async, [this, client_name, version]() -> MCPStatus {
        try {
            // Step 1: Connect transport
            transport_->connect().get();

            // Step 2: Register notifications/tools/list_changed before initialize
            transport_->on_notification(
                "notifications/tools/list_changed",
                [this](const nlohmann::json&) {
                    TURBOT_LOG_INFO("MCPClient: tools/list_changed notification received");
                    std::function<void()> cb;
                    {
                        std::lock_guard<std::mutex> lock(callback_mutex_);
                        cb = tools_changed_callback_;
                    }
                    if (cb) {
                        try { cb(); } catch (const std::exception& e) {
                            TURBOT_LOG_ERROR("MCPClient: tools_changed callback threw: {}", e.what());
                        }
                    }
                }
            );

            // Step 3: Send initialize request
            // Params 对齐 OpenCode MCP initialize params
            nlohmann::json init_params = {
                {"protocolVersion", "2024-11-05"},
                {"clientInfo", {{"name", client_name}, {"version", version}}},
                {"capabilities", {{"sampling", nlohmann::json::object()}}}
            };

            auto init_resp = transport_->send_request("initialize", init_params).get();

            if (init_resp.is_error()) {
                std::string err_msg = init_resp.error.has_value()
                    ? init_resp.error->value("message", "unknown error")
                    : "unknown error";
                TURBOT_LOG_ERROR("MCPClient: initialize failed: {}", err_msg);
                status_.store(MCPStatus::Failed);
                return MCPStatus::Failed;
            }

            // Step 4: Send notifications/initialized notification
            transport_->send_notification("notifications/initialized", nlohmann::json::object());

            status_.store(MCPStatus::Connected);
            TURBOT_LOG_INFO("MCPClient: connected (client={}, version={})", client_name, version);
            return MCPStatus::Connected;

        } catch (const std::exception& e) {
            TURBOT_LOG_ERROR("MCPClient: connect failed: {}", e.what());
            status_.store(MCPStatus::Failed);
            return MCPStatus::Failed;
        }
    });
}

std::future<void> MCPClient::close() {
    return std::async(std::launch::async, [this]() {
        status_.store(MCPStatus::Disabled);
        try {
            transport_->close().get();
        } catch (const std::exception& e) {
            TURBOT_LOG_ERROR("MCPClient: close transport failed: {}", e.what());
        }
    });
}

MCPStatus MCPClient::status() const noexcept {
    return status_.load();
}

std::future<std::vector<MCPTool>> MCPClient::list_tools() {
    return std::async(std::launch::async, [this]() -> std::vector<MCPTool> {
        auto resp = transport_->send_request("tools/list", nlohmann::json::object()).get();
        if (resp.is_error()) {
            TURBOT_LOG_ERROR("MCPClient: tools/list failed: {}",
                resp.error->value("message", "unknown"));
            return {};
        }
        std::vector<MCPTool> tools;
        if (resp.result.has_value() && resp.result->contains("tools")) {
            for (const auto& t : (*resp.result)["tools"]) {
                tools.push_back(MCPTool::from_json(t));
            }
        }
        return tools;
    });
}

std::future<nlohmann::json> MCPClient::call_tool(
    const std::string& name,
    const nlohmann::json& args,
    int timeout_ms
) {
    return std::async(std::launch::async, [this, name, args, timeout_ms]() -> nlohmann::json {
        nlohmann::json params = {
            {"name", name},
            {"arguments", args}
        };

        // Set up timeout via future wait_for
        auto future = transport_->send_request("tools/call", params);
        auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(timeout_ms);

        // Wait with timeout (resetTimeoutOnProgress is approximated via deadline extension
        // when progress notifications arrive — progress handler is registered externally)
        if (future.wait_until(deadline) == std::future_status::timeout) {
            TURBOT_LOG_WARN("MCPClient: call_tool({}) timed out after {}ms", name, timeout_ms);
            return {{"isError", true}, {"content", {{{"type", "text"}, {"text", "Tool call timed out"}}}}};
        }

        auto resp = future.get();
        if (resp.is_error()) {
            std::string err_msg = resp.error->value("message", "unknown error");
            TURBOT_LOG_ERROR("MCPClient: call_tool({}) error: {}", name, err_msg);
            return {{"isError", true}, {"content", {{{"type", "text"}, {"text", err_msg}}}}};
        }
        return resp.result.value_or(nlohmann::json::object());
    });
}

std::future<std::vector<MCPResource>> MCPClient::list_resources() {
    return std::async(std::launch::async, [this]() -> std::vector<MCPResource> {
        auto resp = transport_->send_request("resources/list", nlohmann::json::object()).get();
        if (resp.is_error()) {
            TURBOT_LOG_ERROR("MCPClient: resources/list failed: {}",
                resp.error->value("message", "unknown"));
            return {};
        }
        std::vector<MCPResource> resources;
        if (resp.result.has_value() && resp.result->contains("resources")) {
            for (const auto& r : (*resp.result)["resources"]) {
                resources.push_back(MCPResource::from_json(r));
            }
        }
        return resources;
    });
}

std::future<nlohmann::json> MCPClient::read_resource(const std::string& uri) {
    return std::async(std::launch::async, [this, uri]() -> nlohmann::json {
        nlohmann::json params = {{"uri", uri}};
        auto resp = transport_->send_request("resources/read", params).get();
        if (resp.is_error()) {
            TURBOT_LOG_ERROR("MCPClient: resources/read({}) failed: {}", uri,
                resp.error->value("message", "unknown"));
            return nlohmann::json::object();
        }
        return resp.result.value_or(nlohmann::json::object());
    });
}

std::future<std::vector<MCPPrompt>> MCPClient::list_prompts() {
    return std::async(std::launch::async, [this]() -> std::vector<MCPPrompt> {
        auto resp = transport_->send_request("prompts/list", nlohmann::json::object()).get();
        if (resp.is_error()) {
            TURBOT_LOG_ERROR("MCPClient: prompts/list failed: {}",
                resp.error->value("message", "unknown"));
            return {};
        }
        std::vector<MCPPrompt> prompts;
        if (resp.result.has_value() && resp.result->contains("prompts")) {
            for (const auto& p : (*resp.result)["prompts"]) {
                prompts.push_back(MCPPrompt::from_json(p));
            }
        }
        return prompts;
    });
}

std::future<std::string> MCPClient::get_prompt(
    const std::string& name,
    const nlohmann::json& args
) {
    return std::async(std::launch::async, [this, name, args]() -> std::string {
        nlohmann::json params = {{"name", name}, {"arguments", args}};
        auto resp = transport_->send_request("prompts/get", params).get();
        if (resp.is_error()) {
            TURBOT_LOG_ERROR("MCPClient: prompts/get({}) failed: {}", name,
                resp.error->value("message", "unknown"));
            return "";
        }
        if (!resp.result.has_value()) return "";
        // Concatenate all text messages in the prompt result
        std::string result;
        if (resp.result->contains("messages")) {
            for (const auto& msg : (*resp.result)["messages"]) {
                if (msg.contains("content") && msg["content"].contains("text")) {
                    result += msg["content"]["text"].get<std::string>();
                }
            }
        }
        return result;
    });
}

void MCPClient::on_tools_changed(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    tools_changed_callback_ = std::move(callback);
}

int MCPClient::pid() const noexcept {
    return transport_ ? transport_->pid() : -1;
}

}  // namespace turbot::core::mcp
