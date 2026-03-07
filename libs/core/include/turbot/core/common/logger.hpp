#pragma once

#include <spdlog/spdlog.h>
#include <memory>
#include <turbot/core/common/export.hpp>

namespace turbot::core {

class TURBOT_CORE_API Logger {
public:
    static Logger& instance();

    void set_level(spdlog::level::level_enum level);
    std::shared_ptr<spdlog::logger> get_logger() const;

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

// Convenience macros
#define TURBOT_LOG_TRACE(...)    turbot::core::Logger::instance().get_logger()->trace(__VA_ARGS__)
#define TURBOT_LOG_DEBUG(...)    turbot::core::Logger::instance().get_logger()->debug(__VA_ARGS__)
#define TURBOT_LOG_INFO(...)     turbot::core::Logger::instance().get_logger()->info(__VA_ARGS__)
#define TURBOT_LOG_WARN(...)     turbot::core::Logger::instance().get_logger()->warn(__VA_ARGS__)
#define TURBOT_LOG_ERROR(...)    turbot::core::Logger::instance().get_logger()->error(__VA_ARGS__)
#define TURBOT_LOG_CRITICAL(...) turbot::core::Logger::instance().get_logger()->critical(__VA_ARGS__)

} // namespace turbot::core
