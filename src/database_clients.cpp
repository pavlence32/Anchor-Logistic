#include "database.h"
#include "database_helpers.h"

#include <iostream>

int Database::AddClient(const Client& client) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const char* sql =
        "INSERT INTO clients (name, phone, address, longitude, latitude) "
        "VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "AddClient prepare error: " << sqlite3_errmsg(db_) << '\n';
        return -1;
    }
    db_internal::StatementGuard guard(stmt);

    const Coordinates coords = client.Get_coords();
    sqlite3_bind_text(stmt, 1, client.Get_name().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, client.Get_phone().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, client.Get_address().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, coords.longitude);
    sqlite3_bind_double(stmt, 5, coords.latitude);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        return static_cast<int>(sqlite3_last_insert_rowid(db_));
    }

    std::cerr << "AddClient step error: " << sqlite3_errmsg(db_) << '\n';
    return -1;
}

std::optional<Client> Database::GetClient(int id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const char* sql =
        "SELECT id, name, phone, address, longitude, latitude "
        "FROM clients WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetClient prepare error: " << sqlite3_errmsg(db_) << '\n';
        return std::nullopt;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const Coordinates coords(
            sqlite3_column_double(stmt, 4),
            sqlite3_column_double(stmt, 5)
        );

        return Client(
            sqlite3_column_int(stmt, 0),
            db_internal::ColumnText(stmt, 1),
            db_internal::ColumnText(stmt, 2),
            db_internal::ColumnText(stmt, 3),
            coords
        );
    }

    return std::nullopt;
}

std::vector<Client> Database::GetAllClients() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<Client> clients;

    const char* sql =
        "SELECT id, name, phone, address, longitude, latitude "
        "FROM clients ORDER BY id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetAllClients prepare error: " << sqlite3_errmsg(db_) << '\n';
        return clients;
    }
    db_internal::StatementGuard guard(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const Coordinates coords(
            sqlite3_column_double(stmt, 4),
            sqlite3_column_double(stmt, 5)
        );

        clients.emplace_back(
            sqlite3_column_int(stmt, 0),
            db_internal::ColumnText(stmt, 1),
            db_internal::ColumnText(stmt, 2),
            db_internal::ColumnText(stmt, 3),
            coords
        );
    }

    return clients;
}

bool Database::DeleteClient(int id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const char* sql = "DELETE FROM clients WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "DeleteClient prepare error: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "DeleteClient step error: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    return sqlite3_changes(db_) > 0;
}