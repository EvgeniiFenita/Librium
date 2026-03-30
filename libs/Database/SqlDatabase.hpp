#pragma once

#include "SqlStatement.hpp"
#include <memory>
#include <string>
#include <string_view>

namespace Librium::Db {

class ISqlDatabase
{
public:
    virtual ~ISqlDatabase() = default;

    virtual void Exec(std::string_view sql) = 0;
    [[nodiscard]] virtual std::unique_ptr<ISqlStatement> Prepare(std::string_view sql) = 0;

    virtual void BeginTransaction() = 0;
    virtual void Commit() = 0;
    virtual void Rollback() = 0;

    [[nodiscard]] virtual int64_t LastInsertRowId() const = 0;
    virtual void ReleaseMemory() = 0;
};

} // namespace Librium::Db
