#include <turbot/network/url.hpp>
#include <turbot/core/common/logger.hpp>
#include <algorithm>
#include <stdexcept>

namespace turbot::network {

Url::Url(std::string_view url) {
    parse(url);
}

void Url::parse(std::string_view url) {
    if (url.empty()) {
        valid_ = false;
        return;
    }

    // Parse scheme
    auto scheme_end = url.find("://");
    if (scheme_end == std::string_view::npos) {
        valid_ = false;
        return;
    }
    scheme_ = std::string(url.substr(0, scheme_end));
    
    // Normalize scheme to lowercase per RFC 3986
    std::transform(scheme_.begin(), scheme_.end(), scheme_.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    // Validate scheme - must start with letter and contain only alphanumeric, +, -, .
    if (scheme_.empty() || !std::isalpha(static_cast<unsigned char>(scheme_[0]))) {
        valid_ = false;
        return;
    }
    for (char c : scheme_) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '-' && c != '.') {
            valid_ = false;
            return;
        }
    }
    
    url = url.substr(scheme_end + 3);

    // Parse host and port
    auto path_start = url.find('/');
    auto host_port = url.substr(0, path_start);

    bool port_parse_error = false;

    // Handle IPv6 addresses enclosed in brackets: [::1] or [::1]:8080
    if (!host_port.empty() && host_port[0] == '[') {
        auto bracket_end = host_port.find(']');
        if (bracket_end == std::string_view::npos) {
            valid_ = false;
            return;
        }
        host_ = std::string(host_port.substr(1, bracket_end - 1));
        auto port_sep = host_port.find(':', bracket_end + 1);
        if (port_sep != std::string_view::npos) {
            try {
                int parsed_port = std::stoi(std::string(host_port.substr(port_sep + 1)));
                if (parsed_port > 0 && parsed_port <= 65535) {
                    port_ = parsed_port;
                } else {
                    TURBOT_LOG_WARN("Port out of valid range in URL: {}", parsed_port);
                    port_parse_error = true;
                }
            } catch (const std::exception& e) {
                TURBOT_LOG_WARN("Invalid port in IPv6 URL: {}",
                                std::string(host_port.substr(port_sep + 1)));
                port_parse_error = true;
            }
        }
    } else {
        auto port_start = host_port.find(':');
        if (port_start != std::string_view::npos) {
            host_ = std::string(host_port.substr(0, port_start));
            try {
                int parsed_port = std::stoi(std::string(host_port.substr(port_start + 1)));
                // Validate port range
                if (parsed_port > 0 && parsed_port <= 65535) {
                    port_ = parsed_port;
                } else {
                    TURBOT_LOG_WARN("Port out of valid range in URL: {}", parsed_port);
                    port_parse_error = true;
                }
            } catch (const std::invalid_argument& e) {
                TURBOT_LOG_WARN("Invalid port in URL: {}", std::string(host_port.substr(port_start + 1)));
                port_parse_error = true;
            } catch (const std::out_of_range& e) {
                TURBOT_LOG_WARN("Port out of range in URL: {}", std::string(host_port.substr(port_start + 1)));
                port_parse_error = true;
            }
        } else {
            host_ = std::string(host_port);
        }
    }
    
    // Set default ports only if no port was parsed and no error occurred
    if (!port_.has_value() && !port_parse_error) {
        if (scheme_ == "http") port_ = 80;
        else if (scheme_ == "https") port_ = 443;
    }

    // Parse path
    if (path_start != std::string_view::npos) {
        auto path_query = url.substr(path_start);
        auto query_start = path_query.find('?');
        auto fragment_start = path_query.find('#');

        if (query_start != std::string_view::npos) {
            // Guard against malformed URLs where '#' appears before '?'
            // (e.g. "/path#frag?not-query").  In that case the '?' is part of
            // the fragment, not a query string.
            if (fragment_start != std::string_view::npos && fragment_start < query_start) {
                // Treat as path + fragment only; no query string.
                path_ = std::string(path_query.substr(0, fragment_start));
                fragment_ = std::string(path_query.substr(fragment_start + 1));
            } else {
                path_ = std::string(path_query.substr(0, query_start));
                if (fragment_start != std::string_view::npos) {
                    // fragment_start > query_start here, so subtraction is safe.
                    query_ = std::string(path_query.substr(query_start + 1, fragment_start - query_start - 1));
                    fragment_ = std::string(path_query.substr(fragment_start + 1));
                } else {
                    query_ = std::string(path_query.substr(query_start + 1));
                }
            }
        } else if (fragment_start != std::string_view::npos) {
            path_ = std::string(path_query.substr(0, fragment_start));
            fragment_ = std::string(path_query.substr(fragment_start + 1));
        } else {
            path_ = std::string(path_query);
        }
    } else {
        path_ = "/";
    }

    // URL is valid only if we have a non-empty host AND a valid scheme
    valid_ = !host_.empty() && !scheme_.empty();
    
    // If port parsing failed, mark URL as invalid
    if (port_parse_error) {
        valid_ = false;
    }
}

std::string Url::to_string() const {
    std::string result = scheme_ + "://" + host_;
    if (port_.has_value()) {
        result += ":" + std::to_string(*port_);
    }
    result += path_;
    if (!query_.empty()) {
        result += "?" + query_;
    }
    if (!fragment_.empty()) {
        result += "#" + fragment_;
    }
    return result;
}

} // namespace turbot::network
