#pragma once

#include <string_view>
#include <turbot/core/export.hpp>

namespace turbot::core {

struct Version {
    static constexpr int major = 0;
    static constexpr int minor = 1;
    static constexpr int patch = 0;

    [[nodiscard]] static constexpr std::string_view string() noexcept {
        return "0.1.0";
    }

    [[nodiscard]] static constexpr std::string_view name() noexcept {
        return "turbot-ai";
    }
};

[[nodiscard]] TURBOT_CORE_API std::string_view get_version_string() noexcept;

} // namespace turbot::core
