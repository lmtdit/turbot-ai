#include <turbot/core/tool/builtin/websearch_tool.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/network/http_client.hpp>
#include <fmt/format.h>
#include <chrono>

namespace turbot::core::tool::builtin {

namespace {

/// Exa MCP API configuration
constexpr const char* EXA_BASE_URL = "https://mcp.exa.ai";
constexpr const char* EXA_SEARCH_ENDPOINT = "/mcp";
constexpr int DEFAULT_NUM_RESULTS = 8;
constexpr int REQUEST_TIMEOUT_SECONDS = 25;

/// Get current year for description templating
int get_current_year() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time_t);
    return tm.tm_year + 1900;
}

} // anonymous namespace

// ============================================================================
// WebSearchTool Implementation
// ============================================================================

std::string WebSearchTool::description() const {
    const int year = get_current_year();
    return fmt::format(
        "Search the web for current information. Returns relevant search results with titles, "
        "URLs, and content snippets. Useful for finding up-to-date information, researching "
        "topics, or answering questions about current events as of {}. "
        "Use this tool when you need to find information that may not be in your training data "
        "or when you need to verify current facts.",
        year
    );
}

nlohmann::json WebSearchTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"query", {
                {"type", "string"},
                {"description", "The search query string"}
            }},
            {"numResults", {
                {"type", "integer"},
                {"description", "Number of search results to return (default: 8)"},
                {"default", DEFAULT_NUM_RESULTS}
            }},
            {"livecrawl", {
                {"type", "string"},
                {"enum", nlohmann::json::array({"fallback", "preferred"})},
                {"description", "Live crawl mode - 'fallback': use live crawling as backup if cached content unavailable, 'preferred': prioritize live crawling (default: 'fallback')"},
                {"default", "fallback"}
            }},
            {"type", {
                {"type", "string"},
                {"enum", nlohmann::json::array({"auto", "fast", "deep"})},
                {"description", "Search type - 'auto': balanced search (default), 'fast': quick results, 'deep': comprehensive search"},
                {"default", "auto"}
            }},
            {"contextMaxCharacters", {
                {"type", "integer"},
                {"description", "Maximum characters for context string optimized for LLMs (default: 10000)"}
            }}
        }},
        {"required", nlohmann::json::array({"query"})}
    };
}

bool WebSearchTool::validate_input(const nlohmann::json& input) const {
    if (!input.contains("query") || !input["query"].is_string()) {
        return false;
    }
    
    const std::string query = input["query"].get<std::string>();
    if (query.empty() || query.size() > 1000) {
        return false;  // Reject empty or very long queries
    }
    
    // Validate optional parameters
    if (input.contains("numResults") && !input["numResults"].is_number_integer()) {
        return false;
    }
    if (input.contains("livecrawl") && !input["livecrawl"].is_string()) {
        return false;
    }
    if (input.contains("type") && !input["type"].is_string()) {
        return false;
    }
    if (input.contains("contextMaxCharacters") && !input["contextMaxCharacters"].is_number_integer()) {
        return false;
    }
    
    return true;
}

nlohmann::json WebSearchTool::build_mcp_request(const nlohmann::json& params) const {
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "tools/call"},
        {"params", {
            {"name", "web_search_exa"},
            {"arguments", {
                {"query", params["query"].get<std::string>()},
                {"type", params.value("type", "auto")},
                {"numResults", params.value("numResults", DEFAULT_NUM_RESULTS)},
                {"livecrawl", params.value("livecrawl", "fallback")}
            }}
        }}
    };
    
    // Add optional contextMaxCharacters if provided
    if (params.contains("contextMaxCharacters") && !params["contextMaxCharacters"].is_null()) {
        request["params"]["arguments"]["contextMaxCharacters"] = params["contextMaxCharacters"];
    }
    
    return request;
}

std::string WebSearchTool::parse_sse_response(const std::string& response) const {
    // Parse SSE response format: look for "data: " lines
    std::istringstream stream(response);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Remove trailing \r if present (Windows line endings)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (line.rfind("data: ", 0) == 0) {
            std::string json_str = line.substr(6);  // Skip "data: "
            
            try {
                auto data = nlohmann::json::parse(json_str);
                
                // Check for result content
                if (data.contains("result") && 
                    data["result"].contains("content") &&
                    data["result"]["content"].is_array() &&
                    !data["result"]["content"].empty()) {
                    
                    const auto& content = data["result"]["content"][0];
                    if (content.contains("text") && content["text"].is_string()) {
                        return content["text"].get<std::string>();
                    }
                }
            } catch (const nlohmann::json::parse_error& e) {
                TURBOT_LOG_DEBUG("websearch: Failed to parse SSE data line: {}", e.what());
                continue;
            }
        }
    }
    
    return "";
}

ToolResult WebSearchTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    if (!validate_input(input)) {
        return ToolResult::error(
            "websearch",
            "Invalid input: 'query' must be a non-empty string (max 1000 chars)"
        );
    }
    
    const std::string query = input["query"].get<std::string>();
    
    // Request permission for web search
    if (ctx.ask_permission) {
        permission::PermissionRequest perm_request;
        perm_request.permission = "websearch";
        perm_request.patterns = {query};
        perm_request.tool = "websearch";
        
        auto reply = ctx.ask_permission(perm_request);
        if (reply.type == permission::PermissionReply::Type::Reject) {
            return ToolResult::error("websearch", 
                fmt::format("Web search for '{}' rejected by user", query));
        }
    }
    
    // Check for abort before making request
    if (ctx.should_abort()) {
        return ToolResult::error("websearch", "Search aborted before request");
    }
    
    // Build MCP request
    nlohmann::json mcp_request = build_mcp_request(input);
    
    // Build HTTP headers
    turbot::network::HttpHeaders headers;
    headers.emplace_back("Accept", "application/json, text/event-stream");
    headers.emplace_back("Content-Type", "application/json");
    
    // Build request URL
    std::string url = fmt::format("{}{}", EXA_BASE_URL, EXA_SEARCH_ENDPOINT);
    
    TURBOT_LOG_DEBUG("websearch: POST {} with query: {}", url, query);
    
    // Execute HTTP request
    turbot::network::HttpClient client;
    client.set_timeout(REQUEST_TIMEOUT_SECONDS);
    
    turbot::network::HttpResponse response;
    try {
        response = client.post(url, mcp_request.dump(), headers);
    } catch (const std::exception& e) {
        return ToolResult::error("websearch",
            fmt::format("Search request failed: {}", e.what()));
    }
    
    // Check for abort after request
    if (ctx.should_abort()) {
        return ToolResult::error("websearch", "Search aborted after request");
    }
    
    // Handle HTTP errors
    if (response.status_code < 200 || response.status_code >= 300) {
        return ToolResult::error("websearch",
            fmt::format("Search API error (HTTP {}): {}", 
                response.status_code, 
                response.body.substr(0, 200)));
    }
    
    // Parse SSE response
    std::string search_result = parse_sse_response(response.body);
    
    if (search_result.empty()) {
        search_result = "No search results found. Please try a different query.";
    }
    
    // Build metadata
    nlohmann::json metadata;
    metadata["query"] = query;
    metadata["numResults"] = input.value("numResults", DEFAULT_NUM_RESULTS);
    
    return ToolResult::success(
        fmt::format("Web search: {}", query),
        search_result,
        metadata
    );
}

} // namespace turbot::core::tool::builtin
