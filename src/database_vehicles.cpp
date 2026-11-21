#include "database.h"
#include "database_helpers.h"

#include <iostream>

int Database::AddVehicle(const Vehicle& vehicle) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const char* sql =
        "INSERT INTO vehicles (type, capacity_kg, speed_kmh, is_available) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "AddVehicle prepare error: " << sqlite3_errmsg(db_) << '\n';
        return -1;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_text(
        stmt, 1, db_internal::TransportToDb(vehicle.Get_type()),
        -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, vehicle.Get_capacity_kg());
    sqlite3_bind_double(stmt, 3, vehicle.Get_speed_kmh());
    sqlite3_bind_int(stmt, 4, vehicle.Is_available() ? 1 : 0);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        return static_cast<int>(sqlite3_last_insert_rowid(db_));
    }

    std::cerr << "AddVehicle step error: " << sqlite3_errmsg(db_) << '\n';
    return -1;
}

std::optional<Vehicle> Database::GetVehicle(int id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const char* sql =
        "SELECT id, type, capacity_kg, speed_kmh, is_available "
        "FROM vehicles WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetVehicle prepare error: " << sqlite3_errmsg(db_) << '\n';
        return std::nullopt;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return Vehicle(
            sqlite3_column_int(stmt, 0),
            TransportFromString(db_internal::ColumnText(stmt, 1)),
            sqlite3_column_double(stmt, 2),
            sqlite3_column_double(stmt, 3),
            sqlite3_column_int(stmt, 4) != 0
        );
    }

    return std::nullopt;
}

std::vector<Vehicle> Database::GetAllVehicles() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<Vehicle> vehicles;

    const char* sql =
        "SELECT id, type, capacity_kg, speed_kmh, is_available "
        "FROM vehicles ORDER BY id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetAllVehicles prepare error: " << sqlite3_errmsg(db_) << '\n';
        return vehicles;
    }
    db_internal::StatementGuard guard(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        vehicles.emplace_back(
            sqlite3_column_int(stmt, 0),
            TransportFromString(db_internal::ColumnText(stmt, 1)),
            sqlite3_column_double(stmt, 2),
            sqlite3_column_double(stmt, 3),
            sqlite3_column_int(stmt, 4) != 0
        );
    }

    return vehicles;
}

bool Database::UpdateVehicleAvailability(int vehicleId, bool available) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const char* sql = "UPDATE vehicles SET is_available = ? WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "UpdateVehicleAvailability prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return false;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_int(stmt, 1, available ? 1 : 0);
    sqlite3_bind_int(stmt, 2, vehicleId);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "UpdateVehicleAvailability step error: "
                  << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    return sqlite3_changes(db_) > 0;
}

bool Database::DeleteVehicle(int id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const char* sql = "DELETE FROM vehicles WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "DeleteVehicle prepare error: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "DeleteVehicle step error: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    return sqlite3_changes(db_) > 0;
}
