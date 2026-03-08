# 实现 Config 配置分级机制

## Phase 1: 核心实现

### 1.1 新增文件

**头文件**: `libs/core/include/turbot/core/config/config_manager.hpp`
- ConfigLevel 枚举（Default, User, Project）
- ExtensionType 枚举（Agent, Skill, Rule, Event, Extension）
- LoadResult 结构体
- ConfigManager 类

**源文件**: `libs/core/src/config/config_manager.cpp`
- 配置路径解析
- 三级配置加载逻辑
- 配置合并策略
- 环境变量解析增强

**YAML 解析**: `libs/core/src/config/yaml_parser.cpp`
- YAML Frontmatter 解析
- yaml_to_json 转换

### 1.2 修改文件

**CMakeLists.txt**: `libs/core/CMakeLists.txt`
- 添加新源文件

**Config 类增强**: `libs/core/include/turbot/core/config/config.hpp`
- 添加 `merge_config()` 公开方法
- 添加 `resolve_env_vars()` 方法

## Phase 2: 单元测试（覆盖率 > 95%）

### 2.1 新增测试文件

**测试文件**: `tests/unit/config_manager_test.cpp`

测试用例覆盖：

| 测试场景 | 测试内容 |
|---------|---------|
| **配置层级** | |
| ConfigLevel 枚举 | Default/User/Project 枚举值验证 |
| 路径解析 | user_config_path, project_config_path |
| 环境变量覆盖 | TURBOT_USER_CONFIG_PATH, TURBOT_PROJECT_CONFIG_PATH |
| **配置加载** | |
| 加载默认配置 | get_default_config() 返回正确默认值 |
| 加载用户配置 | ~/.turbot/turbot.json 加载 |
| 加载项目配置 | ./.turbot/turbot.json 加载 |
| 文件不存在处理 | 跳过该层级，返回 LoadResult |
| JSON 解析错误 | 记录错误，返回 LoadResult |
| **配置合并** | |
| 对象递归合并 | 深度合并嵌套对象 |
| 数组替换 | 非对象直接替换 |
| _append 后缀 | 数组追加功能 |
| **环境变量** | |
| 环境变量引用 | ${VAR} 解析 |
| 带默认值引用 | ${VAR:-default} 解析 |
| API Key 映射 | OPENAI_API_KEY → providers[name=openai].api_key |
| **YAML Frontmatter** | |
| 解析 Frontmatter | 提取 YAML 元数据 |
| 解析 Markdown 正文 | 提取正文内容 |
| 无 Frontmatter 处理 | 返回空 frontmatter |
| **配置验证** | |
| 版本检查 | 缺少 version 字段 |
| Provider 验证 | 缺少 name/type 字段 |
| 权限模式验证 | 非法 mode 值 |
| **错误处理** | |
| ConfigException | 错误类型和消息 |
| 错误恢复策略 | 文件不存在/解析错误/验证错误 |
| **完整流程** | |
| initialize() | 按顺序加载所有层级 |
| reload() | 重新加载配置 |
| save_config() | 保存到指定层级 |

### 2.2 测试覆盖率目标

| 模块 | 目标覆盖率 |
|------|-----------|
| ConfigManager | > 95% |
| YAML Parser | > 95% |
| Config 增强 | > 95% |

## Phase 3: 代码审核与优化

### 3.1 审核要点

- 线程安全性：mutex 保护
- 内存安全：RAII、智能指针
- 异常安全：错误处理完整
- 性能：避免不必要的拷贝
- 代码风格：符合 clang-tidy

### 3.2 优化方向

- 配置热重载
- 缓存机制
- 延迟加载

## Phase 4: 文档更新与提交

### 4.1 文档更新

更新 `docs/architecture/config-hierarchy.md`：
- 更新实现状态
- 添加 API 使用示例

### 4.2 Git 提交

使用 git-commit-push skill 提交代码：
```
feat(config): implement hierarchical config loading mechanism

- Add ConfigManager class with 3-level config loading
- Support YAML Frontmatter parsing
- Add config validation and error handling
- Add comprehensive unit tests (>95% coverage)
```

## 文件清单

### 新增文件
- `libs/core/include/turbot/core/config/config_manager.hpp`
- `libs/core/src/config/config_manager.cpp`
- `libs/core/src/config/yaml_parser.cpp`
- `tests/unit/config_manager_test.cpp`

### 修改文件
- `libs/core/CMakeLists.txt`
- `libs/core/include/turbot/core/config/config.hpp`
- `libs/core/src/config/config.cpp`
- `docs/architecture/config-hierarchy.md`
