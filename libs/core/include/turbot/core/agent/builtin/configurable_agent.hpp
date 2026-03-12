#pragma once

#include <turbot/core/agent/agent.hpp>

namespace turbot::core::agent {

/**
 * @brief A lightweight Agent whose behaviour is fully described by an AgentInfo.
 *
 * Many built-in agents (general, compaction, title, summary) do not implement
 * custom tool-execution logic at the C++ level; their purpose is to carry a
 * well-defined AgentInfo (name, mode, hidden, permissions, prompt, temperature…)
 * that the SessionLoop reads when it runs under that agent's identity.
 *
 * Usage:
 * @code
 *   AgentInfo info;
 *   info.name = "title";
 *   info.hidden = true;
 *   // ... other fields ...
 *   auto agent = std::make_shared<ConfigurableAgent>(std::move(info));
 *   AgentRegistry::instance().register_agent(agent);
 * @endcode
 */
class TURBOT_CORE_API ConfigurableAgent : public Agent {
public:
    explicit ConfigurableAgent(AgentInfo info) {
        info_ = std::move(info);
    }

    [[nodiscard]] std::string name()        const override { return info_.name; }
    [[nodiscard]] std::string description() const override {
        return info_.description.value_or("");
    }

    /// ConfigurableAgent has no internal execution logic.
    /// The caller (SessionLoop) is responsible for all LLM orchestration.
    [[nodiscard]] ExecuteResult execute(const ExecuteParams& params) override {
        return ExecuteResult::ok(
            "ConfigurableAgent '" + info_.name + "' executed for session " +
            params.session_id
        );
    }
};

} // namespace turbot::core::agent
