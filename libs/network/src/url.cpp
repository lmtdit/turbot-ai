#include <turbot/network/url.hpp>
#include <algorithm>

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
    url = url.substr(scheme_end + 3);

    // Parse host and port
    auto path_start = url.find('/');
    auto host_port = url.substr(0, path_start);

    auto port_start = host_port.find(':');
    if (port_start != std::string_view::npos) {
        host_ = std::string(host_port.substr(0, port_start));
        try {
            port_ = std::stoi(std::string(host_port.substr(port_start + 1)));
        } catch (...) {
            port_ = std::nullopt;
        }
    } else {
        host_ = std::string(host_port);
        // Default ports
        if (scheme_ == "http") port_ = 80;
        else if (scheme_ == "https") port_ = 443;
    }

    // Parse path
    if (path_start != std::string_view::npos) {
        auto path_query = url.substr(path_start);
        auto query_start = path_query.find('?');
        auto fragment_start = path_query.find('#');

        if (query_start != std::string_view::npos) {
            path_ = std::string(path_query.substr(0, query_start));
            if (fragment_start != std::string_view::npos) {
                query_ = std::string(path_query.substr(query_start + 1, fragment_start - query_start - 1));
                fragment_ = std::string(path_query.substr(fragment_start + 1));
            } else {
                query_ = std::string(path_query.substr(query_start + 1));
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

    valid_ = !host_.empty();
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
