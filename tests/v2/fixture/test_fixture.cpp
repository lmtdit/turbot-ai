#include "test_fixture.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>

namespace turbot::test {

// ==================== TmpDir 实现 ====================

std::filesystem::path TmpDir::generate_unique_path() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dis(0, UINT64_MAX);
    
    std::stringstream ss;
    ss << std::hex << dis(gen);
    std::string random_str = ss.str();
    
    auto temp_dir = std::filesystem::temp_directory_path();
    return temp_dir / ("turbot-test-" + random_str);
}

TmpDir::TmpDir(bool git_init, std::optional<nlohmann::json> config) 
    : path_(generate_unique_path()) {
    std::filesystem::create_directories(path_);
    
    if (git_init) {
        // 初始化 git 仓库
        std::system(("cd " + path_.string() + " && git init --quiet").c_str());
        std::system(("cd " + path_.string() + " && git config core.fsmonitor false").c_str());
        std::system(("cd " + path_.string() + " && git config user.email \"test@turbot.test\"").c_str());
        std::system(("cd " + path_.string() + " && git config user.name \"Test\"").c_str());
        std::system(("cd " + path_.string() + " && git commit --allow-empty -m \"root commit\" --quiet").c_str());
    }
    
    if (config.has_value()) {
        // 创建配置文件
        std::ofstream file(path_ / "turbot.json");
        file << config->dump(2);
    }
}

TmpDir::~TmpDir() {
    if (owns_path_ && !path_.empty()) {
        try {
            // 先停止 git fsmonitor
            std::system(("git -C " + path_.string() + " fsmonitor--daemon stop 2>/dev/null").c_str());
            std::filesystem::remove_all(path_);
        } catch (...) {
            // 忽略清理错误
        }
    }
}

TmpDir::TmpDir(TmpDir&& other) noexcept 
    : path_(std::move(other.path_)), owns_path_(other.owns_path_) {
    other.owns_path_ = false;
    other.path_.clear();
}

TmpDir& TmpDir::operator=(TmpDir&& other) noexcept {
    if (this != &other) {
        if (owns_path_ && !path_.empty()) {
            try {
                std::filesystem::remove_all(path_);
            } catch (...) {}
        }
        path_ = std::move(other.path_);
        owns_path_ = other.owns_path_;
        other.owns_path_ = false;
        other.path_.clear();
    }
    return *this;
}

std::filesystem::path TmpDir::path() const noexcept {
    return path_;
}

void TmpDir::init(std::function<void(const std::filesystem::path&)> setup) {
    if (setup) {
        setup(path_);
    }
}

std::filesystem::path TmpDir::create_subdir(const std::string& name) {
    auto subdir = path_ / name;
    std::filesystem::create_directories(subdir);
    return subdir;
}

std::filesystem::path TmpDir::create_file(const std::string& name, const std::string& content) {
    auto filepath = path_ / name;
    // 确保父目录存在
    std::filesystem::create_directories(filepath.parent_path());
    std::ofstream file(filepath);
    file << content;
    return filepath;
}

// ==================== TestContext 实现 ====================

void TestContext::ask(const nlohmann::json& request) {
    permission_requests.push_back(request);
}

void TestContext::clear_requests() {
    permission_requests.clear();
}

TestContext TestContext::create_default() {
    TestContext ctx;
    ctx.session_id = "test-session";
    ctx.message_id = "test-message";
    ctx.call_id = "test-call";
    ctx.agent = "build";
    return ctx;
}

// ==================== TestInstance 实现 ====================

TestInstance& TestInstance::instance() {
    static TestInstance inst;
    return inst;
}

void TestInstance::provide(const std::filesystem::path& directory) {
    directory_ = std::filesystem::weakly_canonical(directory);
}

std::filesystem::path TestInstance::directory() const noexcept {
    return directory_;
}

bool TestInstance::contains_path(const std::filesystem::path& path) const {
    if (directory_.empty()) {
        return false;
    }
    
    try {
        auto canonical_path = std::filesystem::weakly_canonical(path);
        auto canonical_dir = std::filesystem::weakly_canonical(directory_);
        
        // 检查 canonical_path 是否以 canonical_dir 开头
        auto it = canonical_path.begin();
        auto dir_it = canonical_dir.begin();
        
        while (dir_it != canonical_dir.end() && it != canonical_path.end()) {
            if (*dir_it != *it) {
                return false;
            }
            ++dir_it;
            ++it;
        }
        
        return dir_it == canonical_dir.end();
    } catch (...) {
        return false;
    }
}

void TestInstance::clear() {
    directory_.clear();
}

// ==================== fs_utils 实现 ====================

namespace fs_utils {

std::filesystem::path canonical_path(const std::filesystem::path& p) {
    try {
        return std::filesystem::weakly_canonical(p);
    } catch (...) {
        return p;
    }
}

bool is_subpath(const std::filesystem::path& base, const std::filesystem::path& path) {
    try {
        auto canonical_base = std::filesystem::weakly_canonical(base);
        auto canonical_path = std::filesystem::weakly_canonical(path);
        
        auto it = canonical_path.begin();
        auto base_it = canonical_base.begin();
        
        while (base_it != canonical_base.end() && it != canonical_path.end()) {
            if (*base_it != *it) {
                return false;
            }
            ++base_it;
            ++it;
        }
        
        return base_it == canonical_base.end();
    } catch (...) {
        return false;
    }
}

} // namespace fs_utils

} // namespace turbot::test
