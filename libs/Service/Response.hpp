#pragma once

#include <string>
#include <filesystem>
#include "Database/QueryTypes.hpp"
#include "Database/Database.hpp"
#include "Service/LibraryApi.hpp"

namespace Librium::Service {

class IResponse
{
public:
    virtual ~IResponse() = default;

    virtual void SetError(const std::string& message) = 0;
    
    virtual void SetData(const Db::SImportStats& stats) = 0;
    virtual void SetData(const Db::SQueryResult& result) = 0;
    virtual void SetData(const SAppStats& stats) = 0;
    virtual void SetData(const SBookDetails& book) = 0;
    virtual void SetDataExport(const std::filesystem::path& path, const std::string& filename) = 0;
    virtual void SetProgress(size_t processed, size_t total) = 0;
};

} // namespace Librium::Service
