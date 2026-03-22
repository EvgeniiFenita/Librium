#pragma once

struct sqlite3;

namespace Librium::Db {

// Registers all custom SQLite functions for the given connection.
// Call immediately after opening the database.
void RegisterSqliteFunctions(sqlite3* db);

} // namespace Librium::Db
