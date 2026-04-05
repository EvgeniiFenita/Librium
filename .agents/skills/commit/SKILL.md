---
name: commit
description: >
  Use when writing or reviewing a git commit message for Librium.
  Triggers on tasks like "write a commit message", "what should the commit say",
  "make a commit", "propose a commit", or "draft a commit for these changes".
  Always proposes a draft and waits for user approval — never commits automatically.
---

# Commit Message Guidelines — Librium

## Format

```
<Type>: <Short Summary>
```

Imperative mood. Capitalized Type. No trailing whitespace. Plain UTF-8 (no BOM).

## Types (exactly these 6)

| Type | Use for |
|---|---|
| `Feature` | New functionality |
| `Refactor` | Code change without bug fix or new feature |
| `Fix` | Bug fix |
| `Test` | Adding or updating tests |
| `Docs` | Documentation changes |
| `Build` | Build system, deps, CI/CD |

## Examples

```
Feature: Add book cover extraction from FB2 archives     ✅
Fix: Prevent TCP deadlock in engineClient reconnect      ✅
Refactor: Extract SQL queries to SqlQueries.hpp          ✅
Test: Add scenarios for Cyrillic unicode search          ✅
Docs: Add commit message guidelines                      ✅
Build: Add linux-release preset to CMakePresets.json     ✅
```

## Common Violations

```
fix(web): deadlock          ❌  lowercase type, has scope in parens
feat: add cover             ❌  lowercase type, abbreviation
Added test cases            ❌  past tense, no type
Refactoring                 ❌  no summary
Improve: performance        ❌  non-standard type (use Refactor:)
 Feature: something         ❌  leading space
```

## Workflow

1. **Never stage or commit automatically.** Always propose a draft first.
2. **Run `git status`** before composing to confirm what's actually staged.
3. **For complex changes**, describe the "what" and "why" in a short body paragraph after a blank line (optional but welcome).
4. **Await explicit user approval** before committing.
