#include "database.h"
#include "database_helpers.h"

#include <iostream>

Database::Database(const std::string& dbPath) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

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
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (transactionActive_) {
        Execute("ROLLBACK;");
        transactionActive_ = false;
        if (transactionLock_.owns_lock()) {
            transactionLock_.unlock();
        }
    }

    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::isOpen() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
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

        CREATE TABLE IF NOT EXISTS vehicles (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            type         TEXT NOT NULL CHECK (type IN ('Car', 'Truck')),
            capacity_kg  REAL NOT NULL CHECK (capacity_kg > 0),
            speed_kmh    REAL NOT NULL CHECK (speed_kmh > 0),
            is_available INTEGER NOT NULL DEFAULT 1 CHECK (is_available IN (0, 1))
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

            FOREIGN KEY (client_id)
                REFERENCES clients(id)
                ON DELETE CASCADE,

            FOREIGN KEY (courier_id)
                REFERENCES couriers(id)
                ON DELETE SET NULL
        );

        CREATE TABLE IF NOT EXISTS routes (
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            order_id         INTEGER NOT NULL UNIQUE,
            distance_meters  REAL NOT NULL CHECK (distance_meters >= 0),
            duration_seconds REAL NOT NULL CHECK (duration_seconds >= 0),

            FOREIGN KEY (order_id)
                REFERENCES orders(id)
                ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_orders_status
            ON orders(status);

        CREATE INDEX IF NOT EXISTS idx_orders_client
            ON orders(client_id);

        CREATE INDEX IF NOT EXISTS idx_couriers_available
            ON couriers(transport, available);

        CREATE INDEX IF NOT EXISTS idx_vehicles_available
            ON vehicles(type, is_available);
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

bool Database::SeedInitialData() {
    if (!BeginTransaction()) {
        return false;
    }

    bool ok = true;

    if (GetAllVehicles().empty()) {
        ok = AddVehicle(Vehicle(0, Transport::Car, 20.0, 60.0, true)) > 0 && ok;
        ok = AddVehicle(Vehicle(0, Transport::Truck, 1000.0, 50.0, true)) > 0 && ok;
    }

    if (GetAllCouriers().empty()) {
        ok = AddCourier(Courier(0, "Иван", Transport::Car, true)) > 0 && ok;
        ok = AddCourier(Courier(0, "Петр", Transport::Truck, true)) > 0 && ok;
        ok = AddCourier(Courier(0, "Сергей", Transport::Car, true)) > 0 && ok;
        ok = AddCourier(Courier(0, "Алексей", Transport::Truck, true)) > 0 && ok;
    }

    if (!ok) {
        Rollback();
        return false;
    }

    return Commit();
}

bool Database::BeginTransaction() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (transactionActive_) {
        return false;
    }

    transactionLock_ = std::unique_lock<std::recursive_mutex>(mutex_);

    if (!Execute("BEGIN IMMEDIATE TRANSACTION;")) {
        transactionLock_.unlock();
        return false;
    }

    transactionActive_ = true;
    return true;
}

bool Database::Commit() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (!transactionActive_) {
        return false;
    }

    const bool committed = Execute("COMMIT;");
    if (!committed) {
        Execute("ROLLBACK;");
    }

    transactionActive_ = false;
    transactionLock_.unlock();
    return committed;
}

bool Database::Rollback() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (!transactionActive_) {
        return false;
    }

    const bool rolledBack = Execute("ROLLBACK;");
    transactionActive_ = false;
    transactionLock_.unlock();
    return rolledBack;
}
