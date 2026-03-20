#pragma once

namespace Librium::Db {

class ISqlDatabase;

class CDatabaseSchema
{
public:
    // Создает схему базы данных (таблицы и индексы)
    static void Create(ISqlDatabase& db);

private:
    static void Exec(ISqlDatabase& db, const char* sql);
};

} // namespace Librium::Db
