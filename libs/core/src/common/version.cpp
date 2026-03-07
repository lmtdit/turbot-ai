#include <turbot/core/common/version.hpp>

namespace turbot::core {

std::string_view get_version_string() noexcept {
    return Version::string();
}

} // namespace turbot::core
