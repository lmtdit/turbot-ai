#include <turbot/core/tool/builtin/truncate_tool.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/core/id/id.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace turbot::core::tool::builtin {
namespace Truncate {

namespace fs = std::filesystem;

// ─── helpers ──────────────────────────────────────────────────────────────────

/// Count UTF-8 bytes for a given string (same as Buffer.byteLength in Node).
static int byte_count(const std::string& s) noexcept {
    return static_cast<int>(s.size());
}

/// Split @p text into lines (preserving empty trailing line).
static std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }
    // If text ends with '\n', the last element from getline is empty — keep it.
    return lines;
}

// ─── output() ─────────────────────────────────────────────────────────────────

TruncateResult output(
    const std::string& text,
    const TruncateOptions& options,
    const std::string& working_dir,
    bool has_task_tool
) {
    const auto lines = split_lines(text);
    const int total_bytes = byte_count(text);
    const int max_lines   = options.max_lines > 0 ? options.max_lines : TRUNCATE_MAX_LINES;
    const int max_bytes   = options.max_bytes > 0 ? options.max_bytes : TRUNCATE_MAX_BYTES;

    // Fast path: fits within limits
    if (static_cast<int>(lines.size()) <= max_lines && total_bytes <= max_bytes) {
        return TruncateResult{.content = text, .truncated = false};
    }

    // Build the preview subset
    std::vector<std::string> out;
    int bytes = 0;
    bool hit_bytes = false;

    if (options.direction == TruncateOptions::Direction::Head) {
        for (int i = 0; i < static_cast<int>(lines.size()) && i < max_lines; ++i) {
            const int sz = byte_count(lines[i]) + (i > 0 ? 1 : 0); // +1 for newline
            if (bytes + sz > max_bytes) { hit_bytes = true; break; }
            out.push_back(lines[i]);
            bytes += sz;
        }
    } else {
        // Collect in reverse order then reverse once (avoids O(n²) insert-at-begin)
        for (int i = static_cast<int>(lines.size()) - 1;
             i >= 0 && static_cast<int>(out.size()) < max_lines; --i) {
            const int sz = byte_count(lines[i]) + (out.empty() ? 0 : 1);
            if (bytes + sz > max_bytes) { hit_bytes = true; break; }
            out.push_back(lines[i]);
            bytes += sz;
        }
        std::reverse(out.begin(), out.end());
    }

    const int removed = hit_bytes
        ? (total_bytes - bytes)
        : (static_cast<int>(lines.size()) - static_cast<int>(out.size()));
    const std::string unit = hit_bytes ? "bytes" : "lines";
    const std::string preview = [&]() -> std::string {
        std::string s;
        for (size_t i = 0; i < out.size(); ++i) {
            if (i) s += '\n';
            s += out[i];
        }
        return s;
    }();

    // Persist full text to a file inside the truncation directory
    std::string trunc_dir = working_dir.empty()
        ? TRUNCATION_SUBDIR
        : (fs::path(working_dir) / TRUNCATION_SUBDIR).string();

    // Generate a time-ordered filename using the project ID generator
    const std::string file_id = turbot::core::id::ascending("tool");
    const std::string file_path = (fs::path(trunc_dir) / file_id).string();

    std::error_code ec;
    fs::create_directories(trunc_dir, ec);
    if (!ec) {
        std::ofstream f(file_path);
        if (f.is_open()) {
            f << text;
        } else {
            TURBOT_LOG_WARN("Truncate::output: could not open {} for writing", file_path);
        }
    } else {
        TURBOT_LOG_WARN("Truncate::output: could not create truncation dir {}: {}",
                        trunc_dir, ec.message());
    }

    const std::string hint = has_task_tool
        ? "The tool call succeeded but the output was truncated. Full output saved to: " + file_path +
          "\nUse the Task tool to have an explore agent process this file with Grep and Read "
          "(with offset/limit). Do NOT read the full file yourself - delegate to save context."
        : "The tool call succeeded but the output was truncated. Full output saved to: " + file_path +
          "\nUse Grep to search the full content or Read with offset/limit to view specific sections.";

    std::string content;
    if (options.direction == TruncateOptions::Direction::Head) {
        content = preview + "\n\n..." + std::to_string(removed) + " " + unit +
                  " truncated...\n\n" + hint;
    } else {
        content = "..." + std::to_string(removed) + " " + unit +
                  " truncated...\n\n" + hint + "\n\n" + preview;
    }

    return TruncateResult{
        .content = std::move(content),
        .truncated = true,
        .output_path = file_path
    };
}

// ─── cleanup() ────────────────────────────────────────────────────────────────

void cleanup(const std::string& working_dir, int retention_days) {
    std::string trunc_dir = working_dir.empty()
        ? TRUNCATION_SUBDIR
        : (fs::path(working_dir) / TRUNCATION_SUBDIR).string();

    std::error_code ec;
    if (!fs::exists(trunc_dir, ec) || ec) return;

    const auto cutoff = std::filesystem::file_time_type::clock::now() -
                        std::chrono::hours(retention_days * 24);

    for (const auto& entry : fs::directory_iterator(trunc_dir, ec)) {
        if (ec) break;
        std::error_code last_ec;
        const auto last_write = fs::last_write_time(entry.path(), last_ec);
        if (last_ec) continue;
        if (last_write >= cutoff) continue;
        fs::remove(entry.path(), last_ec);  // silent on error
    }
}

} // namespace Truncate
} // namespace turbot::core::tool::builtin
