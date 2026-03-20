#pragma once

#include "ISqlStatement.hpp"
#include <memory>
#include <string>

namespace Librium::Db {

class ISqlDatabase
{
public:
    virtual ~ISqlDatabase() = default;

    virtual void Exec(const std::string& sql) = 0;
    [[nodiscard]] virtual std::unique_ptr<ISqlStatement> Prepare(const std::string& sql) = 0;

    virtual void BeginTransaction() = 0;
    virtual void Commit() = 0;
    virtual void Rollback() = 0;

    [[nodiscard]] virtual int64_t LastInsertRowId() const = 0;
};

} // namespace Librium::Db
