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

} // namespace turbot::core::provider
