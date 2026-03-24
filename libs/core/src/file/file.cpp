#include <turbot/core/file/file.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/string_utils.hpp>
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_set>

namespace turbot::core::file {

namespace fs = std::filesystem;

// ============================================================================
// FileIgnore
// ============================================================================

namespace {

// Mirrors OpenCode file/ignore.ts FOLDERS set
const std::unordered_set<std::string>& get_ignore_folders_set() {
    static const std::unordered_set<std::string> s(
        FileIgnore::default_folders().begin(), FileIgnore::default_folders().end());
    return s;
}

// Mirrors OpenCode file/ignore.ts FILES array (basic suffix matching)
struct GlobPattern {
    std::string pattern;

    [[nodiscard]] bool matches(const std::string& filepath) const {
        // Simple glob: "**/*.ext" → check suffix; "**/dir/**" → check component
        const std::string& p = pattern;
        if (p.size() >= 3 && p.substr(0, 3) == "**/") {
            const std::string suffix = p.substr(2); // "/..."
            if (suffix.back() != '*') {
                // "**/*.pyc" — check that path ends with suffix
                if (filepath.size() >= suffix.size() &&
                    filepath.compare(filepath.size() - suffix.size(), suffix.size(), suffix) == 0) {
                    return true;
                }
                // Also check if any path component matches after removing leading "/"
                const std::string component_suffix = suffix.substr(1);
                if (filepath.find("/" + component_suffix) != std::string::npos) return true;
            } else {
                // "**/.DS_Store" with trailing glob handled above; "**/ dir /**"
                const std::string dir = p.substr(3, p.size() - 6); // strip "**/" and "/**"
                if (!dir.empty()) {
                    if (filepath.find("/" + dir + "/") != std::string::npos) return true;
                    if (filepath.find(dir + "/") == 0) return true;
                }
            }
        }
        return false;
    }
};

const std::vector<GlobPattern> kIgnoreFilePatterns = {
    {"**/*.swp"}, {"**/*.swo"},
    {"**/*.pyc"},
    {"**/.DS_Store"}, {"**/Thumbs.db"},
    {"**/logs/**"}, {"**/tmp/**"}, {"**/temp/**"},
    {"**/*.log"},
    {"**/coverage/**"}, {"**/.nyc_output/**"}
};

} // anonymous namespace

namespace FileIgnore {

bool match(
    const std::string& filepath,
    const std::vector<std::string>& extra,
    const std::vector<std::string>& whitelist
) {
    // 1. Whitelist check (simple prefix / exact match)
    for (const auto& w : whitelist) {
        if (filepath.find(w) != std::string::npos) return false;
    }

    // 2. Folder components
    const std::string norm = [&]() -> std::string {
        std::string s = filepath;
        std::replace(s.begin(), s.end(), '\\', '/');
        return s;
    }();

    std::istringstream ss(norm);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (!part.empty() && get_ignore_folders_set().count(part)) return true;
    }

    // 3. Built-in file glob patterns
    for (const auto& g : kIgnoreFilePatterns) {
        if (g.matches(norm)) return true;
    }

    // 4. Extra caller-provided patterns
    for (const auto& ep : extra) {
        GlobPattern gp{ep};
        if (gp.matches(norm)) return true;
    }

    return false;
}

const std::vector<std::string>& default_folders() {
    static const std::vector<std::string> folders = {
        "node_modules", "bower_components", ".pnpm-store", "vendor",
        ".npm", "dist", "build", "out", ".next", "target",
        "bin", "obj", ".git", ".svn", ".hg", ".vscode", ".idea",
        ".turbo", ".output", "desktop", ".sst", ".cache",
        ".webkit-cache", "__pycache__", ".pytest_cache", "mypy_cache",
        ".history", ".gradle"
    };
    return folders;
}

const std::vector<std::string>& default_file_patterns() {
    static const std::vector<std::string> patterns = {
        "**/*.swp", "**/*.swo", "**/*.pyc",
        "**/.DS_Store", "**/Thumbs.db",
        "**/logs/**", "**/tmp/**", "**/temp/**", "**/*.log",
        "**/coverage/**", "**/.nyc_output/**"
    };
    return patterns;
}

} // namespace FileIgnore

// ============================================================================
// FileService helpers
// ============================================================================

namespace {

// File extension sets (lower-case without dot)
const std::unordered_set<std::string> kImageExts = {
    "png","jpg","jpeg","gif","bmp","webp","ico","tif","tiff",
    "svg","svgz","avif","apng","jxl","heic","heif"
};
const std::unordered_set<std::string> kBinaryExts = {
    "exe","dll","pdb","bin","so","dylib","o","a","lib",
    "wav","mp3","ogg","flac","aac","wma","m4a",
    "mp4","avi","mov","wmv","flv","webm","mkv",
    "zip","tar","gz","bz2","7z","rar","xz",
    "pdf","doc","docx","ppt","pptx","xls","xlsx",
    "dmg","iso","img","sqlite","db","mdb",
    "apk","ipa","jar","war","ear","class","wasm","wat"
};
const std::unordered_set<std::string> kTextExts = {
    "ts","tsx","mts","cts","js","jsx","mjs","cjs",
    "sh","bash","zsh","fish","ps1","cmd","bat",
    "json","jsonc","yaml","yml","toml","md","mdx","txt",
    "xml","html","htm","css","scss","sass","less",
    "graphql","gql","sql","ini","cfg","conf","env",
    "cpp","hpp","h","c","cc","cxx","rs","go","py","rb","java",
    "kt","swift","cs","fs","r","m","mm","pl","lua","vim"
};
const std::unordered_map<std::string, std::string> kMimeMap = {
    {"png","image/png"},{"jpg","image/jpeg"},{"jpeg","image/jpeg"},
    {"gif","image/gif"},{"bmp","image/bmp"},{"webp","image/webp"},
    {"ico","image/x-icon"},{"svg","image/svg+xml"},{"avif","image/avif"}
};

std::string lower_ext(const std::string& path_str) {
    const auto pos = path_str.rfind('.');
    if (pos == std::string::npos) return {};
    std::string ext = path_str.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

} // anonymous namespace

// ============================================================================
// FileService
// ============================================================================

FileService::FileService(std::string working_dir)
    : working_dir_(std::move(working_dir)) {}

bool FileService::is_image_extension(const std::string& ext) { return kImageExts.count(ext) > 0; }
bool FileService::is_text_extension(const std::string& ext)  { return kTextExts.count(ext) > 0; }
bool FileService::is_binary_extension(const std::string& ext){ return kBinaryExts.count(ext) > 0; }

std::string FileService::get_mime_type(const std::string& ext) {
    const auto it = kMimeMap.find(ext);
    if (it != kMimeMap.end()) return it->second;
    return "application/octet-stream";
}

bool FileService::is_git_repo() const {
    fs::path git_dir = fs::path(working_dir_) / ".git";
    return fs::exists(git_dir);
}

/// Wrap a string in single quotes for POSIX shell, escaping embedded single quotes.
static std::string shell_quote(const std::string& s) {
    std::string r = "'";
    for (char c : s) {
        if (c == '\'') r += "'\\''";
        else r += c;
    }
    r += "'";
    return r;
}

std::string FileService::run_git(const std::vector<std::string>& args) const {
    std::string cmd = "git -C " + shell_quote(working_dir_);
    for (const auto& a : args) {
        cmd += " " + shell_quote(a);
    }
    cmd += " 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};
    std::string result;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) {
        result += buf;
    }
    const int ret = pclose(pipe);
    if (ret != 0) {
        TURBOT_LOG_DEBUG("run_git: command exited with code {}", ret);
    }
    return result;
}

// ─── list() ──────────────────────────────────────────────────────────────────

std::vector<FileNode> FileService::list(const std::string& dir) const {
    const fs::path resolved = dir.empty()
        ? fs::path(working_dir_)
        : (fs::path(working_dir_) / dir);

    // Build gitignore predicate
    bool use_ignore = is_git_repo();
    std::set<std::string> ignored_paths;
    if (use_ignore) {
        // Use `git ls-files --ignored` to build ignore list
        const std::string out = run_git({"-c", "core.fsmonitor=false", "ls-files",
                                          "--others", "--ignored",
                                          "--exclude-standard", "--directory"});
        if (!out.empty()) {
            std::istringstream ss(out);
            std::string line;
            while (std::getline(ss, line)) {
                if (!line.empty() && line.back() == '\n') line.pop_back();
                if (!line.empty()) ignored_paths.insert(line);
            }
        }
    }

    static const std::set<std::string> kExclude = {".git", ".DS_Store"};

    std::vector<FileNode> nodes;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(resolved, ec)) {
        if (ec) break;
        const std::string bname = entry.path().filename().string();
        if (kExclude.count(bname)) continue;

        const std::string abs = entry.path().string();
        const std::string rel = fs::relative(entry.path(), working_dir_, ec).string();
        const std::string type = entry.is_directory(ec) ? "directory" : "file";
        const std::string rel_check = (type == "directory") ? (rel + "/") : rel;
        const bool is_ignored = ignored_paths.count(rel_check) > 0;

        nodes.push_back(FileNode{
            .name     = bname,
            .path     = rel,
            .absolute = abs,
            .type     = type,
            .ignored  = is_ignored
        });
    }

    // Sort: directories first, then alphabetically
    std::sort(nodes.begin(), nodes.end(), [](const FileNode& a, const FileNode& b) {
        if (a.type != b.type) return a.type == "directory";
        return a.name < b.name;
    });

    return nodes;
}

// ─── read() ──────────────────────────────────────────────────────────────────

FileContent FileService::read(const std::string& relative_path) const {
    const fs::path full = fs::path(working_dir_) / relative_path;

    // Path traversal guard
    {
        std::error_code ec;
        const auto abs  = fs::weakly_canonical(full, ec);
        const auto base = fs::weakly_canonical(working_dir_, ec);
        if (!ec) {
            // Use fs::relative: if abs is outside base, relative path will start with ".."
            const auto rel_path = fs::relative(abs, base, ec);
            if (!ec && !rel_path.empty() && rel_path.string().rfind("..", 0) == 0) {
                return FileContent{.type = "binary", .content = "", .mime_type = "access-denied"};
            }
        }
    }

    const std::string ext = lower_ext(relative_path);

    // Image files — return base64
    if (is_image_extension(ext)) {
        std::ifstream f(full, std::ios::binary);
        if (!f) return FileContent{.type = "text", .content = ""};
        const std::string raw((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());
        // Simple base64 encode
        static const char* kB64 =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string encoded;
        encoded.reserve(((raw.size() + 2) / 3) * 4);
        for (size_t i = 0; i < raw.size(); i += 3) {
            uint32_t grp = static_cast<uint8_t>(raw[i]) << 16;
            if (i + 1 < raw.size()) grp |= static_cast<uint8_t>(raw[i+1]) << 8;
            if (i + 2 < raw.size()) grp |= static_cast<uint8_t>(raw[i+2]);
            encoded += kB64[(grp >> 18) & 0x3F];
            encoded += kB64[(grp >> 12) & 0x3F];
            encoded += (i + 1 < raw.size()) ? kB64[(grp >> 6) & 0x3F] : '=';
            encoded += (i + 2 < raw.size()) ? kB64[grp & 0x3F] : '=';
        }
        return FileContent{
            .type      = "text",
            .content   = std::move(encoded),
            .mime_type = get_mime_type(ext),
            .encoding  = "base64"
        };
    }

    // Known binary — return empty
    if (is_binary_extension(ext) && !is_text_extension(ext)) {
        return FileContent{.type = "binary", .content = ""};
    }

    // Text files
    std::ifstream f(full);
    if (!f) return FileContent{.type = "text", .content = ""};
    const std::string content((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());

    // Get git diff for changed files
    std::string diff_text;
    if (is_git_repo()) {
        diff_text = run_git({"-c", "core.fsmonitor=false", "diff", "--", relative_path});
        if (diff_text.empty()) {
            diff_text = run_git({"-c", "core.fsmonitor=false", "diff", "--staged",
                                 "--", relative_path});
        }
    }

    return FileContent{
        .type    = "text",
        .content = content,
        .diff    = diff_text
    };
}

// ─── status() ────────────────────────────────────────────────────────────────

std::vector<FileInfo> FileService::status() const {
    if (!is_git_repo()) return {};

    std::vector<FileInfo> result;

    // Modified/deleted files: git diff --numstat HEAD
    {
        const std::string out = run_git({"-c", "core.fsmonitor=false",
                                          "-c", "core.quotepath=false",
                                          "diff", "--numstat", "HEAD"});
        std::istringstream ss(out);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.empty()) continue;
            // Format: <added>\t<removed>\t<file>
            std::string added_s, removed_s, file;
            std::istringstream ls(line);
            ls >> added_s >> removed_s;
            std::getline(ls, file);
            if (!file.empty() && file.front() == '\t') file = file.substr(1);
            result.push_back(FileInfo{
                .path    = file,
                .added   = (added_s   == "-") ? 0 : std::stoi(added_s),
                .removed = (removed_s == "-") ? 0 : std::stoi(removed_s),
                .status  = "modified"
            });
        }
    }

    // Untracked (added) files: git ls-files --others --exclude-standard
    {
        const std::string out = run_git({"-c", "core.fsmonitor=false",
                                          "-c", "core.quotepath=false",
                                          "ls-files", "--others",
                                          "--exclude-standard"});
        std::istringstream ss(out);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.empty()) continue;
            // Count lines in the file for "added" count
            const fs::path fp = fs::path(working_dir_) / line;
            int line_count = 0;
            std::ifstream f(fp);
            if (f) {
                std::string dummy;
                while (std::getline(f, dummy)) ++line_count;
            }
            result.push_back(FileInfo{
                .path   = line,
                .added  = line_count,
                .status = "added"
            });
        }
    }

    // Deleted files: git diff --name-only --diff-filter=D HEAD
    {
        const std::string out = run_git({"-c", "core.fsmonitor=false",
                                          "-c", "core.quotepath=false",
                                          "diff", "--name-only",
                                          "--diff-filter=D", "HEAD"});
        std::istringstream ss(out);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.empty()) continue;
            result.push_back(FileInfo{.path = line, .status = "deleted"});
        }
    }

    return result;
}

// ─── scan_impl() ─────────────────────────────────────────────────────────────

void FileService::scan_impl() const {
    cached_files_.clear();
    cached_dirs_.clear();

    // Try ripgrep first
    const std::string rg_cmd =
        "rg --files --hidden --glob='!.git/*' " + shell_quote(working_dir_) + " 2>/dev/null";
    FILE* pipe = popen(rg_cmd.c_str(), "r");
    bool used_rg = false;
    if (pipe) {
        char buf[4096];
        std::set<std::string> dirs_seen;
        while (fgets(buf, sizeof(buf), pipe)) {
            std::string line(buf);
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();
            if (line.empty()) continue;

            // Make relative
            std::error_code ec;
            const std::string rel =
                fs::relative(line, working_dir_, ec).string();
            if (ec || rel.empty()) continue;

            cached_files_.push_back(rel);
            // Accumulate parent directories
            fs::path p(rel);
            while (p.has_parent_path()) {
                p = p.parent_path();
                const std::string d = p.string() + "/";
                if (!dirs_seen.count(d)) {
                    dirs_seen.insert(d);
                    cached_dirs_.push_back(d);
                }
            }
            used_rg = true;
        }
        pclose(pipe);
    }

    if (!used_rg) {
        // Fallback: std::filesystem scan
        std::error_code ec;
        for (const auto& entry :
             fs::recursive_directory_iterator(working_dir_,
                                              fs::directory_options::skip_permission_denied,
                                              ec)) {
            if (ec) break;
            std::error_code ec2;
            const std::string rel = fs::relative(entry.path(), working_dir_, ec2).string();
            if (ec2) continue;

            if (FileIgnore::match(rel)) continue;

            if (entry.is_directory(ec2)) {
                cached_dirs_.push_back(rel + "/");
            } else if (entry.is_regular_file(ec2)) {
                cached_files_.push_back(rel);
            }
        }
    }

    std::sort(cached_files_.begin(), cached_files_.end());
    std::sort(cached_dirs_.begin(), cached_dirs_.end());
    cache_valid_ = true;
}

void FileService::scan() {
    scan_impl();
}

// ─── search() ────────────────────────────────────────────────────────────────

std::vector<std::string> FileService::search(const SearchInput& input) {
    if (!cache_valid_) scan_impl();

    const auto& kind = input.type;
    const bool want_files = kind.empty() || kind == "file";
    const bool want_dirs  = kind.empty() || kind == "directory" || input.dirs;
    const int limit = input.limit > 0 ? input.limit : 100;

    std::vector<std::string> pool;
    if (want_files) pool.insert(pool.end(), cached_files_.begin(), cached_files_.end());
    if (want_dirs)  pool.insert(pool.end(), cached_dirs_.begin(),  cached_dirs_.end());

    if (input.query.empty()) {
        if (static_cast<int>(pool.size()) > limit)
            pool.resize(static_cast<size_t>(limit));
        return pool;
    }

    // Simple substring fuzzy: score = number of query chars found in order
    const std::string& q = input.query;
    std::vector<std::pair<int, std::string>> scored;
    scored.reserve(pool.size());
    for (const auto& item : pool) {
        // Compute score: consecutive matching characters
        int score = 0;
        size_t qi = 0;
        for (size_t i = 0; i < item.size() && qi < q.size(); ++i) {
            if (::tolower(item[i]) == ::tolower(q[qi])) {
                ++score;
                ++qi;
            }
        }
        if (qi == q.size()) {  // All query chars found (in order)
            scored.emplace_back(score, item);
        }
    }

    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    std::vector<std::string> result;
    result.reserve(std::min(static_cast<int>(scored.size()), limit));
    for (int i = 0; i < static_cast<int>(scored.size()) && i < limit; ++i) {
        result.push_back(scored[i].second);
    }
    return result;
}

} // namespace turbot::core::file
