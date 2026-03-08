---
name: javascript-code-review
description: TypeScript 代码审核专用技能。检查类型安全、异步模式、React/Node.js 最佳实践、依赖管理等。当审核 JavaScript/TypeScript 代码时自动加载此技能。
---

# JavaScript 代码审核技能

针对 JavaScript/TypeScript 项目的专业代码审核规范和最佳实践检查。

## 触发条件

- 文件扩展名为 `.js`、`.jsx`、`.ts`、`.tsx`、`.mjs`
- 项目包含 `tsconfig.json`、`package.json`
- 代码中出现 TypeScript 特定语法（接口、类型、泛型等）

## 审核清单

### Critical - 必须修复

#### 类型安全

- [ ] **any 类型**: 是否滥用 `any`，应使用具体类型或 `unknown`
- [ ] **类型断言**: `as` 断言是否安全，是否可以避免
- [ ] **非空断言**: `!` 操作符是否可能导致运行时错误
- [ ] **类型遗漏**: 函数返回值、变量是否缺少类型声明

```typescript
// 错误: 滥用 any
function process(data: any) {
  return data.value
}

// 正确: 使用具体类型
interface Data {
  value: string
}
function process(data: Data): string {
  return data.value
}
```

#### 异步安全

- [ ] **未处理的 Promise**: 是否缺少 `await` 或 `.catch()`
- [ ] **Promise 组合**: `Promise.all` 是否处理部分失败
- [ ] **竞态条件**: 异步操作是否存在竞态风险

```typescript
// 错误: 未处理的 Promise
async function bad() {
  fetchData() // 缺少 await，错误被忽略
}

// 正确: 正确处理
async function good() {
  try {
    await fetchData()
  } catch (error) {
    console.error('Fetch failed:', error)
  }
}
```

#### 空值安全

- [ ] **可选链**: 是否正确使用 `?.` 操作符
- [ ] **空值合并**: 是否正确使用 `??` vs `||`
- [ ] **undefined vs null**: 是否统一处理

```typescript
// 错误: 可能崩溃
function getName(user: User) {
  return user.profile.name // profile 可能为 undefined
}

// 正确: 可选链
function getName(user: User) {
  return user.profile?.name ?? 'Unknown'
}
```

### Warning - 应该修复

#### 代码风格

- [ ] **命名规范**: 变量/函数使用 camelCase，类型/接口使用 PascalCase
- [ ] **导出方式**: 是否统一使用命名导出或默认导出
- [ ] **模块组织**: 文件是否职责单一

```typescript
// 推荐: 清晰的类型命名
interface UserProfile {
  id: string
  displayName: string
}

type UserId = string
```

#### 函数设计

- [ ] **纯函数**: 副作用是否明确
- [ ] **参数数量**: 是否超过 3-4 个参数（考虑使用对象参数）
- [ ] **默认参数**: 是否使用默认参数替代条件判断

```typescript
// 避免: 多个参数
function createUser(name: string, age: number, email: string, role: string) {}

// 推荐: 对象参数
interface CreateUserOptions {
  name: string
  age: number
  email: string
  role?: string
}
function createUser(options: CreateUserOptions) {}
```

#### React 特定（如适用）

- [ ] **Hooks 规则**: 是否在条件语句中使用 hooks
- [ ] **依赖数组**: `useEffect` 依赖是否完整
- [ ] **状态更新**: 是否正确使用函数式更新
- [ ] **内存泄漏**: 是否清理订阅和定时器

```typescript
// 错误: 条件中使用 hooks
function Component({ isActive }) {
  if (isActive) {
    useEffect(() => {}, []) // 违反 hooks 规则
  }
}

// 正确: hooks 在顶层
function Component({ isActive }) {
  useEffect(() => {
    if (!isActive) return
    // ...
  }, [isActive])
}
```

### Suggestion - 建议改进

#### 性能优化

- [ ] **memo/useMemo/useCallback**: 是否不必要的重渲染
- [ ] **代码分割**: 是否使用动态导入 `import()`
- [ ] **bundle 大小**: 是否导入未使用的代码

```typescript
// 避免: 导入整个库
import _ from 'lodash'

// 推荐: 按需导入
import debounce from 'lodash/debounce'
```

#### 可维护性

- [ ] **接口 vs 类型**: 是否合理选择 `interface` 或 `type`
- [ ] **泛型约束**: 泛型是否有适当的约束
- [ ] **工具类型**: 是否利用内置工具类型

```typescript
// 推荐: 使用工具类型
type ReadonlyUser = Readonly<User>
type PartialUser = Partial<User>
type UserKeys = keyof User
```

## 常见问题模式

### 1. 类型陷阱

```typescript
// 错误: 类型推断为 never[]
const arr = [] // never[]

// 正确: 明确类型
const arr: string[] = []
const arr = [] as string[]
```

### 2. 对象字面量类型

```typescript
// 错误: 类型推断可能不精确
function getConfig() {
  return { mode: 'development', port: 3000 }
}

// 正确: 返回类型推断或显式声明
interface Config {
  mode: 'development' | 'production'
  port: number
}
function getConfig(): Config {
  return { mode: 'development', port: 3000 }
}
```

### 3. 枚举 vs 联合类型

```typescript
// 推荐: 字面量联合类型（更简洁）
type Status = 'pending' | 'approved' | 'rejected'

// 或使用 const enum（编译后内联）
const enum Status {
  Pending = 'pending',
  Approved = 'approved',
  Rejected = 'rejected',
}
```

### 4. 类型守卫

```typescript
// 错误: 类型不安全
function process(value: string | number) {
  return value.toUpperCase() // number 没有 toUpperCase
}

// 正确: 使用类型守卫
function process(value: string | number) {
  if (typeof value === 'string') {
    return value.toUpperCase()
  }
  return value.toFixed(2)
}

// 或使用类型谓词
function isString(value: unknown): value is string {
  return typeof value === 'string'
}
```

## 项目配置建议

### tsconfig.json 推荐

```json
{
  "compilerOptions": {
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "noImplicitReturns": true,
    "noFallthroughCasesInSwitch": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "exactOptionalPropertyTypes": true
  }
}
```

### ESLint 规则

```json
{
  "rules": {
    "@typescript-eslint/no-explicit-any": "error",
    "@typescript-eslint/no-non-null-assertion": "warn",
    "@typescript-eslint/explicit-function-return-type": "warn",
    "@typescript-eslint/no-floating-promises": "error",
    "@typescript-eslint/await-thenable": "error"
  }
}
```

## 工具推荐

- **类型检查**: `tsc --noEmit`
- **Lint**: ESLint + @typescript-eslint
- **格式化**: Prettier
- **测试**: Jest / Vitest

## 架构审核规范

### 公共模块识别

审核 TypeScript 代码时，必须检查纯函数是否存在重复实现：

**检测流程**:

1. **标记纯函数**: 识别无副作用、仅依赖输入的函数

   ```typescript
   // 纯函数示例
   export function formatDate(date: Date): string // ✓ 纯函数
   export function log(message: string): void // ✗ 有副作用
   ```

2. **搜索重复实现**: 使用搜索工具查找相似功能

   ```bash
   # 搜索相似函数名
   grep -r "formatDate\|dateFormat\|toDateString" --include="*.ts" --include="*.tsx"
   ```

3. **提升标准**: 重复 >= 2 次时建议提升

**输出示例**:

```markdown
### 架构问题 - 重复代码检测

| 函数           | 重复位置                                                       | 目标位置                | 理由                        |
| -------------- | -------------------------------------------------------------- | ----------------------- | --------------------------- |
| `formatDate()` | `packages/app/utils/date.ts`, `packages/console/utils/date.ts` | `packages/util/date.ts` | 重复 2 次，属于通用日期工具 |
```

### TypeScript 模块组织最佳实践

**通用工具模块**:

```typescript
// packages/util/date.ts

/** 格式化日期为 YYYY-MM-DD 格式 */
export function formatDate(date: Date): string {
  return date.toISOString().split('T')[0]
}

/** 格式化日期时间 */
export function formatDateTime(date: Date): string {
  return date.toISOString()
}
```

**类型定义模块**:

```typescript
// packages/util/types.ts

export interface Result<T, E = Error> {
  ok: true
  value: T
} | {
  ok: false
  error: E
}

export type AsyncResult<T, E = Error> = Promise<Result<T, E>>
```

**索引导出**:

```typescript
// packages/util/index.ts
export * from './date'
export * from './types'
export * from './string'
```

### 重复代码检测检查清单

- [ ] **工具函数**: 日期格式化、字符串处理、数组操作等
- [ ] **类型定义**: 通用接口、类型别名
- [ ] **常量定义**: 枚举值、配置常量
- [ ] **API 调用**: 相似的请求处理逻辑
