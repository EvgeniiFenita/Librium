#pragma once

#include <cstdint>
#include <string>

namespace Librium::Db {

class ISqlStatement
{
public:
    virtual ~ISqlStatement() = default;

    virtual void BindInt(int index, int value) = 0;
    virtual void BindInt64(int index, int64_t value) = 0;
    virtual void BindText(int index, const std::string& value) = 0;
    virtual void BindNull(int index) = 0;

    virtual void Step() = 0;
    virtual void Reset() = 0;

    [[nodiscard]] virtual int ColumnInt(int index) const = 0;
    [[nodiscard]] virtual int64_t ColumnInt64(int index) const = 0;
    [[nodiscard]] virtual std::string ColumnText(int index) const = 0;

    [[nodiscard]] virtual bool IsRow() const = 0;
    [[nodiscard]] virtual bool IsDone() const = 0;
};

} // namespace Librium::Db
