---
name: cpp-code-review
description: C++ 代码审核专用技能。检查内存安全、RAII 模式、现代 C++ 最佳实践、并发安全等。当审核 C++ 代码时自动加载此技能。
---

# C++ 代码审核技能

针对 C++ 项目的专业代码审核规范和最佳实践检查。

## 触发条件

- 文件扩展名为 `.cpp`、`.hpp`、`.h`、`.cxx`、`.cc`
- 项目包含 `CMakeLists.txt` 或 `Makefile`
- 代码中出现 C++ 特定语法（命名空间、模板、智能指针等）

## 审核清单

### Critical - 必须修复

#### 内存安全

- [ ] **内存泄漏**: 检查所有 `new` 是否有对应的 `delete`
- [ ] **双重释放**: 同一指针是否被多次释放
- [ ] **悬空指针**: 指针是否在释放后继续使用
- [ ] **缓冲区溢出**: 数组访问是否越界
- [ ] **未初始化变量**: 变量使用前是否已初始化

```cpp
// 错误: 内存泄漏
void bad() {
    int* p = new int(42);
    // 忘记 delete
}

// 正确: 使用智能指针
void good() {
    auto p = std::make_unique<int>(42);
}
```

#### 资源管理 (RAII)

- [ ] **RAII 模式**: 资源是否封装在对象中管理
- [ ] **异常安全**: 异常发生时资源是否正确释放
- [ ] **所有权清晰**: 指针所有权是否明确

```cpp
// 错误: 非异常安全
void bad(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    process(f);  // 可能抛异常，f 泄漏
    fclose(f);
}

// 正确: RAII 封装
void good(const std::string& path) {
    std::ifstream f(path);
    process(f);  // 异常安全，析构自动关闭
}
```

#### 并发安全

- [ ] **数据竞争**: 共享数据是否正确加锁
- [ ] **死锁风险**: 锁的获取顺序是否一致
- [ ] **原子操作**: 是否正确使用 `std::atomic`

```cpp
// 错误: 数据竞争
int counter = 0;
void bad() {
    counter++;  // 非原子操作
}

// 正确: 原子操作
std::atomic<int> counter{0};
void good() {
    counter.fetch_add(1, std::memory_order_relaxed);
}
```

### Warning - 应该修复

#### 现代 C++ 特性

- [ ] **智能指针**: 是否使用 `unique_ptr`/`shared_ptr` 替代裸指针
- [ ] **范围 for**: 是否使用 `for (auto& x : container)`
- [ ] **auto 关键字**: 是否合理使用 `auto`
- [ ] **移动语义**: 是否正确实现移动构造/赋值
- [ ] **constexpr**: 编译时常量是否使用 `constexpr`

```cpp
// 避免: 裸指针
Widget* w = new Widget();

// 推荐: 智能指针
auto w = std::make_unique<Widget>();
```

#### STL 使用

- [ ] **容器选择**: 是否选择了合适的容器
- [ ] **算法使用**: 是否优先使用 STL 算法而非手写循环
- [ ] **迭代器有效性**: 是否检查迭代器失效问题

#### 性能问题

- [ ] **不必要的拷贝**: 大对象是否使用引用传递
- [ ] **字符串拼接**: 是否使用 `std::string::reserve` 或 `std::ostringstream`
- [ ] **虚函数开销**: 是否在热路径滥用虚函数

```cpp
// 避免: 不必要的拷贝
void process(std::string s);  // 拷贝

// 推荐: const 引用
void process(const std::string& s);  // 无拷贝
```

### Suggestion - 建议改进

#### 代码风格

- [ ] **命名规范**: 是否遵循项目命名约定
- [ ] **const 正确性**: 所有不修改的地方是否标记 `const`
- [ ] **noexcept**: 不抛异常的函数是否标记
- [ ] **[[nodiscard]]**: 返回值是否应该被检查

```cpp
// 推荐: 完整的函数声明
[[nodiscard]] size_t size() const noexcept {
    return data_.size();
}
```

#### 可维护性

- [ ] **头文件依赖**: 是否最小化头文件包含
- [ ] **前向声明**: 能否用前向声明替代包含
- [ ] **PIMPL 模式**: 实现细节是否隐藏

## 常见问题模式

### 1. 迭代器失效

```cpp
// 错误: 迭代器失效
for (auto it = vec.begin(); it != vec.end(); ++it) {
    if (condition(*it)) {
        vec.erase(it);  // it 失效
    }
}

// 正确: 使用返回值
for (auto it = vec.begin(); it != vec.end(); ) {
    if (condition(*it)) {
        it = vec.erase(it);
    } else {
        ++it;
    }
}
```

### 2. 未定义行为

```cpp
// 错误: 有符号整数溢出 (UB)
int sum = 0;
for (int i = 0; i < n; ++i) {
    sum += large_values[i];  // 可能溢出
}

// 正确: 使用无符号或检查
size_t sum = 0;
for (size_t i = 0; i < n; ++i) {
    if (sum > std::numeric_limits<size_t>::max() - large_values[i]) {
        throw std::overflow_error("Integer overflow");
    }
    sum += large_values[i];
}
```

### 3. 字符串生命周期

```cpp
// 错误: 返回局部变量的引用
const std::string& bad() {
    std::string s = "hello";
    return s;  // 返回局部变量引用
}

// 正确: 返回值
std::string good() {
    return "hello";  // RVO 优化
}
```

## 编译器警告建议

推荐启用以下编译器警告：

```cmake
# CMake 配置
target_compile_options(${TARGET} PRIVATE
    -Wall -Wextra -Wpedantic
    -Werror=return-type
    -Werror=non-virtual-dtor
    -Werror=overloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Wold-style-cast
    -Wnull-dereference
    -Wdouble-promotion
)
```

## 工具推荐

- **静态分析**: clang-tidy, cppcheck
- **内存检测**: AddressSanitizer, Valgrind
- **代码格式**: clang-format

## 架构审核规范

### 公共模块识别

审核 C++ 代码时，必须检查纯函数是否存在重复实现：

**目录规范 (turbot-ai)**:

| 功能类型 | 头文件位置                                   | 源文件位置                   |
| -------- | -------------------------------------------- | ---------------------------- |
| 通用工具 | `libs/utils/include/turbot/utils/`           | `libs/utils/src/`            |
| 核心功能 | `libs/core/include/turbot/core/<submodule>/` | `libs/core/src/<submodule>/` |
| 网络相关 | `libs/network/include/turbot/network/`       | `libs/network/src/`          |
| 存储相关 | `libs/storage/include/turbot/storage/`       | `libs/storage/src/`          |

**检测流程**:

1. **标记纯函数**: 识别无副作用、仅依赖输入的函数

   ```cpp
   // 纯函数示例
   std::string trim(const std::string& s);  // ✓ 纯函数
   void log(const std::string& msg);        // ✗ 有副作用
   ```

2. **搜索重复实现**: 使用 `grep` 搜索相似功能

   ```bash
   # 搜索相似函数名
   grep -r "trim\|strip\|lstrip\|rstrip" --include="*.cpp" --include="*.hpp"
   ```

3. **提升标准**: 重复 >= 2 次时建议提升

**输出示例**:

```markdown
### 架构问题 - 重复代码检测

| 函数     | 重复位置                                      | 目标位置                                           | 理由                          |
| -------- | --------------------------------------------- | -------------------------------------------------- | ----------------------------- |
| `trim()` | `apps/cli/utils.cpp`, `apps/server/utils.cpp` | `libs/utils/include/turbot/utils/string_utils.hpp` | 重复 2 次，属于字符串处理工具 |
```

### C++ 模块组织最佳实践

**头文件规范**:

```cpp
// libs/utils/include/turbot/utils/string_utils.hpp
#pragma once

#include <string>

namespace turbot::utils {

// 纯函数声明
[[nodiscard]] std::string trim(const std::string& s) noexcept;
[[nodiscard]] std::string to_lower(std::string s) noexcept;

}  // namespace turbot::utils
```

**实现文件规范**:

```cpp
// libs/utils/src/string_utils.cpp
#include "turbot/utils/string_utils.hpp"

namespace turbot::utils {

std::string trim(const std::string& s) noexcept {
    // 实现...
}

}  // namespace turbot::utils
```
