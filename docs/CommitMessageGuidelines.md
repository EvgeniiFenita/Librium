# Commit Message Guidelines for Librium

To maintain a clean and informative git history, all commit messages must follow these rules. These guidelines are designed to help developers create high-signal, descriptive commit messages based on actual code changes.

## Message Format

Each commit message should follow this structure:
`<Type>: <Short Summary>`

### Critical Rules
- **Capitalization**: The `<Type>` MUST be capitalized (e.g., `Fix:`, not `fix:`).
- **No Scopes**: DO NOT use parentheses for scopes (e.g., `Feature:`, not `feat(api):`). The summary itself should be descriptive enough.
- **No Whitespace**: Commit messages MUST NOT start or end with whitespace characters (spaces, tabs).
- **No BOM**: Messages MUST be plain UTF-8 without Byte Order Mark (BOM) or hidden characters.
- **Imperative Mood**: Always use imperative ("Add", "Fix", "Refactor"), not past tense ("Added", "Fixed", "Refactored"). This is a hard rule.
- **Strict Types**: ONLY use the 6 types listed below.

### Commit Types
- **Feature**: New functionality (e.g., `Feature: Add support for FB2 covers`).
- **Refactor**: Code changes that neither fix a bug nor add a feature (e.g., `Refactor: Extract SQL queries to SqlQueries.hpp`).
- **Fix**: Bug fixes (e.g., `Fix: Handle empty ZIP archives during indexing`).
- **Test**: Adding or updating tests (e.g., `Test: Add scenarios for unicode search`).
- **Docs**: Documentation changes (e.g., `Docs: Update project overview`).
- **Build**: Changes to build system, dependencies, or CI/CD (e.g., `Build: Add Linux preset to CMakePresets.json`).

## Examples of High-Signal vs. Low-Signal Messages

| Low-Signal (AVOID) | High-Signal (USE) |
| :--- | :--- |
| `Refactoring` | `Refactor: Consolidate query logic into CDatabase class` |
| `Update docs` | `Docs: Add guidelines for creating descriptive commit messages` |
| `Fix bug` | `Fix: Correct UTF-8 validation in Fb2Parser for legacy encodings` |
| `Added api` | `Feature: Implement LibraryApi to encapsulate application business logic` |
| `fix(web): deadlock` | `Fix: Prevent TCP deadlock in web interface` |
| `feat: add cover` | `Feature: Add book cover extraction support` |

## Common Violations to Avoid

- **Lowercase types**: `fix: something` (Wrong) -> `Fix: Something` (Right)
- **Scopes in parentheses**: `Refactor(db): rename` (Wrong) -> `Refactor: Rename database fields` (Right)
- **Past tense**: `Added test` (Wrong) -> `Test: Add test cases` (Right)
- **Non-standard types**: `Improve: performance` (Wrong) -> `Refactor: Improve performance` (Right)

## Workflow & Authorization

- **No Auto-Commits**: Commits must NEVER be staged or applied automatically.
- **Explicit Request**: Commits are performed ONLY when the user explicitly provides a directive (e.g., "Commit these changes," "Make a beautiful commit").
- **Draft First**: Before committing, the assistant should propose a draft message and ask for confirmation if the change is complex.

## Verification
Before committing, always review the message with `git status` to ensure it accurately reflects the staged changes.
