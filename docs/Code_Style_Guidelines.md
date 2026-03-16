# LLM Rules for Code Generation (C++20)

This file defines strict rules for **LLM systems generating C++ code** in this repository.

LLM must follow these rules **without exceptions**.

---

# 1. Naming Rules

Classes must start with `C`.

Example:

```cpp
class CUserService
{
};
```

Interfaces must start with `I`.

```cpp
class IUserRepository
{
};
```

Enums must start with `E`.

```cpp
enum class EGameState
{
};
```

---

# 2. Member Variables

All class member variables must use the prefix:

```
m_
```

Example:

```cpp
int m_userId;
std::string m_userName;
```

---

# 3. Function Naming

Functions and methods must use **PascalCase**.

Correct:

```
GetUser
CreateUser
LoadConfiguration
```

Incorrect:

```
get_user
create_user
```

---

# 4. Parameter Naming

Function parameters must use **camelCase**.

Example:

```cpp
int GetUserById(int userId);
```

---

# 5. Braces Style

Opening braces must be on a **new line**.

Correct:

```cpp
class CUserService
{
};
```

Incorrect:

```cpp
class CUserService {
};
```

---

# 6. Namespaces

Use **nested namespace syntax** (C++20) instead of separate nested blocks.

Opening brace must be on the **same line** as the namespace declaration.

Correct:

```cpp
namespace MyProject::Core::Services {

class CUserService
{
};

} // namespace MyProject::Core::Services
```

Incorrect:

```cpp
namespace MyProject
{
    namespace Core
    {
        namespace Services
        {
        }
    }
}
```

Always close with a comment indicating which namespace is being closed.

---

# 7. Access Specifiers

Do **not insert empty lines** after:

```
public:
private:
protected:
```

Correct:

```cpp
class CUserService
{
public:
    void GetUser();
};
```

Incorrect:

```cpp
class CUserService
{
public:

    void GetUser();
};
```

---

# 8. Include Order

Includes must follow this order:

1. Corresponding header
2. Standard library
3. Third-party libraries
4. Project headers

Example:

```cpp
#include "UserService.h"

#include <memory>
#include <string>

#include <fmt/core.h>

#include "UserRepository.h"
```

---

# 9. Smart Pointers

Prefer smart pointers instead of raw pointers.

Use:

```
std::unique_ptr
std::shared_ptr
std::weak_ptr
```

Avoid:

```
new
delete
```

---

# 10. RAII

Resources must follow the **RAII principle**.

Acquire resources in constructors and release them in destructors.

---

# 11. Const Correctness

Use `const` whenever possible.

Example:

```cpp
int GetUserId() const;
```

---

# 12. Enum Style

Enum values must use **PascalCase**.

Example:

```cpp
enum class EGameState
{
    Menu,
    Playing,
    Paused
};
```

---

# 13. Include What You Use

Each file must include **only headers that it actually uses**.

Avoid unnecessary includes.

---

# 14. Language and Comments

- **No Transliteration**: Using transliteration (e.g., writing Russian words using the Latin alphabet) is strictly forbidden.
- **English Only**: All comments, documentation, log messages, and CLI outputs must be written in **English**.
- Exception: Test data may contain localized strings (e.g., Cyrillic titles) to verify Unicode support.

Correct:
```cpp
// Initialize the database connection
```

Incorrect:
```cpp
// Inicializatsiya podklyucheniya k baze dannyh
```

---

# 15. File Naming

All project-specific files must use **PascalCase** (`ThisIsFile.cpp`). This applies to source files (`.cpp`, `.hpp`, `.h`), scripts (`.py`, `.ps1`), and configuration files (`.json`, `.xml`, `.md`). Standard build system files (`CMakeLists.txt`, `vcpkg.json`, `CMakePresets.json`) and version control files (`.gitignore`) remain unchanged.

Incorrect:

```
CUserService.h
IUserRepository.h
EGameState.h
user_service.h
userService.h
run-tests.ps1
```

---

# 26. Build Target Naming

All binary and library targets in CMake must be named in **PascalCase** (`MyLib`, `MyApp`).

Correct:

```cmake
add_library(Config STATIC AppConfig.cpp)
add_executable(Indexer Main.cpp)
```

Incorrect:

```cmake
add_library(libindexer_core STATIC ...)
add_executable(libquery Main.cpp)
```

---

# 16. nullptr

Use `nullptr` instead of `NULL` or `0` for null pointer values.

Correct:

```cpp
int* ptr = nullptr;
```

Incorrect:

```cpp
int* ptr = NULL;
int* ptr = 0;
```

---

# 17. override and final

Always use `override` when overriding a virtual method.

Use `final` to prevent further overriding.

Correct:

```cpp
class CUserService : public IUserService
{
public:
    void GetUser() override;
};
```

Incorrect:

```cpp
class CUserService : public IUserService
{
public:
    void GetUser();  // Missing override
};
```

---

# 18. Class Section Order

Class sections must follow this order:

1. `public`
2. `protected`
3. `private`

Example:

```cpp
class CUserService
{
public:
    void GetUser() const;

protected:
    void OnUserLoaded();

private:
    int m_userId;
};
```

---

# 19. No `using namespace` in Headers

`using namespace` is forbidden in header files.

Correct (in .cpp only):

```cpp
// UserService.cpp
using namespace std;
```

Incorrect:

```cpp
// UserService.h
using namespace std;  // Pollutes all files that include this header
```

---

# 20. [[nodiscard]]

Mark functions with `[[nodiscard]]` when ignoring the return value is likely a bug.

Example:

```cpp
[[nodiscard]] std::unique_ptr<CUser> CreateUser(int userId);
```

---

# 21. Member Initialization

Prefer in-class initialization or constructor member initializer lists over assignment in the constructor body.

Correct:

```cpp
class CUserService
{
public:
    CUserService(int userId)
        : m_userId(userId)
        , m_userName("unknown")
    {
    }

private:
    int m_userId = 0;
    std::string m_userName;
};
```

Incorrect:

```cpp
CUserService::CUserService(int userId)
{
    m_userId = userId;      // Assignment, not initialization
    m_userName = "unknown";
}
```

---

# 22. Braces for Single-Line Bodies

For `if`, `else`, `for`, `while`, and `do-while` statements, curly braces **may be omitted** if the body consists of a single statement.

Correct:

```cpp
if (m_userId == 0)
    return;

for (int i = 0; i < count; ++i)
    ProcessItem(i);
```

Also correct (braces are always allowed):

```cpp
if (m_userId == 0)
{
    return;
}
```

Incorrect (brace style must still follow rule #5 when braces are used):

```cpp
if (m_userId == 0) { return; }
```

---

# 23. Avoid Standalone Free Functions

Free functions outside of anonymous namespaces are **forbidden**.

Instead, group related functions into a class with a meaningful name and `static` methods.

Correct:

```cpp
class CUserValidator
{
public:
    static bool IsValid(int userId);
    static bool HasPermission(int userId, EPermission permission);
};
```

Exception — free functions are allowed inside anonymous namespaces (file-local helpers):

```cpp
namespace {

bool IsValidInternal(int userId)
{
    return userId > 0;
}

} // namespace
```

Incorrect:

```cpp
// UserUtils.h
bool IsValidUser(int userId);
void NormalizeUser(CUser& user);  // Should be grouped into a class
```

---

# 24. Anonymous Namespaces

Anonymous namespaces (`namespace { ... }`) must **not** be nested inside named namespaces.
They must be declared at the global scope (outside of any named namespace).

Correct:

```cpp
namespace {

void HelperFunction()
{
}

} // namespace

namespace MyProject::Core {

void CMyClass::DoWork()
{
    HelperFunction();
}

} // namespace MyProject::Core
```

Incorrect:

```cpp
namespace MyProject::Core {
namespace {

void HelperFunction()
{
}

} // namespace
} // namespace MyProject::Core
```

---

# 25. Code That Violates These Rules

Generated code that violates these rules must be considered **invalid** and must be rewritten.
