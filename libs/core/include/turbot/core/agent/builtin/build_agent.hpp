#pragma once

#include <turbot/core/agent/agent.hpp>

namespace turbot::core::agent {

/// Build Agent - Default agent for executing tools
/// This is the primary agent that can execute any allowed tool
class TURBOT_CORE_API BuildAgent : public Agent {
public:
    BuildAgent();

    [[nodiscard]] std::string name() const override { return info_.name; }
    [[nodiscard]] std::string description() const override;

    [[nodiscard]] ExecuteResult execute(const ExecuteParams& params) override;

private:
    /// Build default permission ruleset for build agent
    [[nodiscard]] permission::Ruleset build_default_permission() const;
};

} // namespace turbot::core::agent
