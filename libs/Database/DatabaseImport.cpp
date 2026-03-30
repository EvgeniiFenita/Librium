#include "Database.hpp"
#include "Utils/GenreTranslator.hpp"

namespace Librium::Db {

int64_t CDatabase::GetOrCreateAuthor(const Inpx::SAuthor& author)
{
    std::string key = author.lastName + "|" + author.firstName + "|" + author.middleName;
    auto it = m_cacheAuthors.find(key);
    if (it != m_cacheAuthors.end()) return it->second;

    m_stmtInsertAuthor->Reset();
    m_stmtInsertAuthor->BindText(1, author.lastName);
    m_stmtInsertAuthor->BindText(2, author.firstName);
    m_stmtInsertAuthor->BindText(3, author.middleName);

    std::string searchName = author.lastName;
    if (!author.firstName.empty()) searchName += " " + author.firstName;
    if (!author.middleName.empty()) searchName += " " + author.middleName;
    m_stmtInsertAuthor->BindText(4, searchName);

    m_stmtInsertAuthor->Step();

    m_stmtGetAuthor->Reset();
    m_stmtGetAuthor->BindText(1, author.lastName);
    m_stmtGetAuthor->BindText(2, author.firstName);
    m_stmtGetAuthor->BindText(3, author.middleName);
    m_stmtGetAuthor->Step();
    CSqlStmtResetGuard authorGuard(*m_stmtGetAuthor);

    int64_t id = 0;
    if (m_stmtGetAuthor->IsRow())
        id = m_stmtGetAuthor->ColumnInt64(0);

    if (id > 0) m_cacheAuthors[key] = id;
    return id;
}

int64_t CDatabase::GetOrCreateGenre(const std::string& genre)
{
    auto it = m_cacheGenres.find(genre);
    if (it != m_cacheGenres.end()) return it->second;

    m_stmtInsertGenre->Reset();
    m_stmtInsertGenre->BindText(1, genre);
    m_stmtInsertGenre->Step();

    m_stmtGetGenre->Reset();
    m_stmtGetGenre->BindText(1, genre);
    m_stmtGetGenre->Step();
    CSqlStmtResetGuard genreGuard(*m_stmtGetGenre);

    int64_t id = 0;
    if (m_stmtGetGenre->IsRow())
        id = m_stmtGetGenre->ColumnInt64(0);

    if (id > 0) m_cacheGenres[genre] = id;
    return id;
}

int64_t CDatabase::GetOrCreateSeries(const std::string& series)
{
    if (series.empty()) return 0;
    auto it = m_cacheSeries.find(series);
    if (it != m_cacheSeries.end()) return it->second;

    m_stmtInsertSeries->Reset();
    m_stmtInsertSeries->BindText(1, series);
    m_stmtInsertSeries->BindText(2, series);
    m_stmtInsertSeries->Step();

    m_stmtGetSeries->Reset();
    m_stmtGetSeries->BindText(1, series);
    m_stmtGetSeries->Step();
    CSqlStmtResetGuard seriesGuard(*m_stmtGetSeries);

    int64_t id = 0;
    if (m_stmtGetSeries->IsRow())
        id = m_stmtGetSeries->ColumnInt64(0);

    if (id > 0) m_cacheSeries[series] = id;
    return id;
}

int64_t CDatabase::GetOrCreatePublisher(const std::string& pub)
{
    if (pub.empty()) return 0;
    auto it = m_cachePublishers.find(pub);
    if (it != m_cachePublishers.end()) return it->second;

    m_stmtInsertPublisher->Reset();
    m_stmtInsertPublisher->BindText(1, pub);
    m_stmtInsertPublisher->Step();

    m_stmtGetPublisher->Reset();
    m_stmtGetPublisher->BindText(1, pub);
    m_stmtGetPublisher->Step();
    CSqlStmtResetGuard publisherGuard(*m_stmtGetPublisher);

    int64_t id = 0;
    if (m_stmtGetPublisher->IsRow())
        id = m_stmtGetPublisher->ColumnInt64(0);

    if (id > 0) m_cachePublishers[pub] = id;
    return id;
}

int64_t CDatabase::GetOrCreateArchive(const std::string& name)
{
    auto it = m_cacheArchives.find(name);
    if (it != m_cacheArchives.end()) return it->second;

    m_stmtInsertArchive->Reset();
    m_stmtInsertArchive->BindText(1, name);
    m_stmtInsertArchive->Step();

    m_stmtGetArchive->Reset();
    m_stmtGetArchive->BindText(1, name);
    m_stmtGetArchive->Step();
    CSqlStmtResetGuard archiveGuard(*m_stmtGetArchive);

    int64_t id = 0;
    if (m_stmtGetArchive->IsRow())
        id = m_stmtGetArchive->ColumnInt64(0);

    if (id > 0) m_cacheArchives[name] = id;
    return id;
}

int64_t CDatabase::InsertBook(const Inpx::SBookRecord& rec, const Fb2::SFb2Data& fb2)
{
    int64_t archId = GetOrCreateArchive(rec.archiveName);
    int64_t seriesId = GetOrCreateSeries(rec.series);
    int64_t pubId = GetOrCreatePublisher(fb2.publisher);

    m_stmtInsertBook->Reset();
    m_stmtInsertBook->BindText(1, rec.libId);
    m_stmtInsertBook->BindInt64(2, archId);
    m_stmtInsertBook->BindText(3, rec.title);
    m_stmtInsertBook->BindText(4, rec.title);

    if (seriesId > 0) m_stmtInsertBook->BindInt64(5, seriesId);
    else              m_stmtInsertBook->BindNull(5);

    m_stmtInsertBook->BindInt(6, rec.seriesNumber);
    m_stmtInsertBook->BindText(7, rec.fileName);
    m_stmtInsertBook->BindInt64(8, rec.fileSize);
    m_stmtInsertBook->BindText(9, rec.fileExt);
    m_stmtInsertBook->BindText(10, rec.dateAdded);
    m_stmtInsertBook->BindText(11, rec.language);
    m_stmtInsertBook->BindInt(12, rec.rating);
    m_stmtInsertBook->BindText(13, rec.keywords);
    m_stmtInsertBook->BindText(14, fb2.annotation);

    if (pubId > 0) m_stmtInsertBook->BindInt64(15, pubId);
    else           m_stmtInsertBook->BindNull(15);

    m_stmtInsertBook->BindText(16, fb2.isbn);
    m_stmtInsertBook->BindText(17, fb2.publishYear);

    m_stmtInsertBook->Step();
    if (!m_stmtInsertBook->IsDone())
        return 0;

    int64_t bookId = LastInsertRowId();

    for (const auto& author : rec.authors)
    {
        int64_t aid = GetOrCreateAuthor(author);
        m_stmtInsertBookAuthor->Reset();
        m_stmtInsertBookAuthor->BindInt64(1, bookId);
        m_stmtInsertBookAuthor->BindInt64(2, aid);
        m_stmtInsertBookAuthor->Step();
    }

    for (const auto& genre : rec.genres)
    {
        int64_t gid = GetOrCreateGenre(Utils::CGenreTranslator::Translate(genre));
        m_stmtInsertBookGenre->Reset();
        m_stmtInsertBookGenre->BindInt64(1, bookId);
        m_stmtInsertBookGenre->BindInt64(2, gid);
        m_stmtInsertBookGenre->Step();
    }

    return bookId;
}

bool CDatabase::BookExists(const std::string& libId, const std::string& archiveName)
{
    int64_t archId = GetOrCreateArchive(archiveName);
    m_stmtBookExists->Reset();
    m_stmtBookExists->BindText(1, libId);
    m_stmtBookExists->BindInt64(2, archId);
    m_stmtBookExists->Step();
    CSqlStmtResetGuard guard(*m_stmtBookExists);
    return m_stmtBookExists->IsRow();
}

} // namespace Librium::Db
