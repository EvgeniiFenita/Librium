# LLM Rules for Code Generation (C++20)

This file defines strict rules for **LLM systems generating C++ code** in this repository.

LLM must follow these rules **without exceptions**.

---

# 1. Naming Rules

**Classes** must start with `C`.

Example:
```cpp
class CUserService
{
};
```

**Interfaces** must start with `I`.
```cpp
class IUserRepository
{
};
```

**Structs** must start with `S`.
```cpp
struct SUserRecord
{
    int id;
    std::string name;
};
```

**Enums** must start with `E`.
```cpp
enum class EGameState
{
};
```

---

# 2. Member Variables

All class and struct member variables must use the prefix:
```
m_
```
*(Note: For simple data-only structs, `m_` prefix may be omitted if they are used as plain-old-data containers, but `S` prefix is still mandatory)*

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

Opening braces for classes, structs, functions, and methods must be on a **new line** (Allman style).

Correct:
```cpp
class CUserService
{
};

void DoWork()
{
}
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
Use: `std::unique_ptr`, `std::shared_ptr`, `std::weak_ptr`.
Avoid: `new`, `delete`.

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

---

# 15. File Naming

All project-specific files must use **PascalCase** (`ThisIsFile.cpp`). Standard build system files (`CMakeLists.txt`, `vcpkg.json`, `CMakePresets.json`) and version control files (`.gitignore`) remain unchanged.

---

# 16. nullptr

Use `nullptr` instead of `NULL` or `0`.

---

# 17. override and final

Always use `override` when overriding a virtual method.
Use `final` to prevent further overriding.

---

# 18. Class Section Order

Class sections must follow this order:
1. `public`
2. `protected`
3. `private`

---

# 19. No `using namespace` in Headers

`using namespace` is forbidden in header files.

---

# 20. [[nodiscard]]

Mark functions with `[[nodiscard]]` when ignoring the return value is likely a bug.

---

# 21. Member Initialization

Prefer in-class initialization or constructor member initializer lists.

---

# 22. Braces for Single-Line Bodies

For `if`, `else`, `for`, `while`, curly braces **may be omitted** if the body consists of a single statement.

---

# 23. Formatting for Structs

Struct members must each be on their own line.

Correct:
```cpp
struct SResult
{
    int code;
    std::string message;
};
```

Incorrect:
```cpp
struct SResult { int code; std::string message; };
```

---

# 24. Function Parameters Layout

Function parameters should stay on a single line unless there are too many of them or the line exceeds reasonable length (e.g., 100-120 characters).

Correct:
```cpp
void ProcessData(const std::string& input, int count, bool dryRun);
```

---

# 25. Build Target Naming

All binary and library targets in CMake must be named in **PascalCase** (`MyLib`, `MyApp`).

---

# 26. Code That Violates These Rules

Generated code that violates these rules must be considered **invalid** and must be rewritten.
