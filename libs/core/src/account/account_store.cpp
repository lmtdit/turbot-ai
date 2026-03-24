#include <turbot/core/account/account_store.hpp>
#include <turbot/core/common/logger.hpp>
#include <chrono>
#include <stdexcept>

namespace turbot::core::account {

static int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

AccountStore& AccountStore::instance() {
    static AccountStore inst;
    return inst;
}

void AccountStore::init(std::shared_ptr<turbot::storage::Database> db) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) return;  // First call wins.
    db_ = std::move(db);
    ensure_schema();
}

bool AccountStore::is_initialized() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return db_ != nullptr;
}

void AccountStore::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    db_.reset();
    schema_ready_ = false;
}

// ─── ensure_schema ────────────────────────────────────────────────────────────

void AccountStore::ensure_schema() {
    if (schema_ready_) return;

    // account table — aligned with OpenCode account.sql.ts schema.
    db_->execute(R"SQL(
        CREATE TABLE IF NOT EXISTS account (
            id           TEXT    PRIMARY KEY NOT NULL,
            email        TEXT    NOT NULL,
            url          TEXT    NOT NULL,
            time_created INTEGER NOT NULL,
            time_updated INTEGER NOT NULL
        )
    )SQL");

    // account_state — singleton row for the active auth session.
    // id = 'singleton' (only one row ever; INSERT OR REPLACE used for updates).
    db_->execute(R"SQL(
        CREATE TABLE IF NOT EXISTS account_state (
            id            TEXT    PRIMARY KEY NOT NULL DEFAULT 'singleton',
            account_id    TEXT,
            access_token  TEXT,
            refresh_token TEXT,
            token_expiry  INTEGER
        )
    )SQL");

    schema_ready_ = true;
}

// ─── save_account ─────────────────────────────────────────────────────────────

bool AccountStore::save_account(const std::string& id,
                                 const std::string& email,
                                 const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    try {
        const int64_t now = now_ms();
        db_->execute(R"SQL(
            INSERT INTO account (id, email, url, time_created, time_updated)
            VALUES (?, ?, ?, ?, ?)
            ON CONFLICT(id) DO UPDATE SET
                email        = excluded.email,
                url          = excluded.url,
                time_updated = excluded.time_updated
        )SQL", {id, email, url, now, now});
        return true;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("AccountStore::save_account failed for {}: {}", id, e.what());
        return false;
    }
}

// ─── find_account ─────────────────────────────────────────────────────────────

std::optional<nlohmann::json> AccountStore::find_account(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return std::nullopt;

    try {
        return db_->execute_one(
            "SELECT * FROM account WHERE id = ?", {id});
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("AccountStore::find_account failed for {}: {}", id, e.what());
        return std::nullopt;
    }
}

// ─── list_accounts ────────────────────────────────────────────────────────────

std::vector<nlohmann::json> AccountStore::list_accounts() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return {};

    try {
        auto result = db_->execute(
            "SELECT * FROM account ORDER BY time_updated DESC");
        return result.rows;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("AccountStore::list_accounts failed: {}", e.what());
        return {};
    }
}

// ─── remove_account ───────────────────────────────────────────────────────────

bool AccountStore::remove_account(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    try {
        auto result = db_->execute(
            "DELETE FROM account WHERE id = ?", {id});
        return result.affected_rows > 0;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("AccountStore::remove_account failed for {}: {}", id, e.what());
        return false;
    }
}

// ─── save_state ───────────────────────────────────────────────────────────────

bool AccountStore::save_state(const AccountState& state) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    try {
        nlohmann::json acct_j  = state.account_id  ? nlohmann::json(*state.account_id)  : nlohmann::json(nullptr);
        nlohmann::json at_j    = state.access_token ? nlohmann::json(*state.access_token) : nlohmann::json(nullptr);
        nlohmann::json rt_j    = state.refresh_token? nlohmann::json(*state.refresh_token): nlohmann::json(nullptr);
        nlohmann::json exp_j   = state.token_expiry ? nlohmann::json(*state.token_expiry) : nlohmann::json(nullptr);

        db_->execute(R"SQL(
            INSERT INTO account_state (id, account_id, access_token, refresh_token, token_expiry)
            VALUES ('singleton', ?, ?, ?, ?)
            ON CONFLICT(id) DO UPDATE SET
                account_id    = excluded.account_id,
                access_token  = excluded.access_token,
                refresh_token = excluded.refresh_token,
                token_expiry  = excluded.token_expiry
        )SQL", {acct_j, at_j, rt_j, exp_j});
        return true;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("AccountStore::save_state failed: {}", e.what());
        return false;
    }
}

// ─── load_state ───────────────────────────────────────────────────────────────

std::optional<AccountState> AccountStore::load_state() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return std::nullopt;

    try {
        auto row = db_->execute_one(
            "SELECT * FROM account_state WHERE id = 'singleton'");
        if (!row) return std::nullopt;

        AccountState state;
        const auto& j = *row;
        if (j.contains("account_id") && !j["account_id"].is_null())
            state.account_id = j["account_id"].get<std::string>();
        if (j.contains("access_token") && !j["access_token"].is_null())
            state.access_token = j["access_token"].get<std::string>();
        if (j.contains("refresh_token") && !j["refresh_token"].is_null())
            state.refresh_token = j["refresh_token"].get<std::string>();
        if (j.contains("token_expiry") && !j["token_expiry"].is_null())
            state.token_expiry = j["token_expiry"].get<int64_t>();
        return state;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("AccountStore::load_state failed: {}", e.what());
        return std::nullopt;
    }
}

} // namespace turbot::core::account
