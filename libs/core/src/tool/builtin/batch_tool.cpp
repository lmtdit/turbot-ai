#include <turbot/core/tool/builtin/batch_tool.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/string_utils.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <future>
#include <thread>

namespace turbot::core::tool::builtin {

// ============================================================================
// BatchToolParams
// ============================================================================

BatchToolParams BatchToolParams::from_json(const nlohmann::json& j) {
    BatchToolParams params;
    
    if (!j.contains("tool_calls") || !j["tool_calls"].is_array()) {
        throw std::invalid_argument("tool_calls must be an array");
    }
    
    for (const auto& call : j["tool_calls"]) {
        BatchToolCall tool_call;
        tool_call.tool = call.at("tool").get<std::string>();
        tool_call.parameters = call.contains("parameters") ? call["parameters"] : nlohmann::json::object();
        params.tool_calls.push_back(std::move(tool_call));
    }
    
    return params;
}

nlohmann::json BatchToolParams::to_json() const {
    nlohmann::json j;
    nlohmann::json calls = nlohmann::json::array();
    for (const auto& call : tool_calls) {
        calls.push_back({
            {"tool", call.tool},
            {"parameters", call.parameters}
        });
    }
    j["tool_calls"] = calls;
    return j;
}

// ============================================================================
// BatchTool
// ============================================================================

const std::vector<std::string>& BatchTool::disallowed_tools() {
    static const std::vector<std::string> disallowed = {"batch"};
    return disallowed;
}

std::string BatchTool::description() const {
    return "Execute multiple tool calls in parallel. "
           "Maximum 25 tool calls per batch. "
           "The 'batch' tool itself cannot be called within a batch (nested batches are not allowed). "
           "Returns a summary of successful and failed executions.";
}

nlohmann::json BatchTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"tool_calls", {
                {"type", "array"},
                {"minItems", 1},
                {"description", "Array of tool calls to execute in parallel"},
                {"items", {
                    {"type", "object"},
                    {"properties", {
                        {"tool", {
                            {"type", "string"},
                            {"description", "The name of the tool to execute"}
                        }},
                        {"parameters", {
                            {"type", "object"},
                            {"description", "Parameters for the tool"}
                        }}
                    }},
                    {"required", nlohmann::json::array({"tool", "parameters"})}
                }}
            }}
        }},
        {"required", nlohmann::json::array({"tool_calls"})}
    };
}

bool BatchTool::validate_input(const nlohmann::json& input) const {
    if (!input.contains("tool_calls") || !input["tool_calls"].is_array()) {
        return false;
    }
    
    const auto& calls = input["tool_calls"];
    if (calls.empty()) {
        return false;
    }
    
    for (const auto& call : calls) {
        if (!call.contains("tool") || !call["tool"].is_string()) {
            return false;
        }
        if (!call.contains("parameters") || !call["parameters"].is_object()) {
            return false;
        }
    }
    
    return true;
}

BatchCallResult BatchTool::execute_single_call(
    const BatchToolCall& call,
    ToolContext& ctx
) {
    BatchCallResult result;
    result.tool = call.tool;
    
    // Check if tool is disallowed
    const auto& disallowed = disallowed_tools();
    if (std::find(disallowed.begin(), disallowed.end(), call.tool) != disallowed.end()) {
        result.error = fmt::format(
            "Tool '{}' is not allowed in batch. Disallowed tools: {}",
            call.tool,
            turbot::utils::join(disallowed, ", ")
        );
        return result;
    }
    
    // Get tool from registry
    auto tool_ptr = ToolRegistry::instance().get(call.tool);
    if (!tool_ptr) {
        // Get list of available tools (excluding certain ones)
        std::vector<std::string> available;
        for (const auto& name : ToolRegistry::instance().names()) {
            if (name != "invalid" && name != "patch" && 
                std::find(disallowed.begin(), disallowed.end(), name) == disallowed.end()) {
                available.push_back(name);
            }
        }
        
        result.error = fmt::format(
            "Tool '{}' not in registry. External tools (MCP, environment) cannot be batched - call them directly. "
            "Available tools: {}",
            call.tool,
            turbot::utils::join(available, ", ")
        );
        return result;
    }
    
    // Validate input
    if (!tool_ptr->validate_input(call.parameters)) {
        result.error = fmt::format("Invalid parameters for tool '{}'", call.tool);
        return result;
    }
    
    // Execute tool
    try {
        auto tool_result = tool_ptr->execute(call.parameters, ctx);
        result.success = !tool_result.is_error;
        result.result = std::move(tool_result);
        if (!result.success && result.result) {
            result.error = result.result->output;
        }
    } catch (const std::exception& e) {
        result.error = fmt::format("Exception during execution: {}", e.what());
    } catch (...) {
        result.error = "Unknown exception during execution";
    }
    
    return result;
}

ToolResult BatchTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    if (!validate_input(input)) {
        return ToolResult::error("batch", 
            "Invalid input: 'tool_calls' must be a non-empty array of objects with 'tool' (string) and 'parameters' (object).");
    }
    
    BatchToolParams params;
    try {
        params = BatchToolParams::from_json(input);
    } catch (const std::exception& e) {
        return ToolResult::error("batch", fmt::format("Invalid parameters: {}", e.what()));
    }
    
    // Limit to MAX_BATCH_SIZE
    size_t total_calls = params.tool_calls.size();
    size_t executed_count = std::min(total_calls, MAX_BATCH_SIZE);
    size_t discarded_count = total_calls > MAX_BATCH_SIZE ? total_calls - MAX_BATCH_SIZE : 0;
    
    // Get the calls to execute
    std::vector<BatchToolCall> calls_to_execute(
        params.tool_calls.begin(),
        params.tool_calls.begin() + executed_count
    );
    
    // Execute calls in parallel using thread pool
    std::vector<BatchCallResult> results;
    results.reserve(executed_count);
    
    // Use futures for parallel execution
    std::vector<std::future<BatchCallResult>> futures;
    futures.reserve(calls_to_execute.size());
    
    for (const auto& call : calls_to_execute) {
        futures.push_back(std::async(std::launch::async, [&call, &ctx]() {
            return execute_single_call(call, ctx);
        }));
    }
    
    // Collect results
    for (auto& future : futures) {
        results.push_back(future.get());
    }
    
    // Add errors for discarded calls
    for (size_t i = executed_count; i < total_calls; i++) {
        BatchCallResult discarded_result;
        discarded_result.tool = params.tool_calls[i].tool;
        discarded_result.error = "Maximum of 25 tools allowed in batch";
        results.push_back(std::move(discarded_result));
    }
    
    // Count successes and failures
    size_t successful_calls = 0;
    size_t failed_calls = 0;
    
    for (const auto& r : results) {
        if (r.success) {
            successful_calls++;
        } else {
            failed_calls++;
        }
    }
    
    // Build output message
    std::string output_message;
    if (failed_calls > 0) {
        output_message = fmt::format(
            "Executed {}/{} tools successfully. {} failed.",
            successful_calls,
            results.size(),
            failed_calls
        );
    } else {
        output_message = fmt::format(
            "All {} tools executed successfully.\n\nKeep using the batch tool for optimal performance in your next response!",
            successful_calls
        );
    }
    
    // Build metadata
    nlohmann::json details = nlohmann::json::array();
    for (const auto& r : results) {
        details.push_back({
            {"tool", r.tool},
            {"success", r.success}
        });
    }
    
    nlohmann::json tools_list = nlohmann::json::array();
    for (const auto& call : params.tool_calls) {
        tools_list.push_back(call.tool);
    }
    
    nlohmann::json metadata = {
        {"totalCalls", results.size()},
        {"successful", successful_calls},
        {"failed", failed_calls},
        {"tools", tools_list},
        {"details", details}
    };
    
    return ToolResult::success(
        fmt::format("Batch execution ({}/{} successful)", successful_calls, results.size()),
        output_message,
        metadata
    );
}

} // namespace turbot::core::tool::builtin
