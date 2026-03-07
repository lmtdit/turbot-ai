#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/session/session.hpp>
#include <mutex>
#include <unordered_set>

namespace turbot::core::session {

/// Session state machine - manages state transitions
/// Thread-safe implementation for managing session state lifecycle
class TURBOT_CORE_API SessionStateMachine {
public:
    SessionStateMachine() = default;
    explicit SessionStateMachine(SessionState initial_state);

    // Non-copyable
    SessionStateMachine(const SessionStateMachine&) = delete;
    SessionStateMachine& operator=(const SessionStateMachine&) = delete;

    // Non-movable (due to mutex)
    SessionStateMachine(SessionStateMachine&&) = delete;
    SessionStateMachine& operator=(SessionStateMachine&&) = delete;

    /// Check if a transition from one state to another is valid
    /// @param from Current state
    /// @param to Target state
    /// @return true if transition is allowed
    [[nodiscard]] static bool can_transition(SessionState from, SessionState to);

    /// Attempt to transition to a new state
    /// @param to Target state
    /// @return true if transition succeeded
    bool transition(SessionState to);

    /// Get current state
    [[nodiscard]] SessionState current() const noexcept;

    /// Reset to initial state
    void reset();

    /// Reset to a specific state (for testing)
    void reset_to(SessionState state);

    /// Get valid transitions from current state
    [[nodiscard]] std::vector<SessionState> valid_transitions() const;

    /// Get all valid transitions from a given state
    [[nodiscard]] static std::vector<SessionState> valid_transitions_from(SessionState state);

    /// Check if currently in a terminal state (Archived)
    [[nodiscard]] bool is_terminal() const noexcept;

    /// Check if currently in an active state (Active or Busy)
    [[nodiscard]] bool is_active_state() const noexcept;

private:
    SessionState state_ = SessionState::Created;
    mutable std::mutex mutex_;

    /// Define valid state transitions
    static const std::unordered_set<SessionState>& transitions_from_created();
    static const std::unordered_set<SessionState>& transitions_from_active();
    static const std::unordered_set<SessionState>& transitions_from_busy();
    static const std::unordered_set<SessionState>& transitions_from_compacting();
    static const std::unordered_set<SessionState>& transitions_from_archived();
};

} // namespace turbot::core::session
