---
name: code-style
description: >
  Use when you need a quick reference for Librium C++ naming, formatting, or
  style rules. Triggers on tasks like "what's the naming convention", "how should
  I format this", "is this style correct", or when writing any new C++ or Python
  code for the project. Also covers Python scripting style for scripts/.
---

# Librium Code Style — Quick Reference

## Naming

| Entity | Rule | Example |
|---|---|---|
| Class | `C` prefix + PascalCase | `CUserService` |
| Interface | `I` prefix + PascalCase | `IUserRepository` |
| Struct | `S` prefix + PascalCase | `SBookRecord` |
| Enum | `E` prefix + PascalCase | `ELogLevel` |
| Enum value | PascalCase | `Menu`, `Playing` |
| Member variable | `m_` prefix | `m_dbPath` |
| Method / Function | PascalCase | `GetUserById` |
| Parameter | camelCase | `userId`, `queryText` |
| File (project) | PascalCase | `AppConfig.cpp`, `Run.py` |
| CMake target | PascalCase | `Database`, `Librium` |

Build system files (`CMakeLists.txt`, `vcpkg.json`) keep standard names.
Docs in `docs/` use PascalCase without underscores.

## Braces — Allman Style

Opening brace on **new line** for classes, structs, functions, control blocks.
Exception: namespace opening brace on **same line**.

```cpp
// ✅ Correct
class CUserService
{
public:
    void DoWork()
    {
        if (condition)
            DoSomething();   // single-statement body may omit braces
    }
};

namespace Librium::Db {
// ...
} // namespace Librium::Db

// ❌ Wrong
class CUserService {
    void DoWork() {
    }
};
```

## Class Layout

```cpp
class CMyClass
{
public:           // no empty line after specifier
    void Method();
protected:
    void Helper();
private:
    int m_value;
};
```

Order: `public` → `protected` → `private`.

## Include Order

```cpp
#include "MyModule/MyHeader.hpp"   // 1. own header

#include <memory>                  // 2. std
#include <string>

#include <fmt/core.h>              // 3. third-party

#include "OtherModule/Other.hpp"   // 4. project
```

## Key Rules

```cpp
// Paths — always via Utf8ToPath
auto p = Utils::CStringUtils::Utf8ToPath(someUtf8String);  // ✅
std::filesystem::path p = someUtf8String;                   // ❌

// Threads
std::jthread m_worker;   // ✅  (auto-join, interruptible)
std::thread m_worker;    // ❌

// Pointers
if (ptr == nullptr)   // ✅
if (ptr == NULL)      // ❌

// Virtual override
void Execute() override;   // ✅ always
void Execute();            // ❌ missing override

// Nodiscard
[[nodiscard]] bool TryConnect();

// No using namespace in headers — forbidden
// No std::cout / std::cerr in libs/ — use LOG_* macros only
```

## Logging Macros

```cpp
LOG_DEBUG("Parsing record id={}", id);
LOG_INFO("Import completed: {} books", count);
LOG_WARN("Unexpected state in {}", funcName);
LOG_ERROR("Failed to open archive: {}", e.what());
```

Logger is **silent by default**. Console output requires `AddConsoleOutput()`.

## Error Handling

```cpp
// ✅ Always catch specific exception and log
catch (const std::exception& e)
{
    LOG_ERROR("Failed: {}", e.what());
    // then handle or re-throw
}

// ❌ Never
catch (...) { }
catch (const std::exception&) { /* silent */ }
```

## Structs

```cpp
// ✅ Each member on own line
struct SResult
{
    int code;
    std::string message;
};

// ❌ One-liner forbidden
struct SResult { int code; std::string message; };
```

## Smart Pointers & RAII

- Prefer `std::unique_ptr` / `std::shared_ptr` over raw pointers
- All third-party handles (SQLite, libzip) → `unique_ptr` with custom deleter, immediately on acquisition
- No manual `close()`/`finalize()` in business logic

## Python (scripts/)

```python
# File naming: PascalCase
Run.py, LibraryGenerator.py

# Class naming: C prefix
class CLibraryGenerator:

# Method naming: PascalCase
def GenerateLibrary(self): ...

# Variable naming
local_var = ...       # snake_case for locals
def Foo(myParam): ... # camelCase for parameters

# Always at top of entry-point scripts
sys.dont_write_bytecode = True
os.environ["PYTHONDONTWRITEBYTECODE"] = "1"

# Paths
from pathlib import Path
p = Path("some/dir")

# Shared logic
from Core import ...
```
