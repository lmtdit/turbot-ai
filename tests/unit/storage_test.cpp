#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <turbot/storage/sqlite_database.hpp>
#include <turbot/storage/migration.hpp>
#include <turbot/core/logger.hpp>

#include <filesystem>
#include <fstream>

using namespace turbot::storage;
using namespace turbot::storage::sqlite;

// Helper to create a test database
class TestDatabase {
public:
    TestDatabase() {
        DatabaseConfig config;
        config.path = ":memory:";
        db_ = std::make_shared<SQLiteDatabase>(config);
    }

    explicit TestDatabase(const std::string& path) {
        DatabaseConfig config;
        config.path = path;
        db_ = std::make_shared<SQLiteDatabase>(config);
    }

    ~TestDatabase() {
        db_.reset();
    }

    std::shared_ptr<Database> db() const { return db_; }
    std::shared_ptr<SQLiteDatabase> sqlite_db() const { return db_; }

private:
    std::shared_ptr<SQLiteDatabase> db_;
};

// ============================================================================
// DatabaseConfig Tests
// ============================================================================

TEST_CASE("DatabaseConfig::defaults", "[storage][config]") {
    DatabaseConfig config;
    
    REQUIRE(config.path.empty());
    REQUIRE_FALSE(config.read_only);
    REQUIRE(config.timeout == 30);
    REQUIRE(config.cache_size == -2000);
    REQUIRE(config.journal_wal);
    REQUIRE(config.foreign_keys);
}

// ============================================================================
// SQLiteDatabase Tests
// ============================================================================

TEST_CASE("SQLiteDatabase::constructor", "[storage][sqlite]") {
    SECTION("in-memory database") {
        TestDatabase test_db;
        REQUIRE(test_db.db()->is_open());
    }
    
    SECTION("file database") {
        const std::string db_path = "/tmp/test_turbot_storage.db";
        
        // Clean up any existing file
        std::filesystem::remove(db_path);
        
        {
            TestDatabase test_db(db_path);
            REQUIRE(test_db.db()->is_open());
            REQUIRE(std::filesystem::exists(db_path));
        }
        
        // Clean up
        std::filesystem::remove(db_path);
    }
}

TEST_CASE("SQLiteDatabase::execute", "[storage][sqlite]") {
    TestDatabase test_db;
    auto db = test_db.db();
    
    SECTION("create table") {
        auto result = db->execute(R"(
            CREATE TABLE test (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL,
                value INTEGER
            )
        )");
        
        REQUIRE(result.affected_rows == 0);
        REQUIRE(result.rows.empty());
    }
    
    SECTION("insert and select") {
        db->execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)");
        
        auto insert_result = db->execute(
            "INSERT INTO test (name) VALUES (?)",
            {nlohmann::json("test_name")}
        );
        REQUIRE(insert_result.affected_rows == 1);
        
        auto select_result = db->execute("SELECT * FROM test");
        REQUIRE(select_result.rows.size() == 1);
        REQUIRE(select_result.rows[0]["name"] == "test_name");
    }
}

TEST_CASE("SQLiteDatabase::execute_one", "[storage][sqlite]") {
    TestDatabase test_db;
    auto db = test_db.db();
    
    db->execute("CREATE TABLE test (id INTEGER PRIMARY KEY, value INTEGER)");
    db->execute("INSERT INTO test (value) VALUES (42)");
    db->execute("INSERT INTO test (value) VALUES (100)");
    
    SECTION("returns first row") {
        auto row = db->execute_one("SELECT * FROM test WHERE value = 42");
        REQUIRE(row.has_value());
        REQUIRE((*row)["value"] == 42);
    }
    
    SECTION("returns nullopt for no match") {
        auto row = db->execute_one("SELECT * FROM test WHERE value = 999");
        REQUIRE_FALSE(row.has_value());
    }
}

TEST_CASE("SQLiteDatabase::execute_scalar", "[storage][sqlite]") {
    TestDatabase test_db;
    auto db = test_db.db();
    
    db->execute("CREATE TABLE test (id INTEGER PRIMARY KEY, value INTEGER)");
    db->execute("INSERT INTO test (value) VALUES (42)");
    
    SECTION("returns scalar value") {
        auto value = db->execute_scalar<int64_t>("SELECT value FROM test WHERE id = 1");
        REQUIRE(value.has_value());
        REQUIRE(*value == 42);
    }
    
    SECTION("returns nullopt for no result") {
        auto value = db->execute_scalar<int64_t>("SELECT value FROM test WHERE id = 999");
        REQUIRE_FALSE(value.has_value());
    }
}

TEST_CASE("SQLiteDatabase::execute_batch", "[storage][sqlite]") {
    TestDatabase test_db;
    auto db = test_db.db();
    
    db->execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)");
    
    std::vector<std::vector<nlohmann::json>> params_list = {
        {nlohmann::json("name1")},
        {nlohmann::json("name2")},
        {nlohmann::json("name3")}
    };
    
    auto result = db->execute_batch(
        "INSERT INTO test (name) VALUES (?)",
        params_list
    );
    
    REQUIRE(result.affected_rows == 3);
    
    auto count = db->execute_scalar<int64_t>("SELECT COUNT(*) as count FROM test");
    REQUIRE(count.has_value());
    REQUIRE(*count == 3);
}

// ============================================================================
// Transaction Tests
// ============================================================================

TEST_CASE("SQLiteDatabase::transaction", "[storage][transaction]") {
    TestDatabase test_db;
    auto db = test_db.db();
    
    db->execute("CREATE TABLE test (id INTEGER PRIMARY KEY, value INTEGER)");
    
    SECTION("commit transaction") {
        auto tx = db->begin_transaction();
        tx->execute("INSERT INTO test (value) VALUES (1)");
        tx->commit();
        
        auto count = db->execute_scalar<int64_t>("SELECT COUNT(*) as count FROM test");
        REQUIRE(count.has_value());
        REQUIRE(*count == 1);
    }
    
    SECTION("rollback transaction") {
        {
            auto tx = db->begin_transaction();
            tx->execute("INSERT INTO test (value) VALUES (1)");
            tx->rollback();
        }
        
        auto count = db->execute_scalar<int64_t>("SELECT COUNT(*) as count FROM test");
        REQUIRE(count.has_value());
        REQUIRE(*count == 0);
    }
    
    SECTION("automatic rollback on scope exit") {
        {
            auto tx = db->begin_transaction();
            tx->execute("INSERT INTO test (value) VALUES (1)");
            // Transaction should rollback automatically when tx goes out of scope
        }
        
        auto count = db->execute_scalar<int64_t>("SELECT COUNT(*) as count FROM test");
        REQUIRE(count.has_value());
        REQUIRE(*count == 0);
    }
}

TEST_CASE("TransactionGuard", "[storage][transaction]") {
    TestDatabase test_db;
    auto db = test_db.db();
    
    db->execute("CREATE TABLE test (id INTEGER PRIMARY KEY, value INTEGER)");
    
    SECTION("commit via guard") {
        {
            TransactionGuard guard(db->begin_transaction());
            guard->execute("INSERT INTO test (value) VALUES (1)");
            guard.commit();
        }
        
        auto count = db->execute_scalar<int64_t>("SELECT COUNT(*) as count FROM test");
        REQUIRE(count.has_value());
        REQUIRE(*count == 1);
    }
    
    SECTION("automatic rollback via guard") {
        {
            TransactionGuard guard(db->begin_transaction());
            guard->execute("INSERT INTO test (value) VALUES (1)");
            // Guard should rollback automatically
        }
        
        auto count = db->execute_scalar<int64_t>("SELECT COUNT(*) as count FROM test");
        REQUIRE(count.has_value());
        REQUIRE(*count == 0);
    }
    
    SECTION("explicit rollback via guard") {
        {
            TransactionGuard guard(db->begin_transaction());
            guard->execute("INSERT INTO test (value) VALUES (1)");
            guard.rollback();
        }
        
        auto count = db->execute_scalar<int64_t>("SELECT COUNT(*) as count FROM test");
        REQUIRE(count.has_value());
        REQUIRE(*count == 0);
    }
}

// ============================================================================
// Migration Tests
// ============================================================================

class TestMigration : public Migration {
public:
    TestMigration(std::string name, int version, std::string up_sql, std::string down_sql)
        : name_(std::move(name))
        , version_(version)
        , up_sql_(std::move(up_sql))
        , down_sql_(std::move(down_sql)) {}
    
    std::string name() const override { return name_; }
    std::string up() const override { return up_sql_; }
    std::string down() const override { return down_sql_; }
    int version() const override { return version_; }
    
private:
    std::string name_;
    int version_;
    std::string up_sql_;
    std::string down_sql_;
};

TEST_CASE("MigrationRunner::run", "[storage][migration]") {
    // Use a file-based database for each test to avoid conflicts
    const std::string db_path = "/tmp/test_migration_run.db";
    std::filesystem::remove(db_path);
    
    DatabaseConfig config;
    config.path = db_path;
    auto db = std::make_shared<SQLiteDatabase>(config);
    
    MigrationRunner runner(db);
    
    runner.add_migration(std::make_unique<TestMigration>(
        "001_create_users",
        1,
        "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)",
        "DROP TABLE users"
    ));
    
    runner.add_migration(std::make_unique<TestMigration>(
        "002_create_posts",
        2,
        "CREATE TABLE posts (id INTEGER PRIMARY KEY, user_id INTEGER, title TEXT)",
        "DROP TABLE posts"
    ));
    
    SECTION("run migrations") {
        runner.run();
        
        auto pending = runner.get_pending();
        REQUIRE(pending.empty());
        
        auto executed = runner.get_executed();
        REQUIRE(executed.size() == 2);
        REQUIRE(executed[0] == "001_create_users");
        REQUIRE(executed[1] == "002_create_posts");
    }
    
    SECTION("migrations are idempotent") {
        runner.run();
        runner.run();  // Run again
        
        auto executed = runner.get_executed();
        REQUIRE(executed.size() == 2);
    }
    
    SECTION("rollback migration") {
        runner.run();
        runner.rollback(1);
        
        auto executed = runner.get_executed();
        REQUIRE(executed.size() == 1);
    }
    
    // Cleanup
    std::filesystem::remove(db_path);
}

TEST_CASE("MigrationRunner::validate", "[storage][migration]") {
    // Create a fresh database for this test
    const std::string db_path = "/tmp/test_migration_validate.db";
    std::filesystem::remove(db_path);
    
    DatabaseConfig config;
    config.path = db_path;
    auto db = std::make_shared<SQLiteDatabase>(config);
    
    MigrationRunner runner(db);
    
    runner.add_migration(std::make_unique<TestMigration>(
        "001_create_test",
        1,
        "CREATE TABLE test (id INTEGER PRIMARY KEY)",
        "DROP TABLE test"
    ));
    
    SECTION("validate after run") {
        runner.run();
        REQUIRE(runner.validate());
    }
    
    // Cleanup
    std::filesystem::remove(db_path);
}

// ============================================================================
// Health Check Tests
// ============================================================================

TEST_CASE("SQLiteDatabase::health_check", "[storage][sqlite]") {
    TestDatabase test_db;
    REQUIRE(test_db.db()->health_check());
}

// ============================================================================
// Data Type Tests
// ============================================================================

TEST_CASE("SQLiteDatabase::data_types", "[storage][sqlite]") {
    TestDatabase test_db;
    auto db = test_db.db();
    
    db->execute(R"(
        CREATE TABLE types_test (
            id INTEGER PRIMARY KEY,
            text_val TEXT,
            int_val INTEGER,
            float_val REAL,
            bool_val INTEGER,
            null_val TEXT
        )
    )");
    
    SECTION("insert and retrieve basic types") {
        // Use raw SQL for string to avoid JSON serialization
        db->execute("INSERT INTO types_test (text_val, int_val, float_val, bool_val, null_val) VALUES ('hello', 42, 3.14, 1, NULL)");
        
        auto row = db->execute_one("SELECT * FROM types_test WHERE id = 1");
        REQUIRE(row.has_value());
        
        REQUIRE((*row)["text_val"] == "hello");
        REQUIRE((*row)["int_val"] == 42);
        REQUIRE((*row)["float_val"].get<double>() == Catch::Approx(3.14));
        REQUIRE((*row)["bool_val"] == 1);
        REQUIRE((*row)["null_val"].is_null());
    }
    
    SECTION("insert and retrieve with parameters") {
        db->execute(
            "INSERT INTO types_test (int_val, float_val, bool_val, null_val) VALUES (?, ?, ?, ?)",
            {
                nlohmann::json(42),
                nlohmann::json(3.14),
                nlohmann::json(true),
                nlohmann::json(nullptr)
            }
        );
        
        auto row = db->execute_one("SELECT * FROM types_test WHERE id = 1");
        REQUIRE(row.has_value());
        
        REQUIRE((*row)["int_val"] == 42);
        REQUIRE((*row)["float_val"].get<double>() == Catch::Approx(3.14));
        REQUIRE((*row)["bool_val"] == 1);
        REQUIRE((*row)["null_val"].is_null());
    }
}

TEST_CASE("SQLiteDatabase::json_data", "[storage][sqlite]") {
    TestDatabase test_db;
    auto db = test_db.db();
    
    db->execute("CREATE TABLE json_test (id INTEGER PRIMARY KEY, data TEXT)");
    
    SECTION("insert and retrieve JSON object") {
        nlohmann::json obj = {
            {"name", "test"},
            {"values", {1, 2, 3}},
            {"nested", {{"key", "value"}}}
        };
        
        db->execute(
            "INSERT INTO json_test (data) VALUES (?)",
            {obj}
        );
        
        auto row = db->execute_one("SELECT data FROM json_test WHERE id = 1");
        REQUIRE(row.has_value());
        
        // The stored value should be the JSON string
        std::string stored = (*row)["data"].get<std::string>();
        auto parsed = nlohmann::json::parse(stored);
        REQUIRE(parsed["name"] == "test");
    }
}
