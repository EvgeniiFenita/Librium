# Commit Message Guidelines for Librium

To maintain a clean and informative git history, all commit messages must follow these rules. These guidelines are designed to help the AI assistant and developers create high-signal, descriptive commit messages based on actual code changes.

## Message Format

Each commit message should follow this structure:
`<Type>: <Short Summary>`

Optional (but recommended for complex changes):
`<Detailed bullet points explaining WHY and WHAT changed>`

### Commit Types
- **Feature**: New functionality (e.g., `Feature: Add support for FB2 covers`).
- **Refactor**: Code changes that neither fix a bug nor add a feature (e.g., `Refactor: Extract SQL queries to SqlQueries.hpp`).
- **Fix**: Bug fixes (e.g., `Fix: Handle empty ZIP archives during indexing`).
- **Test**: Adding or updating tests (e.g., `Test: Add scenarios for unicode search`).
- **Docs**: Documentation changes (e.g., `Docs: Update project overview`).
- **Build**: Changes to build system, dependencies, or CI/CD (e.g., `Build: Add Linux preset to CMakePresets.json`).

## How to Generate a "Beautiful" Commit Message (AI Instructions)

When asked to "make a beautiful commit" or "describe changes," follow these steps:

1. **Analyze the Diff**: Use `git diff --staged` (or `git diff HEAD` if not staged) to see exactly what changed.
2. **Identify the Core Impact**:
    - Which libraries are affected? (`libs/Database`, `libs/Indexer`, etc.)
    - Are there architectural changes (new interfaces, extracted logic)?
    - Are there user-facing changes (new CLI commands, API protocol updates)?
3. **Draft the Summary**:
    - Be specific. Instead of `Refactor: Database cleanup`, use `Refactor: Abstract database implementation using ISqlDatabase interface`.
    - Focus on the *intent* and the *result*.
4. **Use Imperative Mood**: (e.g., "Add", "Fix", "Refactor" instead of "Added", "Fixed", "Refactoring"). *Note: The existing history used past tense, but moving forward, imperative is preferred for consistency with standard Git practices.*

## Examples of High-Signal vs. Low-Signal Messages

| Low-Signal (AVOID) | High-Signal (USE) |
| :--- | :--- |
| `Refactoring` | `Refactor: Consolidate query logic into CDatabase class` |
| `Update docs` | `Docs: Add guidelines for creating descriptive commit messages` |
| `Fix bug` | `Fix: Correct UTF-8 validation in Fb2Parser for legacy encodings` |
| `Added api` | `Feature: Implement LibraryApi to encapsulate application business logic` |

## Verification
Before committing, always review the message with `git status` to ensure it accurately reflects the staged changes.
