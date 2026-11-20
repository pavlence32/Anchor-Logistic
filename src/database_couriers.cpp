#include "database.h"
#include "database_helpers.h"

#include <iostream>

int Database::AddCourier(const Courier& courier) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const char* sql =
        "INSERT INTO couriers (name, transport, available) VALUES (?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "AddCourier prepare error: " << sqlite3_errmsg(db_) << '\n';
        return -1;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_text(stmt, 1, courier.Get_name().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(
        stmt, 2, db_internal::TransportToDb(courier.Get_transport()),
        -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, courier.Is_available() ? 1 : 0);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        return static_cast<int>(sqlite3_last_insert_rowid(db_));
    }

    std::cerr << "AddCourier step error: " << sqlite3_errmsg(db_) << '\n';
    return -1;
}

std::optional<Courier> Database::FindFreeCourier(Transport transport) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const char* sql =
        "SELECT id, name, transport, available "
        "FROM couriers WHERE transport = ? AND available = 1 LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "FindFreeCourier prepare error: " << sqlite3_errmsg(db_) << '\n';
        return std::nullopt;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_text(
        stmt, 1, db_internal::TransportToDb(transport), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return Courier(
            sqlite3_column_int(stmt, 0),
            db_internal::ColumnText(stmt, 1),
            TransportFromString(db_internal::ColumnText(stmt, 2)),
            sqlite3_column_int(stmt, 3) != 0
        );
    }

    return std::nullopt;
}

std::optional<Courier> Database::GetCourier(int id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const char* sql =
        "SELECT id, name, transport, available FROM couriers WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetCourier prepare error: " << sqlite3_errmsg(db_) << '\n';
        return std::nullopt;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return Courier(
            sqlite3_column_int(stmt, 0),
            db_internal::ColumnText(stmt, 1),
            TransportFromString(db_internal::ColumnText(stmt, 2)),
            sqlite3_column_int(stmt, 3) != 0
        );
    }

    return std::nullopt;
}

std::vector<Courier> Database::GetAllCouriers() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<Courier> couriers;

    const char* sql =
        "SELECT id, name, transport, available FROM couriers ORDER BY id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetAllCouriers prepare error: " << sqlite3_errmsg(db_) << '\n';
        return couriers;
    }
    db_internal::StatementGuard guard(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        couriers.emplace_back(
            sqlite3_column_int(stmt, 0),
            db_internal::ColumnText(stmt, 1),
            TransportFromString(db_internal::ColumnText(stmt, 2)),
            sqlite3_column_int(stmt, 3) != 0
        );
    }

    return couriers;
}

bool Database::UpdateCourierAvailability(int courierId, bool available) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const char* sql = "UPDATE couriers SET available = ? WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "UpdateCourierAvailability prepare error: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_int(stmt, 1, available ? 1 : 0);
    sqlite3_bind_int(stmt, 2, courierId);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "UpdateCourierAvailability step error: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    return sqlite3_changes(db_) > 0;
}

bool Database::DeleteCourier(int id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const char* sql = "DELETE FROM couriers WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "DeleteCourier prepare error: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "DeleteCourier step error: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    return sqlite3_changes(db_) > 0;
}