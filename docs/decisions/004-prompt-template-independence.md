# ADR-004: Prompt 模板文件独立

| 属性     | 值          |
| -------- | ----------- |
| 状态     | ✅ 已批准   |
| 日期     | 2026-03-09  |
| 影响版本 | v2.0        |
| 作者     | Turbot Team |

## 背景

`system_prompt.cpp` 包含大量 prompt 长文本内容（约 300 行），导致：

1. **代码可读性差**: 混合 C++ 代码与长文本，难以维护
2. **修改困难**: 修改 prompt 需要重新编译 C++ 代码
3. **版本追踪不便**: prompt 变更淹没在代码提交中
4. **国际化障碍**: 无法方便地进行 prompt 本地化

## 决策

将 Prompt 模板从 C++ 代码中独立为 Markdown 文件存储。

### 文件结构

```
libs/core/prompts/
├── codex.md      # GPT-5 专用
├── beast.md      # GPT-4/o1/o3 模型
├── anthropic.md  # Claude 模型
├── openai.md     # OpenAI 兼容模型
├── gemini.md     # Gemini 模型
├── qwen.md       # Qwen 及国产模型
└── trinity.md    # Trinity 模型
```

### 加载机制

```cpp
std::string load_prompt_file(const std::string& name) {
    // 1. 检查缓存
    // 2. 从 TURBOT_PROMPTS_DIR 环境变量或默认路径加载
    // 3. 缓存结果并返回
}
```

### 关键设计

1. **环境变量支持**: `TURBOT_PROMPTS_DIR` 指定 prompts 目录路径
2. **内存缓存**: 避免重复文件 I/O
3. **Fallback 机制**: 文件加载失败时使用内嵌简短 prompt
4. **安装支持**: CMake 安装到 `share/turbot/prompts/`

## 替代方案

### 方案 A: 编译时嵌入 (已拒绝)

使用 `#include` 或 `cmake` 将文件内容嵌入二进制。

**拒绝理由**: 丧失运行时修改 prompt 的灵活性。

### 方案 B: 数据库存储 (已拒绝)

将 prompt 存储在 SQLite 数据库中。

**拒绝理由**: 过度设计，增加复杂度，prompt 不需要查询能力。

## 后果

### 正面

- 代码更清晰，`system_prompt.cpp` 减少 ~200 行
- Prompt 可独立修改，无需重新编译
- 便于版本追踪 prompt 变更
- 支持运行时自定义 prompt 目录

### 负面

- 需要确保 prompts 目录与可执行文件一起部署
- 文件加载失败时需要 fallback 机制

### 风险缓解

1. **部署问题**: CMake 自动安装 prompts 目录
2. **加载失败**: 内嵌 fallback prompt 确保基本功能

## 实施

- [x] 创建 `libs/core/prompts/` 目录
- [x] 创建 7 个 prompt 模板文件
- [x] 修改 `system_prompt.cpp` 添加文件加载逻辑
- [x] 更新 CMakeLists.txt 添加安装规则
- [x] 测试验证 (32/32 prompt 相关测试通过)
