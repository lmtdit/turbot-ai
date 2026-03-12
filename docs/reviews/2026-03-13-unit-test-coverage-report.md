# 单元测试覆盖率报告

> **报告日期**: 2026-03-13
> **工具链**: Apple Clang + llvm-cov (LLVM 16)
> **构建配置**: Debug + Coverage (`TURBOT_ENABLE_COVERAGE=ON`)
> **测试二进制**: `build/debug/tests/turbot-unit-tests`

---

## 测试执行摘要

| 指标               | 数值       |
| ------------------ | ---------- |
| 总 test cases      | 745        |
| 通过               | 738        |
| **失败**           | **7**      |
| 总 assertions      | 4329       |
| 通过 assertions    | 4315       |
| 失败 assertions    | 14         |
| **整体行覆盖率**   | **70.33%** |
| 整体函数覆盖率     | 88.03%     |
| 整体 Region 覆盖率 | 68.66%     |

---

## 测试失败详情（7 个 failed test cases）

所有失败集中在 `system_prompt_test.cpp` 和 `prompt_builder_test.cpp`，共 14 个 assertion 失败。

### 根因分析

测试在 `build/debug/tests/` 工作目录下运行，`system_prompt.cpp` 通过 `current_path() / "prompts"` 查找 prompt 文件。但 `copy_prompts` CMake target 将文件复制到 `build/debug/prompts/`（非 `build/debug/tests/prompts/`），导致文件未找到，使用内嵌 fallback 字符串，缺少测试断言的 section。

### 失败断言列表

| 文件                    | 行号        | 断言内容                                |
| ----------------------- | ----------- | --------------------------------------- |
| system_prompt_test.cpp  | 73          | `prompt.find("Editing constraints")`    |
| system_prompt_test.cpp  | 82          | `prompt.find("Tone and style")`         |
| system_prompt_test.cpp  | 83          | `prompt.find("Task Management")`        |
| system_prompt_test.cpp  | 91          | `prompt.find("Workflow")`               |
| system_prompt_test.cpp  | 124         | `prompt.find("Editing constraints")`    |
| system_prompt_test.cpp  | 129,134,139 | `prompt.find("Workflow")` (×3)          |
| system_prompt_test.cpp  | 144,159     | `prompt.find("Task Management")` (×2)   |
| system_prompt_test.cpp  | 166,171     | `prompt.find("Workflow")` (×2)          |
| system_prompt_test.cpp  | 315         | `prompt.find("Task Management")`        |
| prompt_builder_test.cpp | 307         | `result.system.find("Workflow")`        |
| prompt_builder_test.cpp | 315         | `result.system.find("Task Management")` |

---

## 各模块行覆盖率详情

### libs/core

| 文件                                 | 行覆盖率   | 达标(≥90%) |
| ------------------------------------ | ---------- | ---------- |
| `agent/agent.cpp`                    | 97.66%     | ✅         |
| `agent/agent_loader.cpp`             | **62.57%** | ❌         |
| `agent/builtin/build_agent.cpp`      | 90.00%     | ✅         |
| `agent/builtin/explore_agent.cpp`    | 90.32%     | ✅         |
| `agent/builtin/plan_agent.cpp`       | 85.71%     | ⚠️         |
| `common/logger.cpp`                  | 100.00%    | ✅         |
| `config/config.cpp`                  | 87.66%     | ⚠️         |
| `config/config_manager.cpp`          | **80.87%** | ❌         |
| `event/event_bus.cpp`                | 100.00%    | ✅         |
| `llm/llm.cpp`                        | **63.13%** | ❌         |
| `llm/message_builder.cpp`            | **83.43%** | ❌         |
| `llm/prompt_builder.cpp`             | 100.00%    | ✅         |
| `llm/provider_adapter.cpp`           | **40.45%** | ❌         |
| `llm/stream_event.cpp`               | 97.30%     | ✅         |
| `llm/system_prompt.cpp`              | **84.62%** | ❌         |
| `llm/tool_schema.cpp`                | **83.25%** | ❌         |
| `message/message.cpp`                | **87.70%** | ⚠️         |
| `message/part.cpp`                   | 95.60%     | ✅         |
| `permission/permission.cpp`          | 97.56%     | ✅         |
| `plugin/plugin.cpp`                  | 100.00%    | ✅         |
| `provider/impl/bailian_provider.cpp` | **19.95%** | ❌         |
| `provider/impl/iflow_provider.cpp`   | **15.84%** | ❌         |
| `provider/impl/kimi_provider.cpp`    | **15.84%** | ❌         |
| `provider/impl/openai_provider.cpp`  | **24.22%** | ❌         |
| `provider/impl/zhipu_provider.cpp`   | **21.93%** | ❌         |
| `provider/provider.cpp`              | 95.38%     | ✅         |
| `session/retry_manager.cpp`          | 97.18%     | ✅         |
| `session/session.cpp`                | 88.06%     | ⚠️         |
| `session/session_compaction.cpp`     | 90.58%     | ✅         |
| `session/session_loop.cpp`           | **67.81%** | ❌         |
| `session/session_state_machine.cpp`  | **88.24%** | ⚠️         |
| `session/usage_tracker.cpp`          | 97.22%     | ✅         |

### libs/network

| 文件              | 行覆盖率   | 达标(≥90%) |
| ----------------- | ---------- | ---------- |
| `http_client.cpp` | **83.15%** | ❌         |
| `url.cpp`         | **67.07%** | ❌         |

### libs/storage

| 文件                  | 行覆盖率   | 达标(≥90%) |
| --------------------- | ---------- | ---------- |
| `migration.cpp`       | 92.21%     | ✅         |
| `sqlite_database.cpp` | **77.45%** | ❌         |
| `transaction.cpp`     | 95.83%     | ✅         |

### libs/utils

| 文件               | 行覆盖率   | 达标(≥90%) |
| ------------------ | ---------- | ---------- |
| `crypto_utils.cpp` | **77.34%** | ❌         |
| `env_utils.cpp`    | **27.40%** | ❌         |
| `file_utils.cpp`   | **82.22%** | ❌         |
| `json_utils.cpp`   | **76.77%** | ❌         |
| `string_utils.cpp` | **85.59%** | ⚠️         |

---

## 覆盖率缺口优先级分析

### P0 — 影响整体覆盖率最大

| 文件                     | 当前    | 目标 | 缺口    | 预计可补充行数 |
| ------------------------ | ------- | ---- | ------- | -------------- |
| Provider impls (5 files) | ~15-24% | ≥85% | ~60-70% | ~1200 行       |
| `provider_adapter.cpp`   | 40.45%  | ≥85% | 44.55%  | ~80 行         |
| `env_utils.cpp`          | 27.40%  | ≥90% | 62.60%  | ~35 行         |
| `session_loop.cpp`       | 67.81%  | ≥80% | 12.19%  | ~56 行         |
| `llm.cpp`                | 63.13%  | ≥80% | 16.87%  | ~69 行         |
| `agent_loader.cpp`       | 62.57%  | ≥85% | 22.43%  | ~36 行         |
| `url.cpp`                | 67.07%  | ≥85% | 17.93%  | ~25 行         |

### P1 — 中等缺口

| 文件                  | 当前   | 目标 |
| --------------------- | ------ | ---- |
| `message_builder.cpp` | 83.43% | ≥90% |
| `tool_schema.cpp`     | 83.25% | ≥90% |
| `system_prompt.cpp`   | 84.62% | ≥90% |
| `config_manager.cpp`  | 80.87% | ≥85% |
| `sqlite_database.cpp` | 77.45% | ≥85% |
| `json_utils.cpp`      | 76.77% | ≥85% |
| `crypto_utils.cpp`    | 77.34% | ≥85% |

---

## 达标统计

| 类别                  | 统计       |
| --------------------- | ---------- |
| 总模块数              | ~40        |
| ✅ 已达标（≥90%）     | 16         |
| ⚠️ 接近达标（85-90%） | 6          |
| ❌ 未达标（<85%）     | 18         |
| **整体行覆盖率**      | **70.33%** |
| **目标**              | **≥85%**   |
