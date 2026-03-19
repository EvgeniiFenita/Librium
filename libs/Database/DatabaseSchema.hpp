#pragma once

struct sqlite3;

namespace Librium::Db {

class CDatabaseSchema
{
public:
    // Создает схему базы данных (таблицы и индексы)
    static void Create(sqlite3* db);

private:
    static void Exec(sqlite3* db, const char* sql);
};

} // namespace Librium::Db
