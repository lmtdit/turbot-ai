#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/storage/database.hpp>
#include <turbot/storage/sqlite_database.hpp>
#include <fstream>
#include <memory>

using namespace turbot::storage;
using namespace turbot::storage::sqlite;
using namespace turbot::test;

// Helper to create database
static std::unique_ptr<SQLiteDatabase> create_db(const std::string& path = ":memory:") {
    DatabaseConfig config;
    config.path = path;
    return std::make_unique<SQLiteDatabase>(config);
}

// ==================== DatabaseConfig 测试 ====================

TEST_CASE("Storage.DatabaseConfig.Defaults", "[Storage]") {
    DatabaseConfig config;
    REQUIRE(config.path.empty());
    REQUIRE(config.read_only == false);
    REQUIRE(config.timeout == 30);
    REQUIRE(config.journal_wal == true);
    REQUIRE(config.foreign_keys == true);
}

// ==================== QueryResult 测试 ====================

TEST_CASE("Storage.QueryResult.Defaults", "[Storage]") {
    QueryResult result;
    REQUIRE(result.rows.empty());
    REQUIRE(result.affected_rows == 0);
}

// ==================== SqliteDatabase 测试 ====================

TEST_CASE("Storage.Sqlite.OpenInMemory", "[Storage]") {
    auto db = create_db();
    REQUIRE(db != nullptr);
    REQUIRE(db->is_open());
}

TEST_CASE("Storage.Sqlite.OpenFile", "[Storage]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto db_path = (tmp.path() / "test.db").string();
    auto db = create_db(db_path);
    
    REQUIRE(db != nullptr);
    REQUIRE(db->is_open());
    REQUIRE(std::filesystem::exists(db_path));
}

TEST_CASE("Storage.Sqlite.Execute", "[Storage]") {
    auto db = create_db();
    REQUIRE(db != nullptr);
    
    // Create table
    db->execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)");
    
    // Insert
    auto result = db->execute(
        "INSERT INTO test (name) VALUES (?)",
        {R"("test-name")"_json}
    );
    REQUIRE(result.affected_rows == 1);
    
    // Select
    auto rows = db->execute("SELECT * FROM test");
    REQUIRE(rows.rows.size() == 1);
}

TEST_CASE("Storage.Sqlite.ExecuteOne", "[Storage]") {
    auto db = create_db();
    db->execute("CREATE TABLE items (id INTEGER PRIMARY KEY, value TEXT)");
    db->execute("INSERT INTO items (value) VALUES (?)", {R"("item1")"_json});
    db->execute("INSERT INTO items (value) VALUES (?)", {R"("item2")"_json});
    
    auto row = db->execute_one("SELECT * FROM items WHERE id = 1");
    REQUIRE(row.has_value());
    REQUIRE((*row)["id"] == 1);
}

TEST_CASE("Storage.Sqlite.ExecuteScalar", "[Storage]") {
    auto db = create_db();
    db->execute("CREATE TABLE counters (name TEXT PRIMARY KEY, count INTEGER)");
    db->execute("INSERT INTO counters VALUES ('test', 42)");
    
    auto count = db->execute_scalar<int>("SELECT count FROM counters WHERE name = 'test'");
    REQUIRE(count.has_value());
    REQUIRE(*count == 42);
}

TEST_CASE("Storage.Sqlite.Transaction", "[Storage]") {
    auto db = create_db();
    db->execute("CREATE TABLE accounts (id INTEGER PRIMARY KEY, balance INTEGER)");
    db->execute("INSERT INTO accounts (balance) VALUES (100)");
    db->execute("INSERT INTO accounts (balance) VALUES (100)");
    
    // Transaction with commit
    {
        auto txn = db->begin_transaction();
        db->execute("UPDATE accounts SET balance = balance - 50 WHERE id = 1");
        db->execute("UPDATE accounts SET balance = balance + 50 WHERE id = 2");
        txn->commit();
    }
    
    auto balance1 = db->execute_scalar<int>("SELECT balance FROM accounts WHERE id = 1");
    auto balance2 = db->execute_scalar<int>("SELECT balance FROM accounts WHERE id = 2");
    REQUIRE(*balance1 == 50);
    REQUIRE(*balance2 == 150);
}

TEST_CASE("Storage.Sqlite.TransactionRollback", "[Storage]") {
    auto db = create_db();
    db->execute("CREATE TABLE data (id INTEGER PRIMARY KEY, value TEXT)");
    db->execute("INSERT INTO data (value) VALUES ('original')");
    
    // Transaction with rollback
    {
        auto txn = db->begin_transaction();
        db->execute("UPDATE data SET value = 'modified' WHERE id = 1");
        // No commit - destructor will rollback
    }
    
    auto value = db->execute_scalar<std::string>("SELECT value FROM data WHERE id = 1");
    REQUIRE(*value == "original");
}

TEST_CASE("Storage.Sqlite.Migration", "[Storage]") {
    auto db = create_db();
    
    db->migrate("create_users", "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)", 1);
    db->migrate("add_email", "ALTER TABLE users ADD COLUMN email TEXT", 2);
    
    // Verify migration was applied
    auto result = db->execute("SELECT * FROM users");
    // Table should exist with id, name, email columns
}

TEST_CASE("Storage.Sqlite.HealthCheck", "[Storage]") {
    auto db = create_db();
    REQUIRE(db->health_check());
}

TEST_CASE("Storage.Sqlite.Close", "[Storage]") {
    auto db = create_db();
    REQUIRE(db->is_open());
    
    db->close();
    REQUIRE_FALSE(db->is_open());
}

TEST_CASE("Storage.Sqlite.ExecuteBatch", "[Storage]") {
    auto db = create_db();
    db->execute("CREATE TABLE batch_test (id INTEGER PRIMARY KEY, value TEXT)");
    
    std::vector<std::vector<nlohmann::json>> params_list = {
        {R"("a")"_json},
        {R"("b")"_json},
        {R"("c")"_json}
    };
    
    auto result = db->execute_batch("INSERT INTO batch_test (value) VALUES (?)", params_list);
    REQUIRE(result.affected_rows == 3);
    
    auto rows = db->execute("SELECT * FROM batch_test ORDER BY id");
    REQUIRE(rows.rows.size() == 3);
}
