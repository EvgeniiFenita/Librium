#pragma once

#include "Config/AppConfig.hpp"
#include "Indexer/Indexer.hpp"
#include "Database/IBookReader.hpp"
#include "Database/IBookWriter.hpp"
#include "Database/QueryTypes.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <memory>

namespace Librium::Db { class CDatabase; }

namespace Librium::Service {

struct SAppStats {
    int64_t totalBooks      = 0;
    int64_t totalAuthors    = 0;
    int64_t indexedArchives = 0;
};

struct SBookDetails {
    Db::SBookResult book;
    std::u8string coverPath; // Empty if no cover
};

class CLibraryApi
{
public:
    explicit CLibraryApi(Config::SAppConfig cfg);
    ~CLibraryApi();

    [[nodiscard]] Db::SImportStats Import(Indexer::IProgressReporter* reporter);
    [[nodiscard]] Db::SImportStats Upgrade(Indexer::IProgressReporter* reporter);

    [[nodiscard]] Db::SQueryResult SearchBooks(const Db::SQueryParams& params);

    [[nodiscard]] std::filesystem::path ExportBook(int64_t id, const std::filesystem::path& outDir);

    [[nodiscard]] SAppStats GetStats();

    [[nodiscard]] std::optional<SBookDetails> GetBook(int64_t id);

private:
    void EnsureDatabase();
    Db::IBookReader& GetReader();
    Db::IBookWriter& GetWriter();

    Config::SAppConfig m_config;
    std::unique_ptr<Db::CDatabase> m_db;
};

} // namespace Librium::Service
