#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/provider/provider.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace turbot::core::provider {

/// Provider factory function type
using ProviderFactory = std::function<ProviderPtr(const ProviderConfig& config)>;

/// Provider manager - singleton for managing AI providers
class TURBOT_CORE_API ProviderManager {
public:
    /// Get singleton instance
    static ProviderManager& instance() noexcept;

    // Delete copy and move
    ProviderManager(const ProviderManager&) = delete;
    ProviderManager& operator=(const ProviderManager&) = delete;

    /// Register a provider instance
    void register_provider(ProviderPtr provider);

    /// Unregister a provider by ID
    bool unregister_provider(const std::string& provider_id);

    /// Get a provider by ID
    [[nodiscard]] std::optional<ProviderPtr> get_provider(const std::string& provider_id) const;

    /// Get all registered providers
    [[nodiscard]] std::vector<ProviderPtr> list_providers() const;

    /// Check if a provider is registered
    [[nodiscard]] bool has_provider(const std::string& provider_id) const;

    /// Get all available models from all providers
    [[nodiscard]] std::vector<ModelInfo> list_all_models() const;

    /// Find model info across all providers
    [[nodiscard]] std::optional<ModelInfo> find_model(const std::string& model_id) const;

    /// Find provider that supports a model
    [[nodiscard]] std::optional<ProviderPtr> find_provider_for_model(const std::string& model_id) const;

    /// Set default provider
    void set_default_provider(const std::string& provider_id);

    /// Get default provider
    [[nodiscard]] std::optional<ProviderPtr> get_default_provider() const;

    /// Clear all providers
    void clear();

    /// Register a provider factory for lazy creation
    void register_factory(const std::string& provider_id, ProviderFactory factory);

    /// Create provider from factory
    [[nodiscard]] std::optional<ProviderPtr> create_provider(
        const std::string& provider_id,
        const ProviderConfig& config
    );

private:
    ProviderManager() = default;

    // mutex_ is mutable to allow locking in const methods (e.g., get_provider, list_providers)
    // This follows the "logical constness" pattern where thread-safety is an implementation detail
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ProviderPtr> providers_;
    std::unordered_map<std::string, ProviderFactory> factories_;
    std::string default_provider_id_;
};

/// Provider registry helper for automatic registration
template<typename T>
class ProviderRegistrar {
public:
    explicit ProviderRegistrar(std::string id) : id_(std::move(id)) {
        ProviderManager::instance().register_factory(
            id_,
            [](const ProviderConfig& config) -> ProviderPtr {
                return std::make_shared<T>(config);
            }
        );
    }

private:
    std::string id_;
};

// ============================================================================
// T10: ModelsDev — models.dev snapshot support
// Mirrors OpenCode provider/models.ts ModelsDev namespace.
// Priority chain: local cache file → online fetch → registered provider models.
// ============================================================================

/// Represents a provider entry in the models.dev snapshot.
struct TURBOT_CORE_API ModelsDevProvider {
    std::string id;                                   ///< Provider ID (e.g. "anthropic")
    std::string name;                                 ///< Human-readable name
    std::vector<std::string> env;                     ///< Required env vars (e.g. {"ANTHROPIC_API_KEY"})
    nlohmann::json models;                            ///< Raw models map (model_id → model object)
};

/// ModelsDev — static/dynamic model snapshot service.
///
/// Provides the full models.dev catalogue used for model selection in the TUI.
/// On first call, loads in this priority order:
///   1. Local cache file (Global::Path::cache / "models.json")
///   2. Online fetch from models.dev API
///   3. Fall back to the built-in list from registered ProviderManager instances
class TURBOT_CORE_API ModelsDev {
public:
    /// Base URL for the models.dev API. Can be overridden via OPENCODE_MODELS_URL env var.
    static constexpr const char* kDefaultModelsUrl = "https://models.dev";

    /// Get the global ModelsDev singleton.
    static ModelsDev& instance() noexcept;

    ModelsDev(const ModelsDev&) = delete;
    ModelsDev& operator=(const ModelsDev&) = delete;

    /// Fetch and return the full provider→model map as raw JSON.
    /// Uses the priority chain: local cache → network → built-in fallback.
    /// Result is cached in memory after the first successful fetch.
    ///
    /// @param force_refresh  If true, bypass the in-memory cache and re-fetch.
    /// @return JSON object mapping provider ID → provider entry, or empty object on failure.
    [[nodiscard]] nlohmann::json get(bool force_refresh = false);

    /// Refresh the snapshot from the network and persist it to the local cache file.
    /// @return true if the refresh succeeded.
    bool refresh();

    /// Clear the in-memory cached snapshot (does NOT delete the on-disk cache).
    void clear_cache() noexcept;

private:
    ModelsDev() = default;

    mutable std::mutex cache_mutex_;
    nlohmann::json cached_;        ///< In-memory snapshot (empty = not yet loaded)
    bool cache_loaded_ = false;

    /// Path to the on-disk cache file.
    static std::string cache_file_path();

    /// Attempt to load snapshot from the local cache file.
    static nlohmann::json load_from_cache();

    /// Fetch snapshot from the models.dev network API.
    static nlohmann::json fetch_from_network();

    /// Build a minimal fallback snapshot from ProviderManager's registered models.
    static nlohmann::json build_fallback_snapshot();
};

} // namespace turbot::core::provider
