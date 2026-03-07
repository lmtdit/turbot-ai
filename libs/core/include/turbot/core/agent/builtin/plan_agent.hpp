#pragma once

#include <turbot/core/agent/agent.hpp>

namespace turbot::core::agent {

/// Plan Agent - Agent for planning and analysis
/// This agent is specialized for planning tasks and does not execute tools
class TURBOT_CORE_API PlanAgent : public Agent {
public:
    PlanAgent();

    [[nodiscard]] std::string name() const override { return info_.name; }
    [[nodiscard]] std::string description() const override;

    [[nodiscard]] ExecuteResult execute(const ExecuteParams& params) override;
};

} // namespace turbot::core::agent
