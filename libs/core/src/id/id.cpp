// id.cpp - Centralized ID generation implementation
//
// Mirrors: opencode/packages/opencode/src/id/id.ts (Identifier namespace)
//
// ID format: "<prefix>_<12-hex-timestamp><14-base62-random>"
//
// Timestamp encoding:
//   ascending:  raw  ms_timestamp * 0x1000 + per-ms-counter, big-endian 6 bytes → 12 hex
//   descending: ~(ms_timestamp * 0x1000 + per-ms-counter),  big-endian 6 bytes → 12 hex
//
// Random suffix: 14 chars from base62 alphabet using std::random_device

#include <turbot/core/id/id.hpp>
#include <chrono>
#include <mutex>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace turbot::core::id {

namespace {

// --- Monotonic counter (shared across ascending + descending calls) ---

std::mutex  g_id_mutex;
int64_t     g_last_ts{0};
int32_t     g_counter{0};

/// Returns {timestamp_ms, counter} under the global lock.
std::pair<int64_t, int32_t> next_tick(int64_t ts_override) {
    using namespace std::chrono;
    std::lock_guard<std::mutex> lock(g_id_mutex);

    int64_t ts = ts_override > 0
        ? ts_override
        : duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    if (ts != g_last_ts) {
        g_last_ts = ts;
        g_counter = 0;
    }
    ++g_counter;
    return {ts, g_counter};
}

// --- Base62 ---

static constexpr char kBase62[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
// kBase62 has 63 chars incl. NUL; valid indices are 0..61

std::string random_base62(int length) {
    // Use thread_local random_device — aligns with OpenCode's crypto.randomBytes
    thread_local std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 61);

    std::string s;
    s.reserve(static_cast<size_t>(length));
    for (int i = 0; i < length; ++i) {
        s += kBase62[dist(rd)];
    }
    return s;
}

// --- Core generator ---

std::string generate(const std::string& prefix, bool desc, int64_t ts_override) {
    auto [ts, cnt] = next_tick(ts_override);

    // Combine timestamp + counter
    uint64_t val = static_cast<uint64_t>(ts) * 0x1000ULL + static_cast<uint64_t>(cnt);
    if (desc) val = ~val;

    // Encode 6 bytes big-endian → 12 hex chars
    uint8_t bytes[6];
    for (int i = 0; i < 6; ++i) {
        bytes[i] = static_cast<uint8_t>((val >> (40 - 8 * i)) & 0xFFU);
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 6; ++i) {
        oss << std::setw(2) << static_cast<int>(bytes[i]);
    }

    // 14 random base62 chars (LENGTH = 26, prefix not counted)
    return prefix + "_" + oss.str() + random_base62(14);
}

} // anonymous namespace

// ---- Public API ----

std::string ascending(const std::string& prefix, int64_t ts_ms) {
    return generate(prefix, false, ts_ms);
}

std::string descending(const std::string& prefix, int64_t ts_ms) {
    return generate(prefix, true, ts_ms);
}

int64_t timestamp(const std::string& id) {
    // Find '_' separator
    auto sep = id.find('_');
    if (sep == std::string::npos) return 0;

    // Hex portion starts at sep+1, length 12
    if (id.size() < sep + 1 + 12) return 0;

    const std::string hex = id.substr(sep + 1, 12);
    try {
        uint64_t encoded = std::stoull(hex, nullptr, 16);
        // encoded = ts_ms * 0x1000 + counter
        return static_cast<int64_t>(encoded / 0x1000ULL);
    } catch (...) {
        return 0;
    }
}

} // namespace turbot::core::id
