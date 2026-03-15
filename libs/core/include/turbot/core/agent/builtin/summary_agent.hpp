#pragma once

#include <turbot/core/agent/agent.hpp>
#include <memory>

namespace turbot::core::agent {

/// Summary agent - generates conversation summaries
///
/// This is a hidden agent that generates pull-request-style summaries
/// of what was done in a conversation. It does not execute tools.
class TURBOT_CORE_API SummaryAgent : public Agent {
public:
    SummaryAgent();

    [[nodiscard]] std::string name() const override { return "summary"; }
    
    [[nodiscard]] std::string description() const override {
        return "Generates conversation summaries";
    }

    [[nodiscard]] ExecuteResult execute(const ExecuteParams& params) override;

private:
    /// System prompt for summary generation
    static std::string get_prompt();
};

} // namespace turbot::core::agent
