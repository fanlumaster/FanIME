#include <fmt/core.h>
#include <iostream>
#include <sqlite3.h>

int main(int argc, char *argv[])
{
    std::cout << "This is a code template project for Windows cpp!" << '\n';
    fmt::print("Hello World from fmt!!\n");

    sqlite3 *db = nullptr;
    char *errMsg = nullptr;

    // Create or open database
    if (sqlite3_open("test.db", &db) != SQLITE_OK)
    {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    // Create table
    const char *createTableSQL = "CREATE TABLE IF NOT EXISTS users ("
                                 "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                 "name TEXT NOT NULL,"
                                 "age INTEGER);";

    if (sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK)
    {
        std::cerr << "Failed to create table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return 1;
    }

    // Insert data
    const char *insertSQL = "INSERT INTO users (name, age) VALUES ('Alice', 25);";
    if (sqlite3_exec(db, insertSQL, nullptr, nullptr, &errMsg) != SQLITE_OK)
    {
        std::cerr << "Failed to insert data: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return 1;
    }

    // Query data
    const char *querySQL = "SELECT id, name, age FROM users;";
    auto callback = [](void *, int argc, char **argv, char **colName) -> int {
        for (int i = 0; i < argc; ++i)
        {
            std::cout << colName[i] << ": " << (argv[i] ? argv[i] : "NULL") << " | ";
        }
        std::cout << "\n";
        return 0;
    };

    if (sqlite3_exec(db, querySQL, callback, nullptr, &errMsg) != SQLITE_OK)
    {
        std::cerr << "Failed to query data: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return 1;
    }

    // Close database
    sqlite3_close(db);
    std::cout << "All done.\n";
    return 0;
}
