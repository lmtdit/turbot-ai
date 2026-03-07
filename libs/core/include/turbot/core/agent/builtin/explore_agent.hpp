#pragma once

#include <turbot/core/agent/agent.hpp>

namespace turbot::core::agent {

/// Explore Agent - Agent for exploring codebases
/// This agent is specialized for code exploration and analysis
class TURBOT_CORE_API ExploreAgent : public Agent {
public:
    ExploreAgent();

    [[nodiscard]] std::string name() const override { return info_.name; }
    [[nodiscard]] std::string description() const override;

    [[nodiscard]] ExecuteResult execute(const ExecuteParams& params) override;
};

} // namespace turbot::core::agent
