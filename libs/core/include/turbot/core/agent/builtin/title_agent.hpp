#pragma once

#include <turbot/core/agent/agent.hpp>
#include <memory>

namespace turbot::core::agent {

/// Title agent - generates brief conversation titles
///
/// This is a hidden agent that generates short titles (≤50 characters)
/// for conversation threads. It does not execute tools.
class TURBOT_CORE_API TitleAgent : public Agent {
public:
    TitleAgent();

    [[nodiscard]] std::string name() const override { return "title"; }
    
    [[nodiscard]] std::string description() const override {
        return "Generates brief conversation titles";
    }

    [[nodiscard]] ExecuteResult execute(const ExecuteParams& params) override;

private:
    /// System prompt for title generation
    static std::string get_prompt();
};

} // namespace turbot::core::agent
