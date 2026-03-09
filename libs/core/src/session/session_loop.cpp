#include <turbot/core/session/session_loop.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/llm/llm.hpp>
#include <fmt/format.h>
#include <atomic>
#include <filesystem>
#include <sstream>

namespace turbot::core::session {

std::string loop_result_to_string(LoopResult result) {
    switch (result) {
        case LoopResult::Continue: return "continue";
        case LoopResult::Stop: return "stop";
        case LoopResult::Compact: return "compact";
        case LoopResult::Error: return "error";
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

void SessionLoop::set_provider(provider::Provider* provider) {
    provider_ = provider;
}

void SessionLoop::set_model(const std::string& model_id) {
    model_id_ = model_id;
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

void SessionLoop::set_on_stream_event(StreamEventCallback callback) {
    on_stream_event_ = std::move(callback);
}

void SessionLoop::set_on_step(StepCallback callback) {
    on_step_ = std::move(callback);
}

LoopResult SessionLoop::run(const std::string& user_message) {
    running_.store(true, std::memory_order_release);
    stop_requested_.store(false, std::memory_order_relaxed);
    abort_flag_->store(false, std::memory_order_relaxed);
    iteration_count_.store(0, std::memory_order_relaxed);
    
    // Reset tracking
    total_usage_ = TokenUsage{};
    total_cost_ = 0.0;
    last_tool_call_.clear();
    last_tool_input_ = {};
    same_tool_count_ = 0;
    
    // Process the user message
    LoopResult result = process_user_message(user_message);
    
    // Main loop
    while (result == LoopResult::Continue && 
           !stop_requested_.load(std::memory_order_acquire) && 
           iteration_count_.load(std::memory_order_relaxed) < config_.max_iterations) {
        
        // Check for abort
        if (abort_flag_->load(std::memory_order_acquire)) {
            break;
        }
        
        result = step();
        iteration_count_.fetch_add(1, std::memory_order_relaxed);
        
        // Check for compaction
        if (result == LoopResult::Compact && config_.auto_compact) {
            session_.compact();
            token_count_.store(0, std::memory_order_relaxed);
            result = LoopResult::Continue;
        }
    }
    
    running_.store(false, std::memory_order_release);
    return result;
}

LoopResult SessionLoop::step() {
    if (stop_requested_.load(std::memory_order_acquire) || 
        iteration_count_.load(std::memory_order_relaxed) >= config_.max_iterations) {
        return LoopResult::Stop;
    }
    
    // Check for compaction
    if (needs_compaction()) {
        return LoopResult::Compact;
    }
    
    // Process LLM response
    return process_llm_response();
}

void SessionLoop::stop() {
    stop_requested_.store(true, std::memory_order_release);
    abort_flag_->store(true, std::memory_order_release);
}

LoopResult SessionLoop::process_user_message(const std::string& content) {
    // Create a user message
    core::Message user_msg(session_.id(), core::Role::User, 
                           agent_ ? agent_->name() : "system", "", "");
    
    // Add text part
    user_msg.add_part(core::Part::create_text(content));
    
    // Add to messages
    {
        std::lock_guard<std::mutex> lock(messages_mutex_);
        messages_.push_back(user_msg);
    }
    
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

LoopResult SessionLoop::process_llm_response() {
    if (!provider_ || model_id_.empty()) {
        if (on_error_) {
            on_error_("Provider or model not configured", "config_error");
        }
        return LoopResult::Error;
    }
    
    // Build messages for LLM
    auto llm_messages = build_llm_messages();
    auto tools = build_tool_definitions();
    
    // Set up stream parameters
    llm::StreamParams params;
    params.session_id = session_.id();
    params.messages = std::move(llm_messages);
    params.tools = std::move(tools);
    params.is_aborted = [this]() { return abort_flag_->load(std::memory_order_acquire); };
    
    // Track step info
    StepInfo step_info;
    step_info.step_number = iteration_count_.load(std::memory_order_relaxed) + 1;
    
    // Stream from LLM
    auto stream_result = llm::LLM::stream(*provider_, model_id_, params,
        [this](const StreamEvent& event) {
            if (on_stream_event_) {
                on_stream_event_(event);
            }
        }
    );
    
    // Check for errors
    if (stream_result.has_error()) {
        if (on_error_) {
            on_error_(stream_result.error().value_or("Unknown error"), "llm_error");
        }
        return LoopResult::Error;
    }
    
    // Get tool calls from response
    auto tool_calls = stream_result.tool_calls();
    step_info.has_tool_calls = !tool_calls.empty();
    
    // Update token usage
    auto usage = stream_result.usage();
    step_info.tokens = usage;
    total_usage_ = total_usage_ + usage;
    token_count_.fetch_add(static_cast<int>(usage.total()), std::memory_order_relaxed);
    
    // Create assistant message
    core::Message assistant_msg(session_.id(), core::Role::Assistant,
                                agent_ ? agent_->name() : "assistant", "", "");
    assistant_msg.add_part(core::Part::create_text(stream_result.final_text()));
    
    // Add assistant message to history
    {
        std::lock_guard<std::mutex> lock(messages_mutex_);
        messages_.push_back(assistant_msg);
    }
    
    if (on_message_) {
        on_message_(assistant_msg);
    }
    
    // Process tool calls
    for (const auto& tc : tool_calls) {
        step_info.tool_names.push_back(tc.name);
        
        // Check for doom loop
        if (is_doom_loop(tc.name, tc.arguments)) {
            if (on_error_) {
                on_error_(fmt::format("Doom loop detected: tool '{}' called {} times consecutively",
                                      tc.name, config_.doom_loop_threshold), "doom_loop");
            }
            return LoopResult::Error;
        }
        
        update_doom_loop_tracking(tc.name, tc.arguments);
        
        // Callback for tool call
        if (on_tool_call_) {
            on_tool_call_(tc.name, tc.id, tc.arguments);
        }
        
        // Execute tool
        auto result = execute_tool(tc.name, tc.id, tc.arguments);
        
        // Callback for tool result
        if (on_tool_result_) {
            on_tool_result_(tc.name, tc.id, result);
        }
        
        // Add tool result message
        core::Message tool_msg(session_.id(), core::Role::Tool, tc.name, "", "");
        if (result.is_error) {
            tool_msg.add_part(core::Part::create_text(fmt::format("Error: {}", result.output)));
        } else {
            tool_msg.add_part(core::Part::create_text(result.output));
        }
        
        {
            std::lock_guard<std::mutex> lock(messages_mutex_);
            messages_.push_back(tool_msg);
        }
        
        // Update token count
        token_count_.fetch_add(estimate_tokens(result.output), std::memory_order_relaxed);
    }
    
    // Fire step callback
    if (on_step_) {
        on_step_(step_info);
    }
    
    // If there were tool calls, continue the loop
    if (!tool_calls.empty()) {
        return LoopResult::Continue;
    }
    
    // No tool calls means the agent is done
    return LoopResult::Stop;
}

tool::ToolResult SessionLoop::execute_tool(const std::string& tool_name, 
                                            const std::string& call_id,
                                            const nlohmann::json& input) {
    // Get the tool from registry
    auto tool = tool::ToolRegistry::instance().get(tool_name);
    if (!tool) {
        return tool::ToolResult::error("Tool Not Found",
            fmt::format("Tool '{}' is not registered", tool_name));
    }
    
    // Build execution context
    tool::ToolContext ctx;
    ctx.session_id = session_.id();
    ctx.message_id = fmt::format("msg_{}", iteration_count_.load(std::memory_order_relaxed));
    ctx.agent = agent_ ? agent_->name() : "unknown";
    ctx.call_id = call_id;
    ctx.abort_flag = abort_flag_;
    ctx.working_directory = session_.info().directory;
    
    // Execute the tool
    return tool->execute(input, ctx);
}

bool SessionLoop::is_doom_loop(const std::string& tool_name, const nlohmann::json& input) const {
    if (tool_name != last_tool_call_) {
        return false;
    }
    // Same tool but different input is NOT a doom loop
    if (input != last_tool_input_) {
        return false;
    }
    return same_tool_count_ >= config_.doom_loop_threshold - 1;
}

void SessionLoop::update_doom_loop_tracking(const std::string& tool_name, const nlohmann::json& input) {
    if (tool_name == last_tool_call_ && input == last_tool_input_) {
        same_tool_count_++;
    } else {
        last_tool_call_ = tool_name;
        last_tool_input_ = input;
        same_tool_count_ = 1;
    }
}

bool SessionLoop::needs_compaction() const noexcept {
    return token_count_.load(std::memory_order_relaxed) >= config_.compact_threshold;
}

int SessionLoop::estimate_tokens(const std::string& text) noexcept {
    // Simple estimation: ~4 characters per token on average
    return static_cast<int>(text.size() / 4) + 1;
}

std::vector<turbot::core::llm::LLMMessage> SessionLoop::build_llm_messages() const {
    std::vector<turbot::core::llm::LLMMessage> result;
    
    // Add system message if agent has a prompt
    if (agent_ && agent_->prompt().has_value() && !agent_->prompt()->empty()) {
        result.push_back(llm::LLMMessage::system(*agent_->prompt()));
    }
    
    // Add conversation messages
    // Copy under lock, then build LLM messages outside the lock
    std::vector<core::Message> snapshot;
    {
        std::lock_guard<std::mutex> lock(messages_mutex_);
        snapshot = messages_;
    }
    for (const auto& msg : snapshot) {
        switch (msg.role()) {
            case core::Role::User: {
                auto content = msg.get_text();
                result.push_back(llm::LLMMessage::user(content));
                break;
            }
            case core::Role::Assistant: {
                auto content = msg.get_text();
                result.push_back(llm::LLMMessage::assistant(content));
                break;
            }
            case core::Role::Tool: {
                // Tool result message: use tool_call_id stored in agent field
                auto content = msg.get_text();
                result.push_back(llm::LLMMessage::tool_result(msg.info().agent, content));
                break;
            }
            case core::Role::System:
                // System messages are handled separately
                break;
        }
    }
    
    return result;
}

std::vector<turbot::core::llm::LLMToolDefinition> SessionLoop::build_tool_definitions() const {
    std::vector<turbot::core::llm::LLMToolDefinition> result;
    
    // Get all registered tools
    auto tool_names = tool::ToolRegistry::instance().names();
    
    for (const auto& name : tool_names) {
        auto tool = tool::ToolRegistry::instance().get(name);
        if (tool) {
            llm::LLMToolDefinition def;
            def.name = tool->name();
            def.description = tool->description();
            def.parameters = tool->input_schema();
            result.push_back(std::move(def));
        }
    }
    
    return result;
}

} // namespace turbot::core::session
