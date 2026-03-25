#pragma once

/**
 * @file provider_transform.hpp
 * @brief Provider message/option transformation layer.
 *
 * C++ port of OpenCode packages/opencode/src/provider/transform.ts
 * (ProviderTransform namespace).  All functions are pure / stateless.
 */

#include <turbot/core/common/export.hpp>
#include <turbot/core/provider/provider.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>

namespace turbot::core::provider::ProviderTransform {

/// Maximum output tokens (mirrors OUTPUT_TOKEN_MAX = 32,000).
inline constexpr int OUTPUT_TOKEN_MAX = 32'000;

// ---------------------------------------------------------------------------
// Message transformation
// ---------------------------------------------------------------------------

/**
 * @brief Transform a message list before sending to an LLM.
 *
 * Applies in order:
 *  1. unsupported_parts  — replace unsupported file/image parts with error text
 *  2. normalize_messages — provider-specific normalisation (Anthropic/Mistral/…)
 *  3. apply_caching      — add cache-control hints for Anthropic / Bedrock
 *  4. remap provider-options keys from stored providerID to expected SDK key
 *
 * @param messages  List of LLM messages as JSON (role/content objects).
 * @param model     Model metadata (provider_id, api.npm, capabilities …).
 * @param options   Per-call provider options (may be empty JSON object).
 * @return Transformed message list.
 */
[[nodiscard]] TURBOT_CORE_API std::vector<nlohmann::json>
message(std::vector<nlohmann::json> messages,
        const ModelInfo&             model,
        const nlohmann::json&        options);

// ---------------------------------------------------------------------------
// Model-level defaults
// ---------------------------------------------------------------------------

/**
 * @brief Return the recommended temperature for a model, or nullopt.
 *
 * Mirrors ProviderTransform.temperature() in transform.ts.
 */
[[nodiscard]] TURBOT_CORE_API std::optional<double>
temperature(const ModelInfo& model);

/**
 * @brief Return the recommended top_p for a model, or nullopt.
 */
[[nodiscard]] TURBOT_CORE_API std::optional<double>
top_p(const ModelInfo& model);

/**
 * @brief Return the recommended top_k for a model, or nullopt.
 */
[[nodiscard]] TURBOT_CORE_API std::optional<int>
top_k(const ModelInfo& model);

/**
 * @brief Return the effective max output tokens for a model.
 *
 * = min(model.limit.output, OUTPUT_TOKEN_MAX)
 */
[[nodiscard]] TURBOT_CORE_API int
max_output_tokens(const ModelInfo& model);

// ---------------------------------------------------------------------------
// Provider options
// ---------------------------------------------------------------------------

/**
 * @brief Build the providerOptions block for an LLM call.
 *
 * Mirrors ProviderTransform.options() — adds store/usage/thinkingConfig/…
 * provider-specific keys based on model.provider_id / model.api.npm.
 *
 * @param model          Model metadata.
 * @param session_id     Current session ID (used for cache key).
 * @param extra_options  Extra caller-supplied provider options (may be empty).
 * @return JSON object to be merged into the providerOptions block.
 */
[[nodiscard]] TURBOT_CORE_API nlohmann::json
options(const ModelInfo&      model,
        const std::string&     session_id,
        const nlohmann::json&  extra_options = {});

/**
 * @brief Build reduced providerOptions for "small" / background LLM calls.
 *
 * Mirrors ProviderTransform.smallOptions() — lower reasoning effort, etc.
 */
[[nodiscard]] TURBOT_CORE_API nlohmann::json
small_options(const ModelInfo& model);

/**
 * @brief Wrap raw options into the correct providerOptions namespace.
 *
 * Mirrors ProviderTransform.providerOptions() — routes options under the
 * right SDK key (e.g. { anthropic: { … } }).
 *
 * @param model    Model metadata.
 * @param opts     Raw options JSON object.
 * @return Options wrapped in the provider-specific namespace.
 */
[[nodiscard]] TURBOT_CORE_API nlohmann::json
provider_options(const ModelInfo& model, const nlohmann::json& opts);

// ---------------------------------------------------------------------------
// Reasoning variants
// ---------------------------------------------------------------------------

/**
 * @brief Return available reasoning effort variants for a model.
 *
 * Mirrors ProviderTransform.variants() — maps effort level names
 * (low/medium/high/max/…) to provider-specific option objects.
 *
 * @return JSON object where keys are effort names and values are option blobs,
 *         or an empty object if the model has no reasoning capability.
 */
[[nodiscard]] TURBOT_CORE_API nlohmann::json
variants(const ModelInfo& model);

// ---------------------------------------------------------------------------
// JSON schema sanitisation
// ---------------------------------------------------------------------------

/**
 * @brief Sanitise a JSON schema for provider compatibility.
 *
 * Currently applies Gemini-specific fixes:
 *  - Convert integer enums → string enums
 *  - Ensure array items always have a type
 *  - Strip properties/required from non-object types
 *
 * @param model   Target model (determines which fixes are applied).
 * @param schema  Input JSON schema.
 * @return Sanitised JSON schema.
 */
[[nodiscard]] TURBOT_CORE_API nlohmann::json
schema(const ModelInfo& model, nlohmann::json schema);

} // namespace turbot::core::provider::ProviderTransform
