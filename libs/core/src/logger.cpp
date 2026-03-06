#include <turbot/core/logger.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace turbot::core {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    logger_ = spdlog::stdout_color_mt("turbot");
    logger_->set_level(spdlog::level::info);
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
}

void Logger::set_level(spdlog::level::level_enum level) {
    logger_->set_level(level);
}

std::shared_ptr<spdlog::logger> Logger::get_logger() const {
    return logger_;
}

} // namespace turbot::core
