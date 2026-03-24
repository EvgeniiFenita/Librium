#pragma once

#include "QueryTypes.hpp"
#include <cstdint>
#include <optional>

namespace Librium::Db {

class IBookReader
{
public:
    virtual ~IBookReader() = default;

    [[nodiscard]] virtual int64_t CountBooks() const = 0;
    [[nodiscard]] virtual int64_t CountAuthors() const = 0;

    [[nodiscard]] virtual SQueryResult ExecuteQuery(const SQueryParams& params) = 0;
    [[nodiscard]] virtual std::optional<SBookResult> GetBookById(int64_t id) = 0;
    [[nodiscard]] virtual std::optional<SBookPath> GetBookPath(int64_t bookId) = 0;
};

} // namespace Librium::Db
