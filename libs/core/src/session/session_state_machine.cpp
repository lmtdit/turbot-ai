#include <turbot/core/session/session_state_machine.hpp>

namespace turbot::core::session {

SessionStateMachine::SessionStateMachine(SessionState initial_state)
    : state_(initial_state) {}

// State transition rules:
// Created -> Active (session starts)
// Active -> Busy (processing request)
// Active -> Compacting (compact history)
// Active -> Archived (archive session)
// Busy -> Active (request completed)
// Busy -> Archived (archive during processing)
// Compacting -> Active (compaction done)
// Compacting -> Archived (archive during compaction)
// Archived -> Active (restore session) - only if allowed

const std::unordered_set<SessionState>& SessionStateMachine::transitions_from_created() {
    static const std::unordered_set<SessionState> transitions = {
        SessionState::Active
    };
    return transitions;
}

const std::unordered_set<SessionState>& SessionStateMachine::transitions_from_active() {
    static const std::unordered_set<SessionState> transitions = {
        SessionState::Busy,
        SessionState::Compacting,
        SessionState::Archived
    };
    return transitions;
}

const std::unordered_set<SessionState>& SessionStateMachine::transitions_from_busy() {
    static const std::unordered_set<SessionState> transitions = {
        SessionState::Active,
        SessionState::Archived
    };
    return transitions;
}

const std::unordered_set<SessionState>& SessionStateMachine::transitions_from_compacting() {
    static const std::unordered_set<SessionState> transitions = {
        SessionState::Active,
        SessionState::Archived
    };
    return transitions;
}

const std::unordered_set<SessionState>& SessionStateMachine::transitions_from_archived() {
    static const std::unordered_set<SessionState> transitions = {
        SessionState::Active  // Allow restore
    };
    return transitions;
}

bool SessionStateMachine::can_transition(SessionState from, SessionState to) {
    if (from == to) {
        return false;  // No self-transitions
    }
    
    switch (from) {
        case SessionState::Created:
            return transitions_from_created().count(to) > 0;
        case SessionState::Active:
            return transitions_from_active().count(to) > 0;
        case SessionState::Busy:
            return transitions_from_busy().count(to) > 0;
        case SessionState::Compacting:
            return transitions_from_compacting().count(to) > 0;
        case SessionState::Archived:
            return transitions_from_archived().count(to) > 0;
        default:
            return false;
    }
}

bool SessionStateMachine::transition(SessionState to) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!can_transition(state_, to)) {
        return false;
    }
    state_ = to;
    return true;
}

SessionState SessionStateMachine::current() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

void SessionStateMachine::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = SessionState::Created;
}

void SessionStateMachine::reset_to(SessionState state) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = state;
}

std::vector<SessionState> SessionStateMachine::valid_transitions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return valid_transitions_from(state_);
}

std::vector<SessionState> SessionStateMachine::valid_transitions_from(SessionState state) {
    std::vector<SessionState> result;
    
    switch (state) {
        case SessionState::Created:
            for (const auto& s : transitions_from_created()) {
                result.push_back(s);
            }
            break;
        case SessionState::Active:
            for (const auto& s : transitions_from_active()) {
                result.push_back(s);
            }
            break;
        case SessionState::Busy:
            for (const auto& s : transitions_from_busy()) {
                result.push_back(s);
            }
            break;
        case SessionState::Compacting:
            for (const auto& s : transitions_from_compacting()) {
                result.push_back(s);
            }
            break;
        case SessionState::Archived:
            for (const auto& s : transitions_from_archived()) {
                result.push_back(s);
            }
            break;
        default:
            break;  // Unknown state, return empty vector
    }
    
    return result;
}

bool SessionStateMachine::is_terminal() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == SessionState::Archived;
}

bool SessionStateMachine::is_active_state() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == SessionState::Active || state_ == SessionState::Busy;
}

} // namespace turbot::core::session
