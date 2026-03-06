# Turbot AI

A modern C++23 project using multi-package architecture with CMake and Conan.

## Project Structure

```
turbot-ai/
├── CMakeLists.txt              # Root CMake configuration
├── CMakePresets.json           # CMake presets for IDE support
├── conanfile.py                # Conan dependency management
│
├── libs/                       # Internal libraries
│   ├── core/                   # Core library (logging, version)
│   ├── utils/                  # Utility library (string, file)
│   └── network/                # Network library (HTTP, URL)
│
├── apps/                       # Applications
│   ├── turbot-cli/             # Command-line interface
│   └── turbot-server/          # Server application
│
├── tests/                      # Integration tests
├── cmake/                      # CMake modules
└── third_party/                # Third-party dependencies
```

## Requirements

- CMake 3.28+
- C++23 compatible compiler (GCC 14+, Clang 18+, MSVC 2022+)
- Conan 2.x
- Python 3.8+

## Quick Start

### 1. Install Dependencies with Conan

```bash
# Create Conan profile (if not exists)
conan profile detect --force

# Install dependencies
conan install . --output-folder=build/conan --build=missing
```

### 2. Build the Project

```bash
# Using CMake presets (recommended)
cmake --preset=debug-conan
cmake --build build/debug

# Or manually
cmake -B build/debug -S . \
    -DCMAKE_TOOLCHAIN_FILE=build/conan/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
```

### 3. Run Tests

```bash
cd build/debug && ctest --output-on-failure
```

### 4. Run Applications

```bash
# CLI
./build/debug/bin/turbot-cli --help
./build/debug/bin/turbot-cli test

# Server
./build/debug/bin/turbot-server
```

## Development

### CMake Presets

| Preset          | Description                     |
| --------------- | ------------------------------- |
| `default`       | Default release build           |
| `debug`         | Debug build with sanitizers     |
| `release`       | Optimized release build         |
| `debug-conan`   | Debug with Conan dependencies   |
| `release-conan` | Release with Conan dependencies |

### Code Formatting

```bash
# Format all source files
find . -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
```

### Static Analysis

```bash
# Run clang-tidy
clang-tidy -p build/debug libs/**/*.cpp apps/**/*.cpp
```

### Code Coverage

```bash
# Build with coverage
cmake --preset=debug
cmake --build build/debug

# Run tests and generate report
cd build/debug && ctest
cmake --build . --target coverage
```

## Libraries

### turbot-core

Core functionality including logging and version information.

```cpp
#include <turbot/core/logger.hpp>
#include <turbot/core/version.hpp>

TURBOT_LOG_INFO("Hello from Turbot {}!", core::Version::string());
```

### turbot-utils

Utility functions for string manipulation and file operations.

```cpp
#include <turbot/utils/string_utils.hpp>

auto trimmed = utils::trim("  hello  ");
auto parts = utils::split("a,b,c", ',');
```

### turbot-network

HTTP client and URL parsing.

```cpp
#include <turbot/network/http_client.hpp>

network::HttpClient client;
auto response = client.get("https://api.example.com/data");
```

## License

MIT License - See LICENSE file for details.
