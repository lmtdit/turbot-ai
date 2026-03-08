# Git Commit & Push — English Reference

Automatically commit changes following Conventional Commits convention and push to remote repository.

## Commit Message Format

Follow Conventional Commits spec:

```
<type>(<scope>): <description>

[optional body]
```

### Types

| Type       | Purpose                     |
| ---------- | --------------------------- |
| `feat`     | New feature                 |
| `fix`      | Bug fix                     |
| `docs`     | Documentation changes only  |
| `style`    | Formatting, no logic change |
| `refactor` | Code refactoring            |
| `test`     | Adding or updating tests    |
| `chore`    | Build, config, dependencies |
| `perf`     | Performance improvements    |
| `ci`       | CI/CD changes               |

### Examples

```
feat(core): add user authentication module
fix(storage): fix SQLite connection leak
docs(api): update API documentation
refactor(provider): extract common HTTP client logic
test(crypto): add encryption boundary tests
```

## Module-Based Batch Commit Rules

When changes span multiple modules, commits **must** be split by module. Never merge all changes into a single commit.

### Module Identification

Identify module by file path:

| Path Pattern       | Module Name        |
| ------------------ | ------------------ |
| `packages/app/`    | `app`              |
| `packages/ui/`     | `ui`               |
| `packages/sdk/`    | `sdk`              |
| `internal/agent/`  | `agent`            |
| `internal/db/`     | `db`               |
| `internal/config/` | `config`           |
| `libs/core/`       | `core`             |
| `libs/utils/`      | `utils`            |
| `libs/network/`    | `network`          |
| Root config files  | `chore`            |
| Other paths        | Top-level dir name |

### Batch Commit Flow

1. Run `git diff --name-only HEAD` (or `git status`) to get all changed files
2. Group files by module and list each module's change set
3. Show grouping results to user and confirm commit order
4. For each module **in sequence**:
   - `git add <files for this module>`
   - Analyze `git diff --cached`
   - Generate commit message for this module
   - `git commit -m "<type>(<module>): <description>"`
5. After all modules are committed, run one `git push`

### Batch Commit Example

```bash
# Batch 1: core module
git add libs/core/src/xxx.cpp libs/core/include/xxx.h
git commit -m "feat(core): add token streaming support"

# Batch 2: network module
git add libs/network/src/client.cpp
git commit -m "fix(network): fix HTTP connection timeout issue"

# Batch 3: config changes
git add internal/config/config.go
git commit -m "chore(config): update default timeout configuration"

# Push all at once
git push
```

## Workflow

1. **Check status**: Run `git status`
2. **Group by module**: Identify modules from file paths and group changed files
3. **Confirm order**: Show grouping to user and confirm batch order
4. **Stage per batch**: Only `git add` files for the current module
5. **Generate message**: Use `git diff --cached` to generate module-specific commit message
6. **Commit current batch**: Run `git commit -m "message"`
7. **Repeat 4–6**: Until all modules are committed
8. **Push once**: Run `git push` after all batches are done

## Commit Message Language Rules

Commit message language (English/Chinese) must match the repository's historical commits. **Never mix languages** in the same repository.

### Language Detection Flow

1. Fetch the last 10 commits:
   ```bash
   git log --oneline -10
   ```
2. Count the ratio of English vs Chinese descriptions:
   - **Majority English** → use English
   - **Majority Chinese** → use Chinese
   - **Equal (5:5)** → follow the language of the most recent commit
   - **No history (empty repo)** → default to English
3. Apply the detected language uniformly to all batches in this session

## Important Rules

- **Never push to main/master** unless the user explicitly confirms
- **Never use --force** unless the user explicitly requests
- Always show commit message to user before committing
- Stop and notify user if merge conflicts are detected
- **Commit language must match historical log** — never switch language arbitrarily
