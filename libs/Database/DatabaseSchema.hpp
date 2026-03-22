#pragma once

namespace Librium::Db {

class ISqlDatabase;

class CDatabaseSchema
{
public:
    // Creates database schema (tables and indexes)
    static void Create(ISqlDatabase& db);

private:
    static void Exec(ISqlDatabase& db, const char* sql);
};

} // namespace Librium::Db
