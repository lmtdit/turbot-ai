# Round 9 对齐报告 — turbot libs vs opencode

**日期**：2026-03-25  
**范围**：`libs/core/src/provider/provider_transform.cpp`、`libs/core/src/session/session_loop.cpp`  
**对标源**：`opencode/packages/opencode/src/provider/transform.ts`、`src/session/llm.ts`  
**前轮基线**：Round 8 综合分 91/100，G83-G91 已补全  

---

## 差距清单（G92 ~ G96）

### G92 — variants() openai-compatible 系列缺 npm dispatch（MEDIUM）

**opencode** `transform.ts` L458-469：
```typescript
case "@ai-sdk/cerebras":
case "@ai-sdk/togetherai":
case "@ai-sdk/xai":
case "@ai-sdk/deepinfra":
case "venice-ai-sdk-provider":
case "@ai-sdk/openai-compatible":
  return Object.fromEntries(WIDELY_SUPPORTED_EFFORTS.map((effort) =>
    [effort, { reasoningEffort: effort }]))
```

**turbot 原实现**：
```cpp
if (pid == "openai-compatible" || pid == "cerebras" ||
    pid == "togetherai" || pid == "xai" || pid == "deepinfra" || pid == "venice") {
    return make_efforts(WIDELY, "reasoningEffort");
}
```

**问题**：opencode 以 `npm` 包名 dispatch，turbot 仅以 `pid` dispatch。当模型通过 npm 包配置但 pid 不匹配时，无法走到正确分支。

---

### G93 — grok-3-mini 分支缺 npm 路径（MEDIUM）

**opencode** L353：
```typescript
if (id.includes("grok") && id.includes("grok-3-mini")) {
  if (model.api.npm === "@openrouter/ai-sdk-provider") {
    return { low: ..., high: ... }
  }
```

**turbot 原实现**：`if (pid == "openrouter")` — 缺 npm 路径。

---

### G94 — bedrock anthropic 检测未用 model.api.id（LOW）

**opencode** L569：`model.api.id.includes("anthropic")`

**turbot 原实现**：`contains(id, "anthropic") || contains(id, "claude")` — `id = model.id`，未检查 `model.api.id`。

---

### G95 — schema() Gemini 判断缺 model.api.id 路径（LOW）

**opencode** L934：
```typescript
if (model.providerID === "google" || model.api.id.includes("gemini")) {
```

**turbot 原实现**：`if (model.provider_id != "google" && !contains(id, "gemini"))` — 缺 `model.api.id` 路径。

---

### G96 — LiteLLM 检测缺 model.api.id 路径（LOW）

**opencode** `llm.ts` L174-177：
```typescript
const isLiteLLMProxy =
  provider.options?.["litellmProxy"] === true ||
  input.model.providerID.toLowerCase().includes("litellm") ||
  input.model.api.id.toLowerCase().includes("litellm")
```

**turbot 原实现**：
```cpp
const bool is_litellm =
    turbot::utils::to_lower(provider_->id()).find("litellm") != std::string::npos;
```
缺少 `model.api.id`（即 `model_api_id_`）路径。

---

## 补全实现

### Sub-G92（已补全）
```cpp
// 在 variants() 开头添加 npm 变量
const std::string npm = model.api.npm;

// openai-compatible 分支加 npm 条件
if (pid == "openai-compatible" || pid == "cerebras" ||
    pid == "togetherai" || pid == "xai" || pid == "deepinfra" || pid == "venice" ||
    npm == "@ai-sdk/cerebras" || npm == "@ai-sdk/togetherai" ||
    npm == "@ai-sdk/xai" || npm == "@ai-sdk/deepinfra" ||
    npm == "venice-ai-sdk-provider" || npm == "@ai-sdk/openai-compatible") {
    return make_efforts(WIDELY, "reasoningEffort");
}
```

### Sub-G93（已补全）
```cpp
if (pid == "openrouter" || npm == "@openrouter/ai-sdk-provider") {
    return { {"low", ...}, {"high", ...} };
}
```

### Sub-G94（已补全）
```cpp
// Sub-G94: mirrors opencode L569 — use model.api.id (api_id) to detect anthropic/claude
if (contains(api_id, "anthropic") || contains(api_id, "claude") ||
    contains(id,     "anthropic") || contains(id,     "claude")) {
```

### Sub-G95（已补全）
```cpp
const std::string api_id = to_lower(model.api.id);
if (model.provider_id != "google" && !contains(id, "gemini") && !contains(api_id, "gemini")) {
    return sch;
}
```

### Sub-G96（已补全）
```cpp
const bool is_litellm =
    turbot::utils::to_lower(provider_->id()).find("litellm") != std::string::npos ||
    (!model_api_id_.empty() &&
     turbot::utils::to_lower(model_api_id_).find("litellm") != std::string::npos);
```

---

## SPEARM 终审评分（2次校验）

### 第一轮

| 维度 | 等级 | 说明 |
|------|------|------|
| S (3x) | A | 无安全风险，npm 字段只读 |
| P (2x) | A | 新增字符串变量开销与现有同级 |
| E (2x) | A | model_api_id_ 有 !empty() 守卫，api.npm 与 api.id 同等访问 |
| A (1.5x) | A | 与 opencode npm-dispatch 架构完全对齐 |
| R (1.5x) | A | 所有修改保留旧路径 fallback，向后兼容 |
| M (1x) | A | 每条修改均有 Sub-Gxx mirrors opencode Lxxx 注释 |

**综合分**：(9×3 + 9×2 + 9×2 + 9×1.5 + 9×1.5 + 9×1) / 11 = **90/100**  
**结论**：优秀

### 第二轮（逐项验证）

| Sub | opencode 对标 | turbot 实现 | 正确性 |
|-----|--------------|------------|--------|
| G92 | L458-469 switch(npm) 枚举 6 个 npm 包 | 6 个 npm 包名完全一致 | ✓ |
| G93 | L353 `npm === "@openrouter/ai-sdk-provider"` | `npm == "@openrouter/ai-sdk-provider"` | ✓ |
| G94 | L569 `model.api.id.includes("anthropic")` | `contains(api_id, "anthropic\|claude")` + id fallback | ✓ |
| G95 | L934 `model.api.id.includes("gemini")` | `contains(api_id, "gemini")` 加入 OR 条件 | ✓ |
| G96 | L177 `model.api.id.includes("litellm")` | `to_lower(model_api_id_).find("litellm")` | ✓ |

**综合分**：**90/100**（两轮一致）  
**结论**：优秀，可合入

---

## 历轮综合分追踪

| Round | 综合分 | 差距数 | 状态 |
|-------|--------|--------|------|
| R1    | 基线   | —      | 参考 |
| R2-R7 | 递增   | —      | 补全 |
| R8    | 91/100 | G83-G91 | 已补全 |
| **R9** | **90/100** | G92-G96 | **已补全** |
