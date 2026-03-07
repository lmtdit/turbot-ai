#pragma once

#include <spdlog/spdlog.h>
#include <memory>
#include <turbot/core/common/export.hpp>

namespace turbot::core {

class TURBOT_CORE_API Logger {
public:
    static Logger& instance() noexcept;

    void set_level(spdlog::level::level_enum level);
    [[nodiscard]] std::shared_ptr<spdlog::logger> get_logger() const noexcept;

    // Delete copy and move
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

private:
    Logger();
    ~Logger() = default;

    std::shared_ptr<spdlog::logger> logger_;
};

// Convenience macros - 使用 do { } while(0) 包装确保在 if-else 中的正确行为
#define TURBOT_LOG_TRACE(...)    do { turbot::core::Logger::instance().get_logger()->trace(__VA_ARGS__); } while(0)
#define TURBOT_LOG_DEBUG(...)    do { turbot::core::Logger::instance().get_logger()->debug(__VA_ARGS__); } while(0)
#define TURBOT_LOG_INFO(...)     do { turbot::core::Logger::instance().get_logger()->info(__VA_ARGS__); } while(0)
#define TURBOT_LOG_WARN(...)     do { turbot::core::Logger::instance().get_logger()->warn(__VA_ARGS__); } while(0)
#define TURBOT_LOG_ERROR(...)    do { turbot::core::Logger::instance().get_logger()->error(__VA_ARGS__); } while(0)
#define TURBOT_LOG_CRITICAL(...) do { turbot::core::Logger::instance().get_logger()->critical(__VA_ARGS__); } while(0)

} // namespace turbot::core
