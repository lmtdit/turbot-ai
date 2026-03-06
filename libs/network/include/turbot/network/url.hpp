#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <turbot/network/export.hpp>

namespace turbot::network {

class TURBOT_NETWORK_API Url {
public:
    Url() = default;
    explicit Url(std::string_view url);

    [[nodiscard]] std::string_view scheme() const noexcept { return scheme_; }
    [[nodiscard]] std::string_view host() const noexcept { return host_; }
    [[nodiscard]] std::optional<int> port() const noexcept { return port_; }
    [[nodiscard]] std::string_view path() const noexcept { return path_; }
    [[nodiscard]] std::string_view query() const noexcept { return query_; }
    [[nodiscard]] std::string_view fragment() const noexcept { return fragment_; }

    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] bool is_valid() const noexcept { return valid_; }

private:
    bool valid_ = false;
    std::string scheme_;
    std::string host_;
    std::optional<int> port_;
    std::string path_;
    std::string query_;
    std::string fragment_;

    void parse(std::string_view url);
};

} // namespace turbot::network
