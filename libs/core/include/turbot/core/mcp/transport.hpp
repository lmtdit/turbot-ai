#pragma once

// Transport interface is defined in mcp.hpp (ITransport)
// This header provides shared transport utilities.

#include <turbot/core/mcp/mcp.hpp>
#include <turbot/network/http_client.hpp>
#include <unordered_map>
#include <string>

namespace turbot::core::mcp {

/// Convert unordered_map headers to HttpHeaders (vector<pair<string,string>>)
inline turbot::network::HttpHeaders to_http_headers(
    const std::unordered_map<std::string, std::string>& m
) {
    turbot::network::HttpHeaders result;
    result.reserve(m.size());
    for (const auto& [k, v] : m) {
        result.emplace_back(k, v);
    }
    return result;
}

}  // namespace turbot::core::mcp
