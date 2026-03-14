#include <turbot/core/tool/builtin/webfetch_tool.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/network/http_client.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <sstream>
#include <regex>
#include <unordered_set>

namespace turbot::core::tool::builtin {

namespace {

/// Maximum response size to process (1 MB)
constexpr std::size_t MAX_RESPONSE_BYTES = 1 * 1024 * 1024;

/// Truncate a string to a max byte count, appending a notice if truncated
std::string maybe_truncate(const std::string& s, std::size_t limit) {
    if (s.size() <= limit) return s;
    return s.substr(0, limit) + "\n\n[Response truncated: " +
           std::to_string(s.size()) + " bytes total, showing first " +
           std::to_string(limit) + " bytes]";
}

/// Reject SSRF-prone private/loopback URLs.
/// Returns true if the URL is safe to fetch (not a private/loopback address).
bool is_safe_url(const std::string& url) {
    // Reject private IPv4 ranges, loopback, and cloud metadata addresses
    // (SSRF protection: prevents Agent from probing internal services)
    static const std::regex kPrivate(
        "https?://(localhost|127\\.\\d+\\.\\d+\\.\\d+"
        "|10\\.\\d+\\.\\d+\\.\\d+"
        "|172\\.(1[6-9]|2\\d|3[01])\\.\\d+\\.\\d+"
        "|192\\.168\\.\\d+\\.\\d+"
        "|169\\.254\\.\\d+\\.\\d+"
        "|\\[::1\\])(:\\d+)?(/|$)",
        std::regex::icase);
    return !std::regex_search(url, kPrivate);
}

/// Strip HTML tags and convert basic structure to Markdown-ish plain text.
/// This is a lightweight HTML-to-text converter — it does NOT use a full parser.
std::string html_to_markdown(const std::string& html) {
    std::string out;
    out.reserve(html.size());

    bool in_tag  = false;
    bool in_script_or_style = false;
    std::string tag_buf;  // accumulates tag name characters

    for (std::size_t i = 0; i < html.size(); ++i) {
        const char c = html[i];

        if (in_tag) {
            if (c == '>') {
                in_tag = false;
                // Check for block-level elements → insert newline
                std::string tag_lower = tag_buf;
                std::transform(tag_lower.begin(), tag_lower.end(),
                               tag_lower.begin(), ::tolower);

                // Detect open vs close tags
                const bool is_close_tag = !tag_lower.empty() && tag_lower[0] == '/';
                // Remove leading slash to get bare tag name
                const std::string bare_tag = is_close_tag ? tag_lower.substr(1) : tag_lower;

                // Track script/style regions (suppress content inside)
                if (!is_close_tag &&
                    (bare_tag == "script" || bare_tag == "style")) {
                    in_script_or_style = true;
                }
                if (is_close_tag &&
                    (bare_tag == "script" || bare_tag == "style")) {
                    in_script_or_style = false;
                }

                static const std::unordered_set<std::string> kBlockTags = {
                    "p", "div", "br", "h1", "h2", "h3", "h4", "h5", "h6",
                    "li", "tr", "td", "th", "blockquote", "pre", "section",
                    "article", "header", "footer", "nav", "main", "ul", "ol"
                };
                if (kBlockTags.count(bare_tag) > 0) {
                    out += '\n';
                }
                tag_buf.clear();
            } else {
                // Only collect the tag name (stop at space/newline)
                if (tag_buf.empty() || (!std::isspace(static_cast<unsigned char>(c)) &&
                                        c != '=' && c != '"' && c != '\'')) {
                    if (std::isspace(static_cast<unsigned char>(c))) {
                        // tag name ends at first space; keep collecting into tag_buf for close-tag detection
                    } else {
                        tag_buf += c;
                    }
                }
            }
        } else if (c == '<') {
            in_tag   = true;
            tag_buf.clear();
        } else {
            if (!in_script_or_style) {
                out += c;
            }
        }
    }

    // Collapse runs of whitespace / blank lines
    // Replace sequences of 3+ newlines with 2 newlines
    std::regex multi_newline(R"(\n{3,})");
    out = std::regex_replace(out, multi_newline, "\n\n");

    // Decode common HTML entities
    struct { const char* entity; const char* replacement; } entities[] = {
        {"&amp;",  "&"}, {"&lt;",   "<"}, {"&gt;",   ">"},
        {"&quot;", "\""}, {"&#39;", "'"}, {"&nbsp;", " "},
        {"&apos;", "'"}, {"&mdash;", "—"}, {"&ndash;", "–"},
    };
    for (const auto& e : entities) {
        std::string s(e.entity);
        std::string r(e.replacement);
        std::size_t pos = 0;
        while ((pos = out.find(s, pos)) != std::string::npos) {
            out.replace(pos, s.size(), r);
            pos += r.size();
        }
    }

    return out;
}

/// Determine if a content type is HTML
bool is_html_content_type(const std::string& ct) {
    std::string lower_ct = ct;
    std::transform(lower_ct.begin(), lower_ct.end(), lower_ct.begin(), ::tolower);
    return lower_ct.find("text/html") != std::string::npos;
}

} // anonymous namespace

// ============================================================================
// WebFetchTool
// ============================================================================

std::string WebFetchTool::description() const {
    return "Fetch the content of a web URL and return it as text (HTML pages are converted to "
           "Markdown-like plain text). Supports HTTP and HTTPS. "
           "Useful for reading documentation, retrieving remote configuration, or accessing APIs. "
           "Response is truncated at 1 MB to prevent overly large outputs.";
}

nlohmann::json WebFetchTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"url", {
                {"type", "string"},
                {"description", "The HTTP or HTTPS URL to fetch"}
            }},
            {"headers", {
                {"type", "object"},
                {"description", "Optional HTTP request headers (key-value pairs)"},
                {"additionalProperties", {{"type", "string"}}}
            }}
        }},
        {"required", nlohmann::json::array({"url"})}
    };
}

bool WebFetchTool::validate_input(const nlohmann::json& input) const {
    if (!input.contains("url") || !input["url"].is_string()) return false;
    const std::string url = input["url"].get<std::string>();
    // Basic URL scheme check + SSRF protection
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) return false;
    return is_safe_url(url);
}

ToolResult WebFetchTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    if (!validate_input(input)) {
        return ToolResult::error(
            "WebFetch failed",
            "Invalid input: 'url' must be a string starting with http:// or https://"
        );
    }

    const std::string url = input["url"].get<std::string>();

    // Build request headers
    turbot::network::HttpHeaders headers;
    headers.emplace_back("User-Agent", "Turbot/1.0 (webfetch tool)");
    headers.emplace_back("Accept", "text/html,application/xhtml+xml,text/plain,*/*;q=0.8");

    if (input.contains("headers") && input["headers"].is_object()) {
        // Blocked headers that should not be overridable by external input
        static const std::unordered_set<std::string> kBlockedHeaders = {
            "host", "content-length", "transfer-encoding", "authorization"
        };
        for (const auto& [k, v] : input["headers"].items()) {
            if (v.is_string()) {
                // Normalise to lowercase for comparison
                std::string lk = k;
                std::transform(lk.begin(), lk.end(), lk.begin(), ::tolower);
                if (kBlockedHeaders.count(lk) > 0) {
                    TURBOT_LOG_WARN("webfetch: ignoring blocked header '{}'", k);
                    continue;
                }
                try {
                    headers.emplace_back(k, v.get<std::string>());
                } catch (const std::exception& e) {
                    TURBOT_LOG_WARN("webfetch: skipping invalid header '{}': {}", k, e.what());
                }
            }
        }
    }

    if (ctx.should_abort()) {
        return ToolResult::error("WebFetch aborted", "Aborted before request");
    }

    TURBOT_LOG_DEBUG("webfetch: GET {}", url);

    turbot::network::HttpClient client;
    client.set_timeout(30);  // 30-second timeout

    turbot::network::HttpResponse response;
    try {
        response = client.get(url, headers);
    } catch (const std::exception& e) {
        return ToolResult::error(
            "WebFetch failed",
            fmt::format("Request error for '{}': {}", url, e.what())
        );
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        return ToolResult::error(
            "WebFetch failed",
            fmt::format("HTTP {} for URL '{}'", response.status_code, url)
        );
    }

    // Convert HTML to plain text if applicable
    std::string body = response.body;
    const auto ct_header = response.get_header("Content-Type");
    const std::string content_type = ct_header ? *ct_header : "";

    if (is_html_content_type(content_type)) {
        body = html_to_markdown(body);
    }

    body = maybe_truncate(body, MAX_RESPONSE_BYTES);

    return ToolResult::success(
        fmt::format("WebFetch: {} bytes from {}", body.size(), url),
        body,
        {{"url", url}, {"status", response.status_code}}
    );
}

} // namespace turbot::core::tool::builtin
