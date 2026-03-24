#include <turbot/core/lsp/manager.hpp>
#include <turbot/core/lsp/builtin_servers.hpp>
#include <turbot/core/event/event_bus.hpp>
#include <turbot/core/common/logger.hpp>

#include <filesystem>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace turbot::core::lsp {

LSPManager& LSPManager::instance() noexcept {
    static LSPManager inst;
    return inst;
}

// ─── initialize ───────────────────────────────────────────────────────────────

void LSPManager::initialize(const std::string& workspace_root) {
    std::unique_lock<std::shared_mutex> lk(mutex_);
    workspace_root_ = workspace_root;
    servers_.clear();
    clients_.clear();
    broken_.clear();
    spawning_.clear();
    lk.unlock();

    // Register builtin servers
    register_server(make_clangd_server(workspace_root));
    register_server(make_pyright_server(workspace_root));
    register_server(make_gopls_server(workspace_root));
    register_server(make_deno_server(workspace_root));
    register_server(make_typescript_server(workspace_root));
    register_server(make_vue_server(workspace_root));
    register_server(make_eslint_server(workspace_root));
    register_server(make_biome_server(workspace_root));
    register_server(make_rust_analyzer_server(workspace_root));
    register_server(make_svelte_server(workspace_root));
    register_server(make_astro_server(workspace_root));
    
    // Register additional LSP servers (v4.2 feature alignment)
    register_server(make_bash_server(workspace_root));
    register_server(make_java_server(workspace_root));
    register_server(make_kotlin_server(workspace_root));
    register_server(make_csharp_server(workspace_root));
    register_server(make_clojure_server(workspace_root));
    register_server(make_dart_server(workspace_root));
    register_server(make_elixir_server(workspace_root));
    register_server(make_erlang_server(workspace_root));
    register_server(make_haskell_server(workspace_root));
    register_server(make_lua_server(workspace_root));
    register_server(make_nix_server(workspace_root));
    register_server(make_ocaml_server(workspace_root));
    register_server(make_php_server(workspace_root));
    register_server(make_ruby_server(workspace_root));
    register_server(make_scala_server(workspace_root));
    register_server(make_swift_server(workspace_root));
    register_server(make_terraform_server(workspace_root));
    register_server(make_zig_server(workspace_root));
}

// ─── register_server / disable_server ─────────────────────────────────────────

void LSPManager::register_server(LSPServerInfo server) {
    std::unique_lock<std::shared_mutex> lk(mutex_);
    servers_[server.id] = std::move(server);
}

void LSPManager::disable_server(const std::string& id) {
    std::unique_lock<std::shared_mutex> lk(mutex_);
    servers_.erase(id);
}

// ─── get_clients ──────────────────────────────────────────────────────────────

std::future<std::vector<LSPClient*>> LSPManager::get_clients(const std::string& file) {
    return std::async(std::launch::async, [this, file]() -> std::vector<LSPClient*> {
        const std::string ext = std::filesystem::path(file).extension().string();
        std::vector<LSPClient*> result;

        // Collect matching servers under shared lock
        std::vector<std::pair<std::string, LSPServerInfo>> matching;
        {
            std::shared_lock<std::shared_mutex> lk(mutex_);
            for (const auto& [sid, server] : servers_) {
                // Extension filter
                if (!server.extensions.empty()) {
                    bool match = false;
                    for (const auto& e : server.extensions) {
                        if (e == ext) { match = true; break; }
                    }
                    if (!match) continue;
                }
                matching.emplace_back(sid, server);
            }
        }

        for (const auto& [sid, server] : matching) {
            // Get project root for this file
            const auto root_opt = server.root ? server.root(file) : std::optional<std::string>{};
            if (!root_opt) continue;
            const std::string root = *root_opt;
            const std::string key  = root + sid;  // OpenCode: no separator between root and id

            std::shared_future<LSPClient*> fut;
            {
                std::unique_lock<std::shared_mutex> lk(mutex_);

                // Skip broken servers
                if (broken_.count(key)) continue;

                // Check if already connected
                for (const auto& c : clients_) {
                    if (c && c->root() == root && c->server_id() == sid) {
                        result.push_back(c.get());
                        goto next_server;
                    }
                }

                // Check in-flight spawn
                auto it = spawning_.find(key);
                if (it != spawning_.end()) {
                    fut = it->second;
                } else {
                    // Start new spawn (capture by value for thread safety)
                    const std::string cap_sid  = sid;
                    const std::string cap_root = root;
                    const std::string cap_key  = key;
                    LSPServerInfo cap_server   = server;

                    std::promise<LSPClient*> prom;
                    fut = prom.get_future().share();
                    spawning_[key] = fut;
                    lk.unlock();

                    // Spawn in background
                    std::thread([this, cap_sid, cap_root, cap_key,
                                 cap_server = std::move(cap_server),
                                 prom = std::move(prom)]() mutable {
                        LSPClient* client_ptr = nullptr;
                        try {
                            // Spawn subprocess
                            auto handle_opt = cap_server.spawn
                                ? cap_server.spawn(cap_root)
                                : std::optional<ServerHandle>{};
                            if (!handle_opt) {
                                TURBOT_LOG_INFO("LSP server '{}' not available, marking broken", cap_sid);
                                std::unique_lock<std::shared_mutex> lk2(mutex_);
                                broken_.insert(cap_key);
                            } else {
                                TURBOT_LOG_INFO("LSP: spawned '{}' for root={}", cap_sid, cap_root);
                                // Initialize client
                                auto client = LSPClient::create(cap_sid, *handle_opt, cap_root);
                                if (!client) {
                                    TURBOT_LOG_ERROR("LSP: create() failed for '{}'", cap_sid);
                                    std::unique_lock<std::shared_mutex> lk2(mutex_);
                                    broken_.insert(cap_key);
                                } else {
                                    client_ptr = client.get();
                                    // Check for duplicate (race condition guard)
                                    std::unique_lock<std::shared_mutex> lk2(mutex_);
                                    bool dup = false;
                                    for (const auto& c : clients_) {
                                        if (c && c->root() == cap_root && c->server_id() == cap_sid) {
                                            dup = true;
                                            client_ptr = c.get();
                                            break;
                                        }
                                    }
                                    if (!dup) {
                                        clients_.push_back(std::move(client));
                                        // Publish lsp.updated event
                                        lk2.unlock();
                                        turbot::core::EventBus::instance().publish<nlohmann::json>(
                                            "lsp.updated", nlohmann::json::object());
                                    }
                                }
                            }
                        } catch (const std::exception& ex) {
                            TURBOT_LOG_ERROR("LSP: spawn exception for '{}': {}", cap_sid, ex.what());
                            std::unique_lock<std::shared_mutex> lk2(mutex_);
                            broken_.insert(cap_key);
                        }
                        // Remove from spawning_ and fulfill promise
                        {
                            std::unique_lock<std::shared_mutex> lk2(mutex_);
                            spawning_.erase(cap_key);
                        }
                        prom.set_value(client_ptr);
                    }).detach();

                    lk = std::unique_lock<std::shared_mutex>(mutex_);
                    fut = spawning_.count(key) ? spawning_[key] : fut;
                }
            }

            // Wait for in-flight spawn (outside lock)
            {
                LSPClient* ptr = nullptr;
                try { ptr = fut.get(); } catch (const std::exception& e) {
                    TURBOT_LOG_DEBUG("LSP spawn failed: {}", e.what());
                } catch (...) {
                    TURBOT_LOG_DEBUG("LSP spawn failed with unknown error");
                }
                if (ptr) result.push_back(ptr);
            }

        next_server:;
        }

        return result;
    });
}

// ─── has_clients ──────────────────────────────────────────────────────────────

bool LSPManager::has_clients(const std::string& file) {
    const std::string ext = std::filesystem::path(file).extension().string();
    std::shared_lock<std::shared_mutex> lk(mutex_);

    for (const auto& [sid, server] : servers_) {
        if (!server.extensions.empty()) {
            bool match = false;
            for (const auto& e : server.extensions) {
                if (e == ext) { match = true; break; }
            }
            if (!match) continue;
        }
        // Check root without triggering spawn
        const auto root_opt = server.root ? server.root(file) : std::optional<std::string>{};
        if (!root_opt) continue;
        const std::string key = *root_opt + sid;
        if (broken_.count(key)) continue;
        return true;
    }
    return false;
}

// ─── touch_file ───────────────────────────────────────────────────────────────

void LSPManager::touch_file(const std::string& file, bool wait_for_diagnostics) {
    TURBOT_LOG_INFO("LSPManager: touching file={}", file);
    try {
        auto clients = get_clients(file).get();
        for (auto* client : clients) {
            if (!client) continue;
            if (wait_for_diagnostics) {
                // Aligned with OpenCode touchFile: register wait BEFORE notify_open
                auto wait_fut = client->wait_for_diagnostics(file);
                client->notify_open(file);
                try { wait_fut.get(); } catch (const std::exception& e) {
                    TURBOT_LOG_DEBUG("LSP wait_for_diagnostics failed: {}", e.what());
                } catch (...) {
                    TURBOT_LOG_DEBUG("LSP wait_for_diagnostics failed with unknown error");
                }
            } else {
                client->notify_open(file);
            }
        }
    } catch (const std::exception& ex) {
        TURBOT_LOG_ERROR("LSPManager: touch_file error: {}", ex.what());
    }
}

// ─── diagnostics ──────────────────────────────────────────────────────────────

std::unordered_map<std::string, std::vector<Diagnostic>> LSPManager::diagnostics() const {
    std::unordered_map<std::string, std::vector<Diagnostic>> merged;
    std::shared_lock<std::shared_mutex> lk(mutex_);
    for (const auto& client : clients_) {
        if (!client) continue;
        for (auto& [path, diags] : client->diagnostics()) {
            auto& arr = merged[path];
            arr.insert(arr.end(), diags.begin(), diags.end());
        }
    }
    return merged;
}

// ─── LSP broadcast helpers ────────────────────────────────────────────────────

std::future<std::optional<Hover>> LSPManager::hover(const std::string& file, Position pos) {
    return std::async(std::launch::async, [this, file, pos]() -> std::optional<Hover> {
        try {
            auto clients = get_clients(file).get();
            for (auto* c : clients) {
                if (!c) continue;
                auto uri = path_to_uri(file);
                auto result = c->hover(uri, pos).get();
                if (result) return result;
            }
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("LSP hover failed: {}", e.what());
        } catch (...) {
            TURBOT_LOG_DEBUG("LSP hover failed with unknown error");
        }
        return std::nullopt;
    });
}

std::future<std::vector<Location>> LSPManager::definition(const std::string& file, Position pos) {
    return std::async(std::launch::async, [this, file, pos]() -> std::vector<Location> {
        try {
            auto clients = get_clients(file).get();
            std::vector<Location> out;
            for (auto* c : clients) {
                if (!c) continue;
                auto locs = c->definition(path_to_uri(file), pos).get();
                out.insert(out.end(), locs.begin(), locs.end());
            }
            return out;
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("LSP definition failed: {}", e.what());
            return {};
        } catch (...) {
            TURBOT_LOG_DEBUG("LSP definition failed with unknown error");
            return {};
        }
    });
}

std::future<std::vector<Location>> LSPManager::references(const std::string& file, Position pos) {
    return std::async(std::launch::async, [this, file, pos]() -> std::vector<Location> {
        try {
            auto clients = get_clients(file).get();
            std::vector<Location> out;
            for (auto* c : clients) {
                if (!c) continue;
                auto locs = c->references(path_to_uri(file), pos).get();
                out.insert(out.end(), locs.begin(), locs.end());
            }
            return out;
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("LSP references failed: {}", e.what());
            return {};
        } catch (...) {
            TURBOT_LOG_DEBUG("LSP references failed with unknown error");
            return {};
        }
    });
}

std::future<std::vector<Location>> LSPManager::implementation(const std::string& file, Position pos) {
    return std::async(std::launch::async, [this, file, pos]() -> std::vector<Location> {
        try {
            auto clients = get_clients(file).get();
            std::vector<Location> out;
            for (auto* c : clients) {
                if (!c) continue;
                auto locs = c->implementation(path_to_uri(file), pos).get();
                out.insert(out.end(), locs.begin(), locs.end());
            }
            return out;
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("LSP implementation failed: {}", e.what());
            return {};
        } catch (...) {
            TURBOT_LOG_DEBUG("LSP implementation failed with unknown error");
            return {};
        }
    });
}

std::future<std::vector<Symbol>> LSPManager::workspace_symbol(const std::string& query) {
    return std::async(std::launch::async, [this, query]() -> std::vector<Symbol> {
        try {
            // Collect client pointers under lock, then release before blocking calls
            std::vector<LSPClient*> clients;
            {
                std::shared_lock<std::shared_mutex> lk(mutex_);
                for (const auto& c : clients_) {
                    if (c) clients.push_back(c.get());
                }
            }
            std::vector<Symbol> out;
            for (auto* c : clients) {
                auto syms = c->workspace_symbol(query).get();
                out.insert(out.end(), syms.begin(), syms.end());
            }
            return out;
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("LSP workspace_symbol failed: {}", e.what());
            return {};
        } catch (...) {
            TURBOT_LOG_DEBUG("LSP workspace_symbol failed with unknown error");
            return {};
        }
    });
}

std::future<std::vector<nlohmann::json>> LSPManager::document_symbol(const std::string& uri) {
    return std::async(std::launch::async, [this, uri]() -> std::vector<nlohmann::json> {
        try {
            const std::string file = uri_to_path(uri);
            auto clients = get_clients(file).get();
            std::vector<nlohmann::json> out;
            for (auto* c : clients) {
                if (!c) continue;
                auto syms = c->document_symbol(uri).get();
                for (const auto& s : syms) out.push_back(s.to_json());
            }
            return out;
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("LSP document_symbol failed: {}", e.what());
            return {};
        } catch (...) {
            TURBOT_LOG_DEBUG("LSP document_symbol failed with unknown error");
            return {};
        }
    });
}

// ─── status ───────────────────────────────────────────────────────────────────

std::vector<nlohmann::json> LSPManager::status() const {
    std::shared_lock<std::shared_mutex> lk(mutex_);
    std::vector<nlohmann::json> result;
    for (const auto& client : clients_) {
        if (!client) continue;
        // Compute relative root
        std::string rel_root = client->root();
        if (!workspace_root_.empty() && rel_root.rfind(workspace_root_, 0) == 0) {
            rel_root = rel_root.substr(workspace_root_.size());
            if (!rel_root.empty() && rel_root[0] == '/') rel_root = rel_root.substr(1);
        }
        result.push_back({
            {"id",     client->server_id()},
            {"name",   client->server_id()},
            {"root",   rel_root},
            {"status", "connected"},
        });
    }
    return result;
}

// ─── shutdown ─────────────────────────────────────────────────────────────────

void LSPManager::shutdown() {
    std::vector<std::unique_ptr<LSPClient>> to_shutdown;
    {
        std::unique_lock<std::shared_mutex> lk(mutex_);
        to_shutdown = std::move(clients_);
        clients_.clear();
        spawning_.clear();
        broken_.clear();
        servers_.clear();
    }
    for (auto& client : to_shutdown) {
        if (client) {
            try { client->shutdown(); } catch (const std::exception& e) {
                TURBOT_LOG_DEBUG("LSP client shutdown failed: {}", e.what());
            } catch (...) {
                TURBOT_LOG_DEBUG("LSP client shutdown failed with unknown error");
            }
        }
    }
}

}  // namespace turbot::core::lsp
