#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/storage/database.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace turbot::core::account {

/// Persisted state for the active authentication session.
struct TURBOT_CORE_API AccountState {
    std::optional<std::string> account_id;    ///< Active account ID
    std::optional<std::string> access_token;  ///< OAuth access token
    std::optional<std::string> refresh_token; ///< OAuth refresh token
    std::optional<int64_t>     token_expiry;  ///< Expiry timestamp (ms since epoch)
};

/// Singleton store for account / token persistence (SQLite backend).
///
/// Aligned with OpenCode's account SQLite schema:
///   - account table: one row per account (id, email, url, time_*)
///   - account_state table: singleton row for the active auth session
class TURBOT_CORE_API AccountStore {
public:
    static AccountStore& instance();

    /// Initialise the store with a database.  First call wins.
    void init(std::shared_ptr<turbot::storage::Database> db);

    /// Returns true if a database has been provided.
    [[nodiscard]] bool is_initialized() const noexcept;

    /// Reset the store (for testing).
    void reset();

    // ── account CRUD ──────────────────────────────────────────────────────

    /// Persist an account row (UPSERT).
    /// Fields: id, email, url; time_created preserved on update.
    [[nodiscard]] bool save_account(const std::string& id,
                                    const std::string& email,
                                    const std::string& url);

    /// Find an account by ID.
    [[nodiscard]] std::optional<nlohmann::json> find_account(const std::string& id);

    /// List all accounts ordered by time_updated DESC.
    [[nodiscard]] std::vector<nlohmann::json> list_accounts();

    /// Delete an account by ID.
    bool remove_account(const std::string& id);

    // ── account_state (singleton) ─────────────────────────────────────────

    /// Persist the active auth state (single-row upsert, key = 'singleton').
    [[nodiscard]] bool save_state(const AccountState& state);

    /// Load the active auth state.  Returns nullopt if not initialised.
    [[nodiscard]] std::optional<AccountState> load_state();

private:
    AccountStore() = default;

    void ensure_schema();   // Called with mutex_ held and db_ valid.

    mutable std::mutex mutex_;
    std::shared_ptr<turbot::storage::Database> db_;
    bool schema_ready_ = false;
};

} // namespace turbot::core::account
