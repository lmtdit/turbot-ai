#include <turbot/core/lsp/server.hpp>
#include <turbot/core/common/logger.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace turbot::core::lsp {

// ─── nearest_root ─────────────────────────────────────────────────────────────

std::optional<std::string> nearest_root(
    const std::string& start_dir,
    const std::vector<std::string>& include_patterns,
    const std::vector<std::string>& exclude_patterns,
    const std::string& stop_dir)
{
    if (start_dir.empty()) return std::nullopt;

    const fs::path stop = stop_dir.empty() ? fs::path{} : fs::path(stop_dir);

    // Walk up from start_dir to stop_dir (or filesystem root)
    fs::path current = fs::weakly_canonical(fs::path(start_dir));

    // 1. Check exclude patterns first (walk up entire tree)
    if (!exclude_patterns.empty()) {
        fs::path dir = current;
        while (true) {
            for (const auto& pat : exclude_patterns) {
                if (fs::exists(dir / pat)) {
                    // Exclude pattern found → server does not apply
                    return std::nullopt;
                }
            }
            // Stop condition
            if (!stop.empty() && (dir == stop || dir == dir.parent_path())) break;
            if (stop.empty() && dir == dir.parent_path()) break;  // filesystem root
            dir = dir.parent_path();
        }
    }

    // 2. Walk up looking for include patterns
    fs::path dir = current;
    while (true) {
        for (const auto& pat : include_patterns) {
            if (fs::exists(dir / pat)) {
                return dir.string();
            }
        }
        // Stop condition
        if (!stop.empty() && dir == stop) break;
        if (dir == dir.parent_path()) break;  // filesystem root
        dir = dir.parent_path();
    }

    // 3. Fallback: return stop_dir (aligned with OpenCode Instance.directory fallback)
    if (!stop_dir.empty()) {
        return stop_dir;
    }

    // No stop_dir and no include found → start_dir itself
    return start_dir;
}

}  // namespace turbot::core::lsp
