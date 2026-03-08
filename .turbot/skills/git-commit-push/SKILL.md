---
name: git-commit-push
description: Execute git commit in Conventional Commits format and automatically push changes. Use when the user requests commit, push, or save changes. Automatically analyzes staged changes and generates descriptive commit messages.
---

# Git Commit & Push

Automate git commits following Conventional Commits convention and push to remote.

## When to Use

- User says "commit", "push", or "save changes"
- User says "提交代码", "推送", or "保存变更"
- After completing a set of code changes

## Language-Specific References

Detect user's preferred language or repository convention, then load the corresponding reference:

| Language | Reference File               | Load When                                                         |
| -------- | ---------------------------- | ----------------------------------------------------------------- |
| English  | [SKILL.en.md](./SKILL.en.md) | User communicates in English, or repo history is majority English |
| 中文     | [SKILL.zh.md](./SKILL.zh.md) | User communicates in Chinese, or repo history is majority Chinese |

> **Detection rule**: Run `git log --oneline -10` to check historical commit language.
> Default to English for empty repositories.

## Quick Workflow

1. **Detect language** — check `git log --oneline -10`, load matching reference file
2. **Check status** — run `git status`
3. **Group by module** — split changed files by directory/module
4. **Batch commit** — stage and commit each module separately
5. **Push once** — run `git push` after all batches

## Critical Rules

- **Never push to main/master** without explicit user confirmation
- **Never use --force** unless explicitly requested
- Always show commit messages to user before committing
- Stop and notify user on merge conflicts
- Commit language must match repository history — never mix languages
