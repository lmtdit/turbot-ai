/**
 * @file session_state_machine_test.cpp
 * @brief Session State Machine 测试
 */

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/session/session_state_machine.hpp>
#include <thread>
#include <vector>

namespace turbot::test {

using namespace turbot::core::session;

// ============================================================================
// Session State Machine 构造测试
// ============================================================================

TEST_CASE("Session.StateMachine.Construct.Default", "[Session][StateMachine]") {
    SessionStateMachine sm;
    REQUIRE(sm.current() == SessionState::Created);
}

TEST_CASE("Session.StateMachine.Construct.WithInitialState", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Active);
    REQUIRE(sm.current() == SessionState::Active);
}

// ============================================================================
// Session State Machine 状态转换测试
// ============================================================================

TEST_CASE("Session.StateMachine.Transition.CreatedToActive", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Created);

    REQUIRE(sm.transition(SessionState::Active));
    REQUIRE(sm.current() == SessionState::Active);
}

TEST_CASE("Session.StateMachine.Transition.ActiveToBusy", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Active);

    REQUIRE(sm.transition(SessionState::Busy));
    REQUIRE(sm.current() == SessionState::Busy);
}

TEST_CASE("Session.StateMachine.Transition.ActiveToCompacting", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Active);

    REQUIRE(sm.transition(SessionState::Compacting));
    REQUIRE(sm.current() == SessionState::Compacting);
}

TEST_CASE("Session.StateMachine.Transition.ActiveToArchived", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Active);

    REQUIRE(sm.transition(SessionState::Archived));
    REQUIRE(sm.current() == SessionState::Archived);
}

TEST_CASE("Session.StateMachine.Transition.BusyToActive", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Busy);

    REQUIRE(sm.transition(SessionState::Active));
    REQUIRE(sm.current() == SessionState::Active);
}

TEST_CASE("Session.StateMachine.Transition.BusyToArchived", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Busy);

    REQUIRE(sm.transition(SessionState::Archived));
    REQUIRE(sm.current() == SessionState::Archived);
}

TEST_CASE("Session.StateMachine.Transition.CompactingToActive", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Compacting);

    REQUIRE(sm.transition(SessionState::Active));
    REQUIRE(sm.current() == SessionState::Active);
}

TEST_CASE("Session.StateMachine.Transition.CompactingToArchived", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Compacting);

    REQUIRE(sm.transition(SessionState::Archived));
    REQUIRE(sm.current() == SessionState::Archived);
}

TEST_CASE("Session.StateMachine.Transition.ArchivedToActive", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Archived);

    REQUIRE(sm.transition(SessionState::Active));
    REQUIRE(sm.current() == SessionState::Active);
}

// ============================================================================
// Session State Machine 无效转换测试
// ============================================================================

TEST_CASE("Session.StateMachine.InvalidTransition.CreatedToBusy", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Created);

    REQUIRE_FALSE(sm.transition(SessionState::Busy));
    REQUIRE(sm.current() == SessionState::Created);
}

TEST_CASE("Session.StateMachine.InvalidTransition.CreatedToCompacting", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Created);

    REQUIRE_FALSE(sm.transition(SessionState::Compacting));
    REQUIRE(sm.current() == SessionState::Created);
}

TEST_CASE("Session.StateMachine.InvalidTransition.CreatedToArchived", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Created);

    REQUIRE_FALSE(sm.transition(SessionState::Archived));
    REQUIRE(sm.current() == SessionState::Created);
}

TEST_CASE("Session.StateMachine.InvalidTransition.SelfTransition", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Active);

    REQUIRE_FALSE(sm.transition(SessionState::Active));
    REQUIRE(sm.current() == SessionState::Active);
}

TEST_CASE("Session.StateMachine.InvalidTransition.BusyToCompacting", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Busy);

    REQUIRE_FALSE(sm.transition(SessionState::Compacting));
    REQUIRE(sm.current() == SessionState::Busy);
}

TEST_CASE("Session.StateMachine.InvalidTransition.CompactingToBusy", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Compacting);

    REQUIRE_FALSE(sm.transition(SessionState::Busy));
    REQUIRE(sm.current() == SessionState::Compacting);
}

// ============================================================================
// Session State Machine 有效转换列表测试
// ============================================================================

TEST_CASE("Session.StateMachine.ValidTransitions.FromCreated", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Created);

    auto valid = sm.valid_transitions();
    REQUIRE(valid.size() == 1);
    REQUIRE(valid[0] == SessionState::Active);
}

TEST_CASE("Session.StateMachine.ValidTransitions.FromActive", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Active);

    auto valid = sm.valid_transitions();
    REQUIRE(valid.size() == 3);

    // 检查所有有效转换
    bool has_busy = false, has_compacting = false, has_archived = false;
    for (const auto& s : valid) {
        if (s == SessionState::Busy) has_busy = true;
        if (s == SessionState::Compacting) has_compacting = true;
        if (s == SessionState::Archived) has_archived = true;
    }
    REQUIRE(has_busy);
    REQUIRE(has_compacting);
    REQUIRE(has_archived);
}

TEST_CASE("Session.StateMachine.ValidTransitions.FromBusy", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Busy);

    auto valid = sm.valid_transitions();
    REQUIRE(valid.size() == 2);

    bool has_active = false, has_archived = false;
    for (const auto& s : valid) {
        if (s == SessionState::Active) has_active = true;
        if (s == SessionState::Archived) has_archived = true;
    }
    REQUIRE(has_active);
    REQUIRE(has_archived);
}

TEST_CASE("Session.StateMachine.ValidTransitions.FromCompacting", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Compacting);

    auto valid = sm.valid_transitions();
    REQUIRE(valid.size() == 2);

    bool has_active = false, has_archived = false;
    for (const auto& s : valid) {
        if (s == SessionState::Active) has_active = true;
        if (s == SessionState::Archived) has_archived = true;
    }
    REQUIRE(has_active);
    REQUIRE(has_archived);
}

TEST_CASE("Session.StateMachine.ValidTransitions.FromArchived", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Archived);

    auto valid = sm.valid_transitions();
    REQUIRE(valid.size() == 1);
    REQUIRE(valid[0] == SessionState::Active);
}

// ============================================================================
// Session State Machine 重置测试
// ============================================================================

TEST_CASE("Session.StateMachine.Reset", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Busy);

    sm.reset();
    REQUIRE(sm.current() == SessionState::Created);
}

TEST_CASE("Session.StateMachine.ResetTo", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Created);

    sm.reset_to(SessionState::Archived);
    REQUIRE(sm.current() == SessionState::Archived);
}

// ============================================================================
// Session State Machine 状态检查测试
// ============================================================================

TEST_CASE("Session.StateMachine.IsTerminal.True", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Archived);
    REQUIRE(sm.is_terminal());
}

TEST_CASE("Session.StateMachine.IsTerminal.False", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Active);
    REQUIRE_FALSE(sm.is_terminal());
}

TEST_CASE("Session.StateMachine.IsActiveState.Active", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Active);
    REQUIRE(sm.is_active_state());
}

TEST_CASE("Session.StateMachine.IsActiveState.Busy", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Busy);
    REQUIRE(sm.is_active_state());
}

TEST_CASE("Session.StateMachine.IsActiveState.False", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Created);
    REQUIRE_FALSE(sm.is_active_state());
}

// ============================================================================
// Session State Machine 线程安全测试
// ============================================================================

TEST_CASE("Session.StateMachine.ThreadSafety", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Created);

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> fail_count{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&sm, &success_count, &fail_count]() {
            for (int j = 0; j < 100; ++j) {
                // 尝试从 Created -> Active
                if (sm.transition(SessionState::Active)) {
                    success_count++;
                    // 立即回到 Created 以便其他线程可以继续
                    sm.reset();
                } else {
                    fail_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 应该有一些成功的转换
    REQUIRE(success_count > 0);
}

TEST_CASE("Session.StateMachine.ConcurrentRead", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Active);

    std::vector<std::thread> threads;
    std::atomic<int> read_count{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&sm, &read_count]() {
            for (int j = 0; j < 100; ++j) {
                auto state = sm.current();
                (void)state;
                read_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    REQUIRE(read_count == 1000);
}

// ============================================================================
// Session State Machine 完整生命周期测试
// ============================================================================

TEST_CASE("Session.StateMachine.FullLifecycle", "[Session][StateMachine]") {
    SessionStateMachine sm(SessionState::Created);

    // Created -> Active
    REQUIRE(sm.transition(SessionState::Active));
    REQUIRE(sm.current() == SessionState::Active);
    REQUIRE_FALSE(sm.is_terminal());
    REQUIRE(sm.is_active_state());

    // Active -> Busy
    REQUIRE(sm.transition(SessionState::Busy));
    REQUIRE(sm.current() == SessionState::Busy);
    REQUIRE(sm.is_active_state());

    // Busy -> Active
    REQUIRE(sm.transition(SessionState::Active));
    REQUIRE(sm.current() == SessionState::Active);

    // Active -> Compacting
    REQUIRE(sm.transition(SessionState::Compacting));
    REQUIRE(sm.current() == SessionState::Compacting);

    // Compacting -> Active
    REQUIRE(sm.transition(SessionState::Active));
    REQUIRE(sm.current() == SessionState::Active);

    // Active -> Archived
    REQUIRE(sm.transition(SessionState::Archived));
    REQUIRE(sm.current() == SessionState::Archived);
    REQUIRE(sm.is_terminal());
    REQUIRE_FALSE(sm.is_active_state());

    // Archived -> Active (restore)
    REQUIRE(sm.transition(SessionState::Active));
    REQUIRE(sm.current() == SessionState::Active);
}

// ============================================================================
// Session State Machine 静态方法测试
// ============================================================================

TEST_CASE("Session.StateMachine.CanTransition.Static", "[Session][StateMachine]") {
    // 测试静态方法
    REQUIRE(SessionStateMachine::can_transition(SessionState::Created, SessionState::Active));
    REQUIRE_FALSE(SessionStateMachine::can_transition(SessionState::Created, SessionState::Busy));

    REQUIRE(SessionStateMachine::can_transition(SessionState::Active, SessionState::Busy));
    REQUIRE(SessionStateMachine::can_transition(SessionState::Active, SessionState::Compacting));
    REQUIRE(SessionStateMachine::can_transition(SessionState::Active, SessionState::Archived));
    REQUIRE_FALSE(SessionStateMachine::can_transition(SessionState::Active, SessionState::Created));

    REQUIRE(SessionStateMachine::can_transition(SessionState::Busy, SessionState::Active));
    REQUIRE(SessionStateMachine::can_transition(SessionState::Busy, SessionState::Archived));
    REQUIRE_FALSE(SessionStateMachine::can_transition(SessionState::Busy, SessionState::Compacting));
}

TEST_CASE("Session.StateMachine.ValidTransitionsFrom.Static", "[Session][StateMachine]") {
    auto from_created = SessionStateMachine::valid_transitions_from(SessionState::Created);
    REQUIRE(from_created.size() == 1);

    auto from_active = SessionStateMachine::valid_transitions_from(SessionState::Active);
    REQUIRE(from_active.size() == 3);

    auto from_busy = SessionStateMachine::valid_transitions_from(SessionState::Busy);
    REQUIRE(from_busy.size() == 2);

    auto from_archived = SessionStateMachine::valid_transitions_from(SessionState::Archived);
    REQUIRE(from_archived.size() == 1);
}

} // namespace turbot::test
