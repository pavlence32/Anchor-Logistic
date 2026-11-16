#include "database.h"
#include "database_helpers.h"

#include <iostream>

Database::Database(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "Cannot open database: "
                  << (db_ != nullptr ? sqlite3_errmsg(db_) : "unknown error")
                  << '\n';

        if (db_ != nullptr) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return;
    }

    sqlite3_busy_timeout(db_, db_internal::kBusyTimeoutMs);

    if (!CreateTables()) {
        std::cerr << "Failed to create database tables\n";
    }
}

Database::~Database() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::isOpen() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return db_ != nullptr;
}

bool Database::Execute(const std::string& sql) {
    char* errorMessage = nullptr;

    const int result = sqlite3_exec(
        db_,
        sql.c_str(),
        nullptr,
        nullptr,
        &errorMessage
    );

    if (result != SQLITE_OK) {
        std::cerr << "SQL error: "
                  << (errorMessage != nullptr ? errorMessage : "unknown error")
                  << '\n';

        sqlite3_free(errorMessage);
        return false;
    }

    return true;
}

bool Database::CreateTables() {
    const char* schema = R"SQL(
        PRAGMA foreign_keys = ON;

        CREATE TABLE IF NOT EXISTS clients (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            name      TEXT NOT NULL,
            phone     TEXT NOT NULL,
            address   TEXT NOT NULL,
            longitude REAL NOT NULL,
            latitude  REAL NOT NULL
        );

        CREATE TABLE IF NOT EXISTS couriers (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            name      TEXT NOT NULL,
            transport TEXT NOT NULL CHECK (transport IN ('Car', 'Truck', 'Plane')),
            available INTEGER NOT NULL DEFAULT 1 CHECK (available IN (0, 1))
        );

        CREATE TABLE IF NOT EXISTS orders (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            client_id    INTEGER NOT NULL,
            courier_id   INTEGER,
            weight_kg    REAL NOT NULL CHECK (weight_kg >= 0),
            transport    TEXT NOT NULL CHECK (transport IN ('Car', 'Truck', 'Plane')),
            status       TEXT NOT NULL CHECK (
                status IN (
                    'Created',
                    'Assigned',
                    'InTransit',
                    'Delivered',
                    'Cancelled'
                )
            ),
            created_at   INTEGER NOT NULL,
            distance_m   REAL NOT NULL DEFAULT 0,
            duration_s   REAL NOT NULL DEFAULT 0,

            FOREIGN KEY (client_id)
                REFERENCES clients(id)
                ON DELETE CASCADE,

            FOREIGN KEY (courier_id)
                REFERENCES couriers(id)
                ON DELETE SET NULL
        );

        CREATE INDEX IF NOT EXISTS idx_orders_status
            ON orders(status);

        CREATE INDEX IF NOT EXISTS idx_orders_client
            ON orders(client_id);

        CREATE INDEX IF NOT EXISTS idx_couriers_available
            ON couriers(transport, available);
    )SQL";

    return Execute(schema);
}

OrderStatus Database::StatusFromString(const std::string& value) {
    if (value == "Assigned") return OrderStatus::Assigned;
    if (value == "InTransit") return OrderStatus::InTransit;
    if (value == "Delivered") return OrderStatus::Delivered;
    if (value == "Cancelled") return OrderStatus::Cancelled;
    return OrderStatus::Created;
}

Transport Database::TransportFromString(const std::string& value) {
    if (value == "Truck") return Transport::Truck;
    return Transport::Car;
}
