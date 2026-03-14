#include <turbot/core/session/session_loop.hpp>
#include <turbot/core/session/session_events.hpp>
#include <turbot/core/session/session_compaction.hpp>
#include <turbot/core/session/retry_manager.hpp>
#include <turbot/core/event/event_bus.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/tool/builtin/question_tool.hpp>
#include <turbot/core/llm/llm.hpp>
#include <turbot/core/plugin/plugin.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/string_utils.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <limits>
#include <sstream>

namespace turbot::core::session {

/// Default title assigned to a freshly-created session (before title inference).
/// Referenced by both run() (when creating the session) and
/// generate_title_if_needed() (to decide whether inference is needed).
static constexpr const char* kDefaultSessionTitle = "Interactive Session";

const char* session_status_to_string(SessionStatus status) noexcept {
    switch (status) {
        case SessionStatus::Idle:  return "idle";
        case SessionStatus::Busy:  return "busy";
        case SessionStatus::Retry: return "retry";
        default:                   return "unknown";
    }
}

/// Helper: publish a SessionStatusEvent via the global EventBus.
static void publish_status(
    const std::string& session_id,
    SessionStatus status,
    int retry_attempt = 0,
    const std::string& retry_message = {},
    int64_t retry_next_ms = 0
) {
    SessionStatusEvent ev;
    ev.session_id    = session_id;
    ev.status        = status;
    ev.retry_attempt = retry_attempt;
    ev.retry_message = retry_message;
    ev.retry_next_ms = retry_next_ms;
    turbot::core::EventBus::instance().publish(SessionStatusEvent::kEventName, std::move(ev));
}

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
        params.title = kDefaultSessionTitle;
        
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
    std::lock_guard<std::mutex> lock(agent_mutex_);
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

void SessionLoop::set_on_permission_request(PermissionCallback callback) {
    on_permission_request_ = std::move(callback);
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
    blocked_ = false;  // Reset blocked flag from any previous question rejection

    // Invalidate tool definition cache so it is rebuilt once for this run().
    // (Tools are registered at startup; the cache remains valid across steps.)
    tool_defs_dirty_ = true;

    // Broadcast session lifecycle: Idle → Busy
    publish_status(session_.id(), SessionStatus::Busy);
    
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

        // Check for blocked state (user rejected a question)
        if (blocked_) {
            result = LoopResult::Stop;
            break;
        }
        
        result = step();
        iteration_count_.fetch_add(1, std::memory_order_relaxed);
        
        // Check for compaction
        if (result == LoopResult::Compact && config_.auto_compact) {
            // Step 1: prune old tool outputs before full compaction.
            // This follows opencode's strategy: reduce context cheaply first,
            // then fall back to full summarization compaction if still needed.
            //
            // IMPORTANT lock ordering: prune() modifies messages_ under
            // messages_mutex_, but EventBus::publish() MUST be called after
            // the lock is released.  Calling publish() while holding
            // messages_mutex_ would allow a subscriber (e.g. one that calls
            // SessionLoop::messages()) to attempt a recursive lock on
            // messages_mutex_, causing a guaranteed deadlock on the same thread
            // since std::mutex is not recursive.
            PruneResult prune_result;
            {
                std::lock_guard<std::mutex> lock(messages_mutex_);
                prune_result = SessionCompaction::prune(messages_);
            } // messages_mutex_ released here

            // Publish compaction event (no lock held — safe for re-entrant subscribers).
            {
                SessionCompactionEvent ce;
                ce.session_id   = session_.id();
                ce.pruned_parts = prune_result.pruned_parts;
                ce.freed_tokens = prune_result.freed_tokens;
                ce.did_prune    = prune_result.did_prune;
                turbot::core::EventBus::instance().publish(SessionCompactionEvent::kEventName, std::move(ce));
            }

            // If pruning freed enough tokens we might not need full compaction.
            if (prune_result.did_prune) {
                // Clamp to [0, INT_MAX] before subtracting to guard against
                // token-estimation drift turning token_count_ negative.
                const int freed = static_cast<int>(std::clamp<int64_t>(
                    prune_result.freed_tokens, 0,
                    static_cast<int64_t>(std::numeric_limits<int>::max())));
                // Atomic subtract with a non-negative floor.
                int expected = token_count_.load(std::memory_order_relaxed);
                int desired;
                do {
                    desired = std::max(0, expected - freed);
                } while (!token_count_.compare_exchange_weak(
                    expected, desired, std::memory_order_relaxed));

                // Re-check whether we still need full compaction after pruning.
                if (!needs_compaction()) {
                    result = LoopResult::Continue;
                    continue;
                }
            }

            // Step 2: full compaction (summarize old messages).
            // Hook 5 — experimental.session.compacting
            // Allow plugins to inject context into the compaction prompt or
            // adjust behaviour before full summarization begins.
            // Mirrors opencode Plugin.trigger("experimental.session.compacting").
            {
                nlohmann::json ctx = {{"session_id", session_.id()}};
                nlohmann::json out = {{"system_injection", ""}};
                plugin::PluginManager::instance().trigger(
                    plugin::kHookSessionCompacting, ctx, out);
                // The system_injection value is reserved for future use by
                // session_.compact() once the compact API supports custom prompts.
                const std::string injection =
                    out.value("system_injection", std::string{});
                if (!injection.empty()) {
                    TURBOT_LOG_DEBUG("Plugin hook '{}': system injection set (len={})",
                                     plugin::kHookSessionCompacting, injection.size());
                }
            }
            session_.compact();
            token_count_.store(0, std::memory_order_relaxed);
            result = LoopResult::Continue;
        }
    }
    
    running_.store(false, std::memory_order_release);

    // Broadcast session lifecycle: Busy → Idle
    publish_status(session_.id(), SessionStatus::Idle);

    // Title Agent — derive a short session title from the first user message.
    // This runs synchronously (but very cheaply — just string slicing) after the
    // session finishes so that callers receive an up-to-date title.
    // The title is only set once: skip if the session already has a custom title.
    if (result == LoopResult::Stop || result == LoopResult::Error) {
        generate_title_if_needed();
    }

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
    // Snapshot agent_ under its own lock (lock ordering: agent_mutex_ before messages_mutex_)
    std::shared_ptr<agent::Agent> local_agent;
    {
        std::lock_guard<std::mutex> lock(agent_mutex_);
        local_agent = agent_;
    }

    // Create a user message
    core::Message user_msg(session_.id(), core::Role::User,
                           local_agent ? local_agent->name() : "system", "", "");
    
    // Add text part
    user_msg.add_part(core::Part::create_text(content));
    
    // Add to messages
    {
        std::lock_guard<std::mutex> lock(messages_mutex_);
        messages_.push_back(user_msg);
    }
    
    // Update token count
    token_count_.fetch_add(estimate_tokens(content), std::memory_order_relaxed);
    
    // Invoke legacy callback and publish Bus event (both fire simultaneously for
    // backward compatibility).
    if (on_message_) {
        on_message_(user_msg);
    }
    turbot::core::EventBus::instance().publish(
        SessionMessageEvent::kEventName,
        SessionMessageEvent{session_.id(), user_msg}
    );
    
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

    // Snapshot agent_ under its own lock before acquiring messages_mutex_
    std::shared_ptr<agent::Agent> local_agent;
    {
        std::lock_guard<std::mutex> lock(agent_mutex_);
        local_agent = agent_;
    }

    // Build messages for LLM
    auto llm_messages = build_llm_messages();
    auto tools = build_tool_definitions();

    // LiteLLM / Anthropic-via-LiteLLM compatibility: inject a _noop placeholder
    // tool when (a) the provider ID contains "litellm", (b) the message history
    // already contains tool-result messages (i.e. prior tool calls exist), and
    // (c) there are currently no active tools to offer.  Without at least one
    // tool defined, LiteLLM proxies reject the request when prior tool calls
    // are present in the conversation context.
    if (tools.empty() && provider_) {
        // Case-insensitive match so that "LiteLLM-Proxy" or "LITELLM" variants
        // are also covered. Uses turbot::utils::to_lower for consistency.
        const bool is_litellm =
            turbot::utils::to_lower(provider_->id()).find("litellm") != std::string::npos;
        if (is_litellm) {
            bool history_has_tool_calls = false;
            {
                std::lock_guard<std::mutex> lock(messages_mutex_);
                for (const auto& msg : messages_) {
                    if (msg.role() == core::Role::Tool) {
                        history_has_tool_calls = true;
                        break;
                    }
                }
            }
            if (history_has_tool_calls) {
                llm::LLMToolDefinition noop;
                noop.name = "_noop";
                noop.description = "Placeholder for LiteLLM/Anthropic proxy compatibility — "
                                   "required when message history contains tool calls but no "
                                   "active tools are needed";
                noop.parameters = {{"type", "object"}, {"properties", nlohmann::json::object()}};
                tools.push_back(std::move(noop));
                TURBOT_LOG_DEBUG("LiteLLM compat: injected _noop tool (history has prior tool calls)");
            }
        }
    }
    
    // Set up stream parameters
    llm::StreamParams params;
    params.session_id = session_.id();
    params.messages = std::move(llm_messages);
    params.tools = std::move(tools);
    params.is_aborted = [this]() { return abort_flag_->load(std::memory_order_acquire); };

    // Apply agent-level tool_choice if configured (e.g. "auto", "required", "none")
    if (local_agent) {
        const auto& agent_opts = local_agent->info().options;
        if (agent_opts.contains("tool_choice") && agent_opts["tool_choice"].is_string()) {
            params.tool_choice = agent_opts["tool_choice"].get<std::string>();
        }
    }

    // Hook 1 — chat.params
    // Allow plugins to modify temperature, top_p, and provider-specific options
    // before the LLM stream call.  Mirrors opencode Plugin.trigger("chat.params").
    {
        nlohmann::json ctx = {
            {"session_id", session_.id()},
            {"agent",      local_agent ? local_agent->name() : ""},
            {"model",      model_id_}
        };
        nlohmann::json out = {
            {"temperature", params.temperature},
            // Use JSON null to signal "no value / keep provider default" when
            // params.top_p is std::nullopt.  A plugin that explicitly sets top_p
            // must provide a number; otherwise the field remains null and we
            // preserve the original optional state.
            {"top_p",       params.top_p.has_value()
                                ? nlohmann::json(params.top_p.value())
                                : nlohmann::json(nullptr)},
            {"options",     nlohmann::json::object()}
        };
        plugin::PluginManager::instance().trigger(plugin::kHookChatParams, ctx, out);
        if (out.contains("temperature") && out["temperature"].is_number()) {
            params.temperature = out["temperature"].get<double>();
        }
        if (out.contains("top_p") && out["top_p"].is_number()) {
            // Only overwrite when the plugin provided an explicit numeric value.
            params.top_p = out["top_p"].get<double>();
        }
        TURBOT_LOG_DEBUG("Plugin hook '{}' applied: temperature={:.3f}, top_p={:.3f}",
                         plugin::kHookChatParams, params.temperature,
                         params.top_p.value_or(1.0));
    }

    // Hook 2 — chat.headers
    // Allow plugins to inject extra HTTP headers into the LLM provider request.
    // Mirrors opencode Plugin.trigger("chat.headers").
    // NOTE: Header injection into the HTTP layer is a future extension point;
    // headers collected here are logged for observability and will be forwarded
    // once the provider abstraction supports per-request headers.
    {
        nlohmann::json ctx = {
            {"session_id", session_.id()},
            {"agent",      local_agent ? local_agent->name() : ""},
            {"model",      model_id_}
        };
        nlohmann::json out = {{"headers", nlohmann::json::object()}};
        plugin::PluginManager::instance().trigger(plugin::kHookChatHeaders, ctx, out);
        if (out.contains("headers") && out["headers"].is_object() &&
            !out["headers"].empty()) {
            TURBOT_LOG_DEBUG("Plugin hook '{}': {} custom header(s) registered (pending provider support)",
                             plugin::kHookChatHeaders,
                             out["headers"].size());
        }
    }
    
    // Track step info
    StepInfo step_info;
    step_info.step_number = iteration_count_.load(std::memory_order_relaxed) + 1;
    
    // Stream from LLM — wrapped in RetryManager for automatic 429/5xx retry
    llm::LLMStreamResult stream_result;
    RetryConfig retry_config;
    // Snapshot cumulative token count before this LLM step so we can compute
    // the per-step delta (needed for M-1 fix: always use fetch_add semantics).
    int64_t prev_cumulative_tokens = total_usage_.total();
    try {
        stream_result = RetryManager::with_retry(
            [&]() -> llm::LLMStreamResult {
                // Re-check abort before each attempt; use AbortRetryException so
                // the signal propagates cleanly without going through is_retryable.
                if (abort_flag_->load(std::memory_order_acquire)) {
                    throw AbortRetryException{};
                }
                return llm::LLM::stream(*provider_, model_id_, params,
                    [this](const StreamEvent& event) {
                        if (on_stream_event_) {
                            on_stream_event_(event);
                        }
                        turbot::core::EventBus::instance().publish(
                            SessionStreamEvent::kEventName,
                            SessionStreamEvent{session_.id(), event}
                        );
                    }
                );
            },
            retry_config,
            [this, &retry_config](int attempt, const APIError& err, int delay_ms) {
                // Abort during retry wait — AbortRetryException propagates cleanly
                // past with_retry's catch block (C-1 fix).
                if (abort_flag_->load(std::memory_order_acquire)) {
                    throw AbortRetryException{};
                }
                TURBOT_LOG_WARN("LLM retry attempt {}: {} (waiting {}ms)", attempt + 1, err.what(), delay_ms);
                // Legacy callback
                if (on_error_) {
                    on_error_(fmt::format("Retrying ({}/{}): {}", attempt + 1, retry_config.max_attempts, err.what()), "retry");
                }
                // Bus: broadcast Retry status with next-fire time estimate
                int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                publish_status(
                    session_.id(), SessionStatus::Retry,
                    attempt,
                    fmt::format("Retrying ({}/{}): {}", attempt + 1, retry_config.max_attempts, err.what()),
                    now_ms + delay_ms
                );
                // Bus: error event for retry notification
                turbot::core::EventBus::instance().publish(
                    SessionErrorEvent::kEventName,
                    SessionErrorEvent{session_.id(), fmt::format("Retrying ({}/{}): {}", attempt + 1, retry_config.max_attempts, err.what()), "retry"}
                );
            }
        );
    } catch (const AbortRetryException&) {
        // User requested stop — treat as graceful Stop, not an error
        return LoopResult::Stop;
    } catch (const APIError& e) {
        // Non-retryable or exhausted retries
        const std::string code = e.code.value_or("llm_error");
        if (on_error_) {
            on_error_(e.what(), code);
        }
        turbot::core::EventBus::instance().publish(
            SessionErrorEvent::kEventName,
            SessionErrorEvent{session_.id(), e.what(), code}
        );
        return LoopResult::Error;
    }
    
    // Get tool calls from response
    auto tool_calls = stream_result.tool_calls();
    step_info.has_tool_calls = !tool_calls.empty();
    
    // Get token usage — use real LLM usage (not character estimation)
    auto usage = stream_result.usage();
    step_info.tokens = usage;
    total_usage_ = total_usage_ + usage;
    // Update token_count_ using fetch_add with the per-step delta so that
    // tool-result estimates (fetch_add'd below) are never overwritten by a
    // subsequent store.  When the provider returns no usage, fall back to a
    // character-based estimate of the assistant text (same fetch_add path).
    int64_t step_tokens = usage.total();
    if (step_tokens > 0) {
        // Compute the delta this LLM step contributed (avoids overwriting
        // tool-token estimates accumulated between steps).
        int64_t delta = total_usage_.total() - prev_cumulative_tokens;
        token_count_.fetch_add(
            static_cast<int>(std::clamp<int64_t>(delta, 0, std::numeric_limits<int>::max())),
            std::memory_order_relaxed
        );
    } else {
        // Provider returned no usage — accumulate character-based estimate
        token_count_.fetch_add(static_cast<int>(estimate_tokens(stream_result.final_text())), std::memory_order_relaxed);
    }
    
    // Create assistant message
    core::Message assistant_msg(session_.id(), core::Role::Assistant,
                                local_agent ? local_agent->name() : "assistant", "", "");
    // Hook 4 — experimental.text.complete
    // Allow plugins to post-process the final assistant text (e.g. strip artefacts,
    // append disclaimers).  Mirrors opencode Plugin.trigger("experimental.text.complete").
    std::string final_text = stream_result.final_text();
    {
        nlohmann::json ctx = {{"session_id", session_.id()}};
        nlohmann::json out = {{"text", final_text}};
        plugin::PluginManager::instance().trigger(plugin::kHookTextComplete, ctx, out);
        if (out.contains("text") && out["text"].is_string()) {
            final_text = out["text"].get<std::string>();
        }
    }
    assistant_msg.add_part(core::Part::create_text(final_text));
    
    // Add assistant message to history
    {
        std::lock_guard<std::mutex> lock(messages_mutex_);
        messages_.push_back(assistant_msg);
    }
    
    if (on_message_) {
        on_message_(assistant_msg);
    }
    turbot::core::EventBus::instance().publish(
        SessionMessageEvent::kEventName,
        SessionMessageEvent{session_.id(), assistant_msg}
    );

    // Process tool calls
    for (const auto& tc : tool_calls) {
        step_info.tool_names.push_back(tc.name);
        
        // Check for doom loop
        if (is_doom_loop(tc.name, tc.arguments)) {
            const std::string msg = fmt::format("Doom loop detected: tool '{}' called {} times consecutively",
                                                  tc.name, config_.doom_loop_threshold);
            if (on_error_) {
                on_error_(msg, "doom_loop");
            }
            turbot::core::EventBus::instance().publish(
                SessionErrorEvent::kEventName,
                SessionErrorEvent{session_.id(), msg, "doom_loop"}
            );
            return LoopResult::Error;
        }
        
        update_doom_loop_tracking(tc.name, tc.arguments);
        
        // Callback for tool call
        if (on_tool_call_) {
            on_tool_call_(tc.name, tc.id, tc.arguments);
        }
        turbot::core::EventBus::instance().publish(
            SessionToolCallEvent::kEventName,
            SessionToolCallEvent{session_.id(), tc.name, tc.id, tc.arguments}
        );
        
        // Execute tool
        auto result = execute_tool(tc.name, tc.id, tc.arguments);
        
        // Callback for tool result
        if (on_tool_result_) {
            on_tool_result_(tc.name, tc.id, result);
        }
        turbot::core::EventBus::instance().publish(
            SessionToolResultEvent::kEventName,
            SessionToolResultEvent{session_.id(), tc.name, tc.id, result}
        );
        
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
    
    // Fire step callback (legacy) and publish Bus event
    if (on_step_) {
        on_step_(step_info);
    }
    turbot::core::EventBus::instance().publish(
        SessionStepEvent::kEventName,
        SessionStepEvent{
            session_.id(),
            step_info.step_number,
            step_info.tokens,
            step_info.cost,
            step_info.has_tool_calls,
            step_info.tool_names
        }
    );
    
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
    // Get the tool from registry — try exact name first, then lowercase fallback
    // (repairToolCall: mirrors opencode experimental_repairToolCall behaviour).
    auto tool = tool::ToolRegistry::instance().get(tool_name);

    if (!tool) {
        // Try case-insensitive ASCII-lowercase match (repairToolCall fallback).
        // Uses turbot::utils::to_lower for consistency with the rest of the codebase.
        const std::string lower_name = turbot::utils::to_lower(tool_name);

        if (lower_name != tool_name) {
            tool = tool::ToolRegistry::instance().get(lower_name);
            if (tool) {
                TURBOT_LOG_WARN("repairToolCall: '{}' → '{}' (case correction)", tool_name, lower_name);
            }
        }
    }

    if (!tool) {
        // Neither exact nor lowercase match — return structured error so the LLM
        // can self-correct (same as opencode fallback to the "invalid" tool).
        nlohmann::json err_payload;
        err_payload["tool"]  = tool_name;
        err_payload["error"] = fmt::format("Tool '{}' is not registered", tool_name);
        return tool::ToolResult::error("ToolNotFound", err_payload.dump());
    }

    // Snapshot agent_ under its own lock
    std::shared_ptr<agent::Agent> local_agent;
    {
        std::lock_guard<std::mutex> lock(agent_mutex_);
        local_agent = agent_;
    }

    // Build execution context
    tool::ToolContext ctx;
    ctx.session_id = session_.id();
    ctx.message_id = fmt::format("msg_{}", iteration_count_.load(std::memory_order_relaxed));
    ctx.agent = local_agent ? local_agent->name() : "unknown";
    ctx.call_id = call_id;
    ctx.abort_flag = abort_flag_;
    ctx.working_directory = session_.info().directory;

    // ── Inject ask_permission callback ────────────────────────────────────────
    // Priority: direct callback > EventBus round-trip > deny (safe default)
    if (on_permission_request_) {
        // Direct path: caller has registered a synchronous callback (e.g. CLI/test)
        ctx.ask_permission = on_permission_request_;
    } else {
        // EventBus path: publish PermissionAskedEvent, block until PermissionRepliedEvent
        // or timeout (30 s), then return the reply.  Uses a per-request subscription
        // that is automatically unsubscribed after the reply arrives.
        ctx.ask_permission = [this, alive_weak = std::weak_ptr<bool>(perm_alive_flag_)]
                (const permission::PermissionRequest& req) -> permission::PermissionReply {

            // Subscribe to PermissionRepliedEvent *before* publishing so we
            // never miss a race-condition reply.
            std::string sub_id = EventBus::instance().subscribe<PermissionRepliedEvent>(
                PermissionRepliedEvent::kEventName,
                [alive_weak, this, req_id = req.id](const Event<PermissionRepliedEvent>& ev) {
                    if (!alive_weak.lock()) return;  // SessionLoop already destructed
                    if (ev.data.request_id != req_id) return;  // not our request
                    std::lock_guard<std::mutex> lk(perm_reply_mutex_);
                    perm_reply_map_[req_id] = ev.data.reply;
                    perm_reply_cv_.notify_all();
                }
            );

            // RAII guard ensures unsubscribe even if an exception is thrown
            struct SubGuard {
                std::string event_name, sub_id;
                ~SubGuard() { EventBus::instance().unsubscribe(event_name, sub_id); }
            } sub_guard{PermissionRepliedEvent::kEventName, sub_id};

            // Publish the permission request via EventBus
            PermissionAskedEvent asked_ev;
            asked_ev.session_id = session_.id();
            asked_ev.request   = req;
            EventBus::instance().publish(PermissionAskedEvent::kEventName, std::move(asked_ev));

            // Wait up to 30 s for a reply
            permission::PermissionReply reply = permission::PermissionReply::reject(); // safe default
            {
                std::unique_lock<std::mutex> lk(perm_reply_mutex_);
                perm_reply_cv_.wait_for(lk, std::chrono::seconds(30), [this, &req]() {
                    return perm_reply_map_.count(req.id) > 0;
                });
                auto it = perm_reply_map_.find(req.id);
                if (it != perm_reply_map_.end()) {
                    reply = it->second;
                    perm_reply_map_.erase(it);
                } else {
                    TURBOT_LOG_WARN("SessionLoop: permission request '{}' timed out after 30s — defaulting to Reject", req.id);
                }
            }

            return reply;
            // sub_guard destructs here → unsubscribes automatically
        };
    }

    // Execute the tool (wrap in try-catch to prevent tool exceptions from crashing the session)
    try {
        return tool->execute(input, ctx);
    } catch (const tool::builtin::Question::RejectedError& e) {
        // User dismissed the question — set blocked_ so the main loop stops
        // after this tool result is appended to the conversation.
        TURBOT_LOG_INFO("QuestionTool: user rejected question in session {}", session_.id());
        blocked_ = true;
        return tool::ToolResult::error("QuestionRejected", e.what());
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("Tool '{}' threw exception: {}", tool_name, e.what());
        return tool::ToolResult::error("ToolException", e.what());
    } catch (...) {
        TURBOT_LOG_ERROR("Tool '{}' threw unknown exception", tool_name);
        return tool::ToolResult::error("ToolException", "Unknown exception in tool execution");
    }
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
    // Improved token estimation:
    // - ASCII characters: ~4 chars per token (standard BPE assumption)
    // - Multi-byte UTF-8 sequences (CJK, emoji, etc.): ~1–1.5 chars per token
    //   We use a conservative ratio of 1.5 bytes-per-token for non-ASCII bytes.
    //
    // Strategy: count ASCII bytes and multi-byte sequence starters separately.
    //   ascii_chars / 4  +  non_ascii_chars * 1  ≈ token estimate
    int ascii_count = 0;
    int non_ascii_count = 0;
    for (unsigned char c : text) {
        if (c < 0x80) {
            ++ascii_count;
        } else if ((c & 0xC0) != 0x80) {
            // Leading byte of a multi-byte sequence (0xC0..0xFF) = one logical code-point
            ++non_ascii_count;
        }
        // Continuation bytes (0x80..0xBF) are skipped — already counted above
    }
    return (ascii_count / 4) + non_ascii_count + 1;
}

std::vector<turbot::core::llm::LLMMessage> SessionLoop::build_llm_messages() const {
    std::vector<turbot::core::llm::LLMMessage> result;

    // Snapshot agent_ under its own lock (lock ordering: agent_mutex_ before messages_mutex_)
    std::shared_ptr<agent::Agent> local_agent;
    {
        std::lock_guard<std::mutex> lock(agent_mutex_);
        local_agent = agent_;
    }

    // Add system message if agent has a prompt
    if (local_agent && local_agent->prompt().has_value() && !local_agent->prompt()->empty()) {
        result.push_back(llm::LLMMessage::system(*local_agent->prompt()));
    }

    // Hook 3 — experimental.chat.system.transform
    // Allow plugins to transform the system-prompt array (append, prepend, replace).
    // Mirrors opencode Plugin.trigger("experimental.chat.system.transform").
    // We collect all existing system messages into a JSON array, run the hook,
    // then sync the (possibly modified) array back into result.
    {
        nlohmann::json system_array = nlohmann::json::array();
        for (const auto& m : result) {
            if (m.role == provider::ChatRole::System) {
                system_array.push_back(m.content);
            }
        }

        nlohmann::json ctx = {
            {"session_id", session_.id()},
            {"model",      model_id_}
        };
        nlohmann::json out = {{"system", system_array}};
        plugin::PluginManager::instance().trigger(plugin::kHookSystemTransform, ctx, out);

        // Rebuild the system portion of result if the hook changed anything.
        if (out.contains("system") && out["system"].is_array() &&
            out["system"] != system_array) {
            // Remove old system messages from result (they are always at the front).
            result.erase(
                std::remove_if(result.begin(), result.end(),
                               [](const llm::LLMMessage& m) {
                                   return m.role == provider::ChatRole::System;
                               }),
                result.end());
            // Prepend the (possibly modified) system messages.
            std::vector<llm::LLMMessage> new_sys;
            new_sys.reserve(out["system"].size());
            for (const auto& s : out["system"]) {
                if (s.is_string()) {
                    new_sys.push_back(llm::LLMMessage::system(s.get<std::string>()));
                }
            }
            result.insert(result.begin(), new_sys.begin(), new_sys.end());
            TURBOT_LOG_DEBUG("Plugin hook '{}' modified system prompt ({} → {} part(s))",
                             plugin::kHookSystemTransform,
                             system_array.size(), out["system"].size());
        }
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
    // Return cached definitions if still valid.
    // ToolRegistry tools are registered at startup and do not change at runtime,
    // so we only rebuild once per run() invocation (tool_defs_dirty_ is reset in run()).
    if (!tool_defs_dirty_) {
        return cached_tool_defs_;
    }

    cached_tool_defs_.clear();

    // Get all registered tools
    auto tool_names = tool::ToolRegistry::instance().names();
    cached_tool_defs_.reserve(tool_names.size());

    for (const auto& name : tool_names) {
        auto tool = tool::ToolRegistry::instance().get(name);
        if (tool) {
            llm::LLMToolDefinition def;
            def.name = tool->name();
            def.description = tool->description();
            def.parameters = tool->input_schema();
            cached_tool_defs_.push_back(std::move(def));
        }
    }

    tool_defs_dirty_ = false;
    return cached_tool_defs_;
}

// ============================================================================
// generate_title_if_needed
// ============================================================================

void SessionLoop::generate_title_if_needed() {
    // Only set the title if the session does not already have a non-default one.
    // A freshly created session has the title kDefaultSessionTitle (set in run()).
    // We replace it with a short snippet derived from the first user message.
    if (session_.info().title != kDefaultSessionTitle) {
        return; // Custom title already set — leave it alone.
    }

    // Find the first user message in the conversation snapshot.
    std::string first_user_text;
    {
        std::lock_guard<std::mutex> lock(messages_mutex_);
        for (const auto& msg : messages_) {
            if (msg.role() == core::Role::User) {
                first_user_text = msg.get_text();
                break;
            }
        }
    }

    if (first_user_text.empty()) {
        return;
    }

    // Derive a title: take the first line, trim whitespace, truncate to 60 chars.
    // This is the "cheap" title agent implementation: no LLM call required.
    // A future enhancement could run the `title` AgentInfo prompt through the LLM.
    std::string title = first_user_text.substr(0, first_user_text.find('\n'));
    // Strip leading/trailing whitespace
    const auto ltrim = title.find_first_not_of(" \t\r\n");
    if (ltrim != std::string::npos) {
        title = title.substr(ltrim);
    }
    const auto rtrim = title.find_last_not_of(" \t\r\n");
    if (rtrim != std::string::npos) {
        title = title.substr(0, rtrim + 1);
    }
    // Truncate to 60 characters, appending "…" if needed
    constexpr std::size_t kMaxLen = 60;
    if (title.size() > kMaxLen) {
        title = title.substr(0, kMaxLen) + "…";
    }

    if (title.empty()) {
        return;
    }

    // Persist and publish — EventBus::publish must be called outside any lock.
    session_.set_title(title);
    turbot::core::EventBus::instance().publish(
        SessionTitleUpdatedEvent::kEventName,
        SessionTitleUpdatedEvent{session_.id(), title}
    );
    TURBOT_LOG_DEBUG("Title Agent: session '{}' titled '{}'", session_.id(), title);
}

} // namespace turbot::core::session