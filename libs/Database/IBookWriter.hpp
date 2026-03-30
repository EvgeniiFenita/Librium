#pragma once

#include "Fb2/Fb2Parser.hpp"
#include "Inpx/BookRecord.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace Librium::Db {

class IBookWriter
{
public:
    virtual ~IBookWriter() = default;

    virtual void BeginTransaction() = 0;
    virtual void Commit() = 0;
    virtual void Rollback() = 0;

    [[nodiscard]] virtual int64_t InsertBook(
        const Inpx::SBookRecord& record,
        const Fb2::SFb2Data&     fb2 = {}) = 0;

    [[nodiscard]] virtual bool BookExists(
        const std::string& libId,
        const std::string& archiveName) = 0;

    [[nodiscard]] virtual std::vector<std::string> GetIndexedArchives() = 0;
    virtual void MarkArchiveIndexed(const std::string& archiveName) = 0;

    virtual void DropIndexes() = 0;
    virtual void CreateIndexes() = 0;
    virtual void BeginBulkImport() = 0;
    virtual void EndBulkImport() = 0;
    virtual void ClearImportCaches() = 0;
};

} // namespace Librium::Db
