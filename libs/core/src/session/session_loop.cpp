#include <turbot/core/session/session_loop.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <fmt/format.h>
#include <atomic>
#include <filesystem>

namespace turbot::core::session {

std::string loop_result_to_string(LoopResult result) {
    switch (result) {
        case LoopResult::Continue: return "continue";
        case LoopResult::Stop: return "stop";
        case LoopResult::Compact: return "compact";
    }
    throw std::invalid_argument(fmt::format("Invalid LoopResult value: {}", static_cast<int>(result)));
}

SessionLoop::SessionLoop(const std::string& session_id)
    : abort_flag_(std::make_shared<std::atomic<bool>>(false)) {
    // Try to get existing session
    auto existing = Session::get(session_id);
    if (existing) {
        session_ = std::move(*existing);
    } else {
        // Create a new session
        CreateParams params;
        params.project_id = "default";
        params.slug = "main";
        params.directory = (std::filesystem::temp_directory_path() / "turbot").string();
        params.title = "Interactive Session";
        
        auto created = Session::create(params);
        if (created) {
            session_ = std::move(*created);
        }
    }
}

SessionLoop::SessionLoop(Session session)
    : session_(std::move(session))
    , abort_flag_(std::make_shared<std::atomic<bool>>(false)) {
}

void SessionLoop::set_config(const SessionLoopConfig& config) {
    config_ = config;
}

void SessionLoop::set_agent(std::shared_ptr<agent::Agent> agent) {
    agent_ = std::move(agent);
}

void SessionLoop::set_on_message(MessageCallback callback) {
    on_message_ = std::move(callback);
}

void SessionLoop::set_on_tool_call(ToolCallCallback callback) {
    on_tool_call_ = std::move(callback);
}

void SessionLoop::set_on_tool_result(ToolResultCallback callback) {
    on_tool_result_ = std::move(callback);
}

void SessionLoop::set_on_error(ErrorCallback callback) {
    on_error_ = std::move(callback);
}

LoopResult SessionLoop::run(const std::string& user_message) {
    running_.store(true, std::memory_order_relaxed);
    stop_requested_.store(false, std::memory_order_relaxed);
    abort_flag_->store(false, std::memory_order_relaxed);
    iteration_count_.store(0, std::memory_order_relaxed);
    
    // Process the user message
    LoopResult result = process_user_message(user_message);
    
    // Main loop
    while (result == LoopResult::Continue && 
           !stop_requested_.load(std::memory_order_relaxed) && 
           iteration_count_.load(std::memory_order_relaxed) < config_.max_iterations) {
        
        // Check for abort
        if (abort_flag_->load(std::memory_order_relaxed)) {
            break;
        }
        
        result = step();
        iteration_count_.fetch_add(1, std::memory_order_relaxed);
        
        // Check for compaction
        if (result == LoopResult::Compact && config_.auto_compact) {
            session_.compact();
            token_count_.store(0, std::memory_order_relaxed);  // Reset token count after compaction
            result = LoopResult::Continue;
        }
    }
    
    running_.store(false, std::memory_order_relaxed);
    return result;
}

LoopResult SessionLoop::step() {
    // This is a simplified implementation
    // In a real implementation, this would:
    // 1. Call the LLM with the current messages
    // 2. Process the response (tool calls, text, etc.)
    // 3. Execute any tool calls
    // 4. Add results to messages
    
    if (stop_requested_.load(std::memory_order_relaxed) || 
        iteration_count_.load(std::memory_order_relaxed) >= config_.max_iterations) {
        return LoopResult::Stop;
    }
    
    // Check for compaction
    if (needs_compaction()) {
        return LoopResult::Compact;
    }
    
    // Simulate agent execution
    if (agent_) {
        agent::ExecuteParams params;
        params.session_id = session_.id();
        params.prompt = "Continue processing";
        
        auto result = agent_->execute(params);
        
        if (!result.is_success) {
            if (on_error_) {
                on_error_(result.error_message.value_or("Unknown error"));
            }
            return LoopResult::Stop;
        }
        
        // Update token count
        token_count_.fetch_add(estimate_tokens(result.output), std::memory_order_relaxed);
    }
    
    return LoopResult::Continue;
}

void SessionLoop::stop() {
    stop_requested_.store(true, std::memory_order_relaxed);
    abort_flag_->store(true, std::memory_order_relaxed);
}

LoopResult SessionLoop::process_user_message(const std::string& content) {
    // Create a user message
    core::Message user_msg(session_.id(), core::Role::User, "system", "", "");
    
    // Add text part using factory method
    user_msg.add_part(core::Part::create_text(content));
    
    // Add to messages
    messages_.push_back(user_msg);
    
    // Update token count
    token_count_.fetch_add(estimate_tokens(content), std::memory_order_relaxed);
    
    // Callback
    if (on_message_) {
        on_message_(user_msg);
    }
    
    // Update session state
    session_.update(UpdateParams{.state = SessionState::Active});
    
    return LoopResult::Continue;
}

LoopResult SessionLoop::process_tool_call(const std::string& tool_name, const nlohmann::json& input) {
    // Callback for tool call
    if (on_tool_call_) {
        on_tool_call_(tool_name, input);
    }
    
    // Get the tool from registry
    auto tool = tool::ToolRegistry::instance().get(tool_name);
    if (!tool) {
        if (on_error_) {
            on_error_(fmt::format("Tool not found: {}", tool_name));
        }
        return LoopResult::Continue;  // Continue despite error
    }
    
    // Build execution context
    tool::ToolContext ctx;
    ctx.session_id = session_.id();
    ctx.message_id = fmt::format("msg_{}", iteration_count_.load(std::memory_order_relaxed));
    ctx.agent = agent_ ? agent_->name() : "unknown";
    ctx.abort_flag = abort_flag_;
    
    // Execute the tool
    auto result = tool->execute(input, ctx);
    
    // Callback for tool result
    if (on_tool_result_) {
        on_tool_result_(tool_name, result);
    }
    
    // Update token count
    token_count_.fetch_add(estimate_tokens(result.output), std::memory_order_relaxed);
    
    return LoopResult::Continue;
}

bool SessionLoop::needs_compaction() const noexcept {
    return token_count_.load(std::memory_order_relaxed) >= config_.compact_threshold;
}

int SessionLoop::estimate_tokens(const std::string& text) noexcept {
    // Simple estimation: ~4 characters per token on average
    // This is a rough approximation; real tokenization would be more accurate
    return static_cast<int>(text.size() / 4) + 1;
}

} // namespace turbot::core::session
