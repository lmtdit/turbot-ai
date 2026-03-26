# Round 10 对齐报告 — turbot libs vs opencode

**日期**：2026-03-25  
**范围**：`libs/core/src/provider/provider_transform.cpp`  
**对标源**：`opencode/packages/opencode/src/provider/transform.ts`  
**前轮基线**：Round 9 综合分 90/100，G92-G96 已补全  

---

## 差距清单（G97 ~ G103）

### G97 — variants() openrouter 主分支缺 npm dispatch（MEDIUM）

**opencode** L366-369：`switch(model.api.npm)` 中 `case "@openrouter/ai-sdk-provider":`

**turbot**：只有 `pid == "openrouter"`，grok-3-mini 的 npm 条件已补（G93），但 openrouter 主分支没有 npm 路径。

**修复**：`pid == "openrouter" || npm == "@openrouter/ai-sdk-provider"`

---

### G98 — variants() 所有主分支缺 npm dispatch fallback（MEDIUM）

**opencode** L366：`switch(model.api.npm)` 全量枚举：  
`@ai-sdk/github-copilot` / `@ai-sdk/azure` / `@ai-sdk/openai` / `@ai-sdk/anthropic` /  
`@ai-sdk/google-vertex/anthropic` / `@ai-sdk/amazon-bedrock` / `@ai-sdk/google` /  
`@ai-sdk/google-vertex` / `@ai-sdk/mistral` / `@ai-sdk/cohere` / `@ai-sdk/groq` /  
`@ai-sdk/perplexity` / `@ai-sdk/gateway` / `@jerome-benoit/sap-ai-provider-v2`

**turbot**：全部用 pid dispatch，当 npm 存在但 pid 不匹配时会漏分支。

**修复**：所有主分支加对应 npm 包名 OR 条件（13 个提供商全覆盖）。

---

### G99 — message() apply_caching 缺 npm="@ai-sdk/anthropic" 条件（LOW）

**opencode** L261：`model.api.npm === "@ai-sdk/anthropic"` 是触发 apply_caching 的条件之一；  
`!is_gateway` 使用 `model.api.npm !== "@ai-sdk/gateway"`。

**turbot**：缺 npm 条件；is_gateway 只检查 pid，未检查 api.npm。

**修复**：加 `model.api.npm == "@ai-sdk/anthropic"`；is_gateway 加 `|| model.api.npm == "@ai-sdk/gateway"`。

---

### G103 — message() apply_caching 缺 model.api.id 检测（LOW）

**opencode** L257-258：`model.api.id.includes("anthropic") || model.api.id.includes("claude")`

**turbot**：只检查 `model.id`（id_lower），缺 model.api.id 路径。

**修复**：加 `msg_api_id = to_lower(model.api.id)`，检查 `contains(msg_api_id, "anthropic/claude")`。

---

### G100 — message() providerOptions remap 用 pid 而非 npm 查 sdkKey（MEDIUM）

**opencode** L268-269：
```typescript
const key = sdkKey(model.api.npm)
if (key && key !== model.providerID && model.api.npm !== "@ai-sdk/azure")
```

**turbot**：`sdk_key(model.provider_id)`，用 pid 而非 npm。

**修复**：`const auto key_npm = sdk_key(model.api.npm); const auto key = key_npm ? key_npm : sdk_key(pid);`  
is_azure 也改为 `npm == "@ai-sdk/azure" || pid == "azure"`。

---

### G101 — providerOptions() 用 pid 而非 npm 查 sdkKey（MEDIUM）

**opencode** L906：`const key = sdkKey(model.api.npm) ?? model.providerID`

**turbot**：`sdk_key(model.provider_id)` — 用 pid。

**修复**：npm-first + pid-fallback 双保险。

---

### G102 — schema() sanitize() hasSchemaIntent 检查不完整（LOW）

**opencode** L937-958：`hasSchemaIntent` 检查 15 个键（type/properties/items/prefixItems/enum/const/$ref/additionalProperties/patternProperties/required/not/if/then/else + anyOf/oneOf/allOf）

**turbot**：只检查 `anyOf/oneOf`，缺少 `allOf/$ref/type/properties/items/prefixItems/enum/const/additionalProperties/patternProperties/required/not/if/then/else`。

**修复**：实现 `has_schema_intent` lambda，覆盖全部 15 个键。

---

## 补全实现摘要

### Sub-G97/G98（variants() 各主分支加 npm dispatch）
```cpp
if (pid == "openrouter" || npm == "@openrouter/ai-sdk-provider") { ... }
if (pid == "github-copilot" || pid == "copilot" || npm == "@ai-sdk/github-copilot") { ... }
if (pid == "azure" || npm == "@ai-sdk/azure") { ... }
if (pid == "openai" || npm == "@ai-sdk/openai") { ... }
if (pid == "anthropic" || npm == "@ai-sdk/anthropic" || npm == "@ai-sdk/google-vertex/anthropic") { ... }
if (pid == "amazon-bedrock" || contains(pid, "bedrock") || npm == "@ai-sdk/amazon-bedrock") { ... }
if (pid == "google" || ... || npm == "@ai-sdk/google" || npm == "@ai-sdk/google-vertex") { ... }
if (pid == "mistral" || pid == "cohere" || pid == "perplexity" ||
    npm == "@ai-sdk/mistral" || npm == "@ai-sdk/cohere" || npm == "@ai-sdk/perplexity") { ... }
if (pid == "groq" || npm == "@ai-sdk/groq") { ... }
if (pid == "gateway" || npm == "@ai-sdk/gateway") { ... }
if (pid == "sap" || npm == "@jerome-benoit/sap-ai-provider-v2") { ... }
```

### Sub-G99/G103（message() apply_caching）
```cpp
const std::string msg_api_id = to_lower(model.api.id);
const bool is_gateway = model.provider_id == "gateway" || model.api.npm == "@ai-sdk/gateway";
if (!is_gateway &&
    (model.provider_id == "anthropic" ||
     contains(msg_api_id, "anthropic") || contains(msg_api_id, "claude") ||
     contains(id_lower,   "anthropic") || contains(id_lower,   "claude") ||
     model.api.npm == "@ai-sdk/anthropic")) { apply_caching(...) }
```

### Sub-G100/G101（sdkKey 用 npm 优先）
```cpp
// message(): 
const auto key_npm = sdk_key(model.api.npm);
const auto key = key_npm ? key_npm : sdk_key(model.provider_id);

// providerOptions():
const std::string key = sdk_key(model.api.npm)
    .value_or(sdk_key(model.provider_id).value_or(model.provider_id));
```

### Sub-G102（schema() hasSchemaIntent 完整实现）
```cpp
auto has_schema_intent = [](const nlohmann::json& node) -> bool {
    if (!node.is_object()) return false;
    if (node.contains("anyOf") || node.contains("oneOf") || node.contains("allOf")) return true;
    static const std::vector<std::string> schema_keys = {
        "type","properties","items","prefixItems","enum","const","$ref",
        "additionalProperties","patternProperties","required","not","if","then","else"
    };
    for (const auto& k : schema_keys) if (node.contains(k)) return true;
    return false;
};
```

---

## SPEARM 终审评分（2次校验）

### 第一轮

| 维度 | 等级 | 说明 |
|------|------|------|
| S (3x) | A | 无安全风险 |
| P (2x) | A- | hasSchemaIntent static vector 微小开销，可接受 |
| E (2x) | A | npm-first + pid-fallback 双保险 |
| A (1.5x) | A | 完整对齐 opencode npm-dispatch 架构 |
| R (1.5x) | A | 所有修改保留 pid 路径，向后兼容 |
| M (1x) | A | 注释完整，可追溯 |

**综合分**：(9×3 + 8.5×2 + 9×2 + 9×1.5 + 9×1.5 + 9×1) / 11 ≈ **89/100**  
**结论**：优秀

### 第二轮（逐项验证）

| Sub | opencode 对标 | turbot 实现 | 正确性 |
|-----|-------------|-----------|--------|
| G97 | L366 `case "@openrouter/ai-sdk-provider":` | `\|\| npm == "@openrouter/ai-sdk-provider"` | ✓ |
| G98 | L430-711 所有 npm case | 11 个提供商全覆盖 | ✓ |
| G99 | L261 npm=="@ai-sdk/anthropic" | 加入 OR 条件 | ✓ |
| G103 | L257-258 api.id includes | msg_api_id 使用 model.api.id | ✓ |
| G100 | L268 sdkKey(model.api.npm) | npm-first fallback | ✓ |
| G101 | L906 sdkKey(npm) ?? providerID | npm-first fallback | ✓ |
| G102 | hasSchemaIntent 15 键 | 全覆盖 | ✓ |

**综合分**：**89/100**（两轮一致）  
**结论**：优秀，可合入

---

## 历轮综合分追踪

| Round | 综合分 | 差距数 | 状态 |
|-------|--------|--------|------|
| R8    | 91/100 | G83-G91 | 已补全 |
| R9    | 90/100 | G92-G96 | 已补全 |
| **R10** | **89/100** | G97-G103 | **已补全** |
