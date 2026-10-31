#include "database.h"

#include <iostream>

namespace {

constexpr int kBusyTimeoutMs = 3000;

std::string ColumnText(sqlite3_stmt* stmt, int column) {
    const unsigned char* text = sqlite3_column_text(stmt, column);

    if (text == nullptr) {
        return {};
    }

    return reinterpret_cast<const char*>(text);
}

}  // namespace

Database::Database(const std::string& dbPath) {
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

    sqlite3_busy_timeout(db_, kBusyTimeoutMs);

    if (!CreateTables()) {
        std::cerr << "Failed to create database tables\n";
    }
}

Database::~Database() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::isOpen() const {
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
            transport TEXT NOT NULL CHECK (transport IN ('Car', 'Truck')),
            available INTEGER NOT NULL DEFAULT 1 CHECK (available IN (0, 1))
        );

        CREATE TABLE IF NOT EXISTS orders (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            client_id    INTEGER NOT NULL,
            courier_id   INTEGER,
            weight_kg    REAL NOT NULL CHECK (weight_kg >= 0),
            transport    TEXT NOT NULL CHECK (transport IN ('Car', 'Truck')),
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
    if (value == "Assigned") {
        return OrderStatus::Assigned;
    }

    if (value == "InTransit") {
        return OrderStatus::InTransit;
    }

    if (value == "Delivered") {
        return OrderStatus::Delivered;
    }

    if (value == "Cancelled") {
        return OrderStatus::Cancelled;
    }

    return OrderStatus::Created;
}

Transport Database::TransportFromString(const std::string& value) {
    if (value == "Truck") {
        return Transport::Truck;
    }

    return Transport::Car;
}

int Database::AddClient(const Client& client) {
    const char* sql =
        "INSERT INTO clients "
        "(name, phone, address, longitude, latitude) "
        "VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "AddClient prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return -1;
    }

    const Coordinates coords = client.Get_coords();

    sqlite3_bind_text(
        stmt, 1,
        client.Get_name().c_str(),
        -1,
        SQLITE_TRANSIENT
    );

    sqlite3_bind_text(
        stmt, 2,
        client.Get_phone().c_str(),
        -1,
        SQLITE_TRANSIENT
    );

    sqlite3_bind_text(
        stmt, 3,
        client.Get_address().c_str(),
        -1,
        SQLITE_TRANSIENT
    );

    sqlite3_bind_double(stmt, 4, coords.longitude);
    sqlite3_bind_double(stmt, 5, coords.latitude);

    int clientId = -1;

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        clientId = static_cast<int>(sqlite3_last_insert_rowid(db_));
    } else {
        std::cerr << "AddClient step error: "
                  << sqlite3_errmsg(db_) << '\n';
    }

    sqlite3_finalize(stmt);
    return clientId;
}

int Database::AddCourier(const Courier& courier) {
    const char* sql =
        "INSERT INTO couriers (name, transport, available) "
        "VALUES (?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "AddCourier prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return -1;
    }

    sqlite3_bind_text(
        stmt, 1,
        courier.Get_name().c_str(),
        -1,
        SQLITE_TRANSIENT
    );

    sqlite3_bind_text(
        stmt, 2,
        toString(courier.Get_transport()),
        -1,
        SQLITE_TRANSIENT
    );

    sqlite3_bind_int(stmt, 3, courier.Is_available() ? 1 : 0);

    int courierId = -1;

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        courierId = static_cast<int>(sqlite3_last_insert_rowid(db_));
    } else {
        std::cerr << "AddCourier step error: "
                  << sqlite3_errmsg(db_) << '\n';
    }

    sqlite3_finalize(stmt);
    return courierId;
}

int Database::AddOrder(const Order& order) {
    const char* sql =
        "INSERT INTO orders "
        "(client_id, courier_id, weight_kg, transport, status, "
        " created_at, distance_m, duration_s) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "AddOrder prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return -1;
    }

    const RouteResult route = order.Get_route();

    sqlite3_bind_int(stmt, 1, order.Get_client_id());

    if (order.Get_courier_id() > 0) {
        sqlite3_bind_int(stmt, 2, order.Get_courier_id());
    } else {
        sqlite3_bind_null(stmt, 2);
    }

    sqlite3_bind_double(stmt, 3, order.Get_weight_kg());

    sqlite3_bind_text(
        stmt, 4,
        toString(order.Get_transport()),
        -1,
        SQLITE_TRANSIENT
    );

    sqlite3_bind_text(
        stmt, 5,
        toString(order.Get_status()),
        -1,
        SQLITE_TRANSIENT
    );

    sqlite3_bind_int64(
        stmt, 6,
        static_cast<sqlite3_int64>(order.Get_created_at())
    );

    sqlite3_bind_double(stmt, 7, route.distance_m);
    sqlite3_bind_double(stmt, 8, route.duration_s);

    int orderId = -1;

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        orderId = static_cast<int>(sqlite3_last_insert_rowid(db_));
    } else {
        std::cerr << "AddOrder step error: "
                  << sqlite3_errmsg(db_) << '\n';
    }

    sqlite3_finalize(stmt);
    return orderId;
}

std::optional<Client> Database::GetClient(int id) {
    const char* sql =
        "SELECT id, name, phone, address, longitude, latitude "
        "FROM clients "
        "WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetClient prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return std::nullopt;
    }

    sqlite3_bind_int(stmt, 1, id);

    std::optional<Client> result;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const Coordinates coords(
            sqlite3_column_double(stmt, 4),
            sqlite3_column_double(stmt, 5)
        );

        result = Client(
            sqlite3_column_int(stmt, 0),
            ColumnText(stmt, 1),
            ColumnText(stmt, 2),
            ColumnText(stmt, 3),
            coords
        );
    }

    sqlite3_finalize(stmt);
    return result;
}

std::optional<Courier> Database::GetCourier(int id) {
    const char* sql =
        "SELECT id, name, transport, available "
        "FROM couriers "
        "WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetCourier prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return std::nullopt;
    }

    sqlite3_bind_int(stmt, 1, id);

    std::optional<Courier> result;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = Courier(
            sqlite3_column_int(stmt, 0),
            ColumnText(stmt, 1),
            TransportFromString(ColumnText(stmt, 2)),
            sqlite3_column_int(stmt, 3) != 0
        );
    }

    sqlite3_finalize(stmt);
    return result;
}

std::optional<Order> Database::GetOrder(int id) {
    const char* sql =
        "SELECT id, client_id, courier_id, weight_kg, transport, status, "
        "created_at, distance_m, duration_s "
        "FROM orders "
        "WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetOrder prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return std::nullopt;
    }

    sqlite3_bind_int(stmt, 1, id);

    std::optional<Order> result;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const int courierId = sqlite3_column_type(stmt, 2) == SQLITE_NULL
                                  ? 0
                                  : sqlite3_column_int(stmt, 2);

        const RouteResult route(
            sqlite3_column_double(stmt, 7),
            sqlite3_column_double(stmt, 8)
        );

        result = Order(
            sqlite3_column_int(stmt, 0),
            sqlite3_column_int(stmt, 1),
            courierId,
            sqlite3_column_double(stmt, 3),
            TransportFromString(ColumnText(stmt, 4)),
            StatusFromString(ColumnText(stmt, 5)),
            static_cast<std::time_t>(sqlite3_column_int64(stmt, 6)),
            route
        );
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<Client> Database::GetAllClients() {
    std::vector<Client> clients;

    const char* sql =
        "SELECT id, name, phone, address, longitude, latitude "
        "FROM clients "
        "ORDER BY id;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetAllClients prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return clients;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const Coordinates coords(
            sqlite3_column_double(stmt, 4),
            sqlite3_column_double(stmt, 5)
        );

        clients.emplace_back(
            sqlite3_column_int(stmt, 0),
            ColumnText(stmt, 1),
            ColumnText(stmt, 2),
            ColumnText(stmt, 3),
            coords
        );
    }

    sqlite3_finalize(stmt);
    return clients;
}

std::vector<Courier> Database::GetAllCouriers() {
    std::vector<Courier> couriers;

    const char* sql =
        "SELECT id, name, transport, available "
        "FROM couriers "
        "ORDER BY id;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetAllCouriers prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return couriers;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        couriers.emplace_back(
            sqlite3_column_int(stmt, 0),
            ColumnText(stmt, 1),
            TransportFromString(ColumnText(stmt, 2)),
            sqlite3_column_int(stmt, 3) != 0
        );
    }

    sqlite3_finalize(stmt);
    return couriers;
}

std::vector<Order> Database::GetAllOrders() {
    std::vector<Order> orders;

    const char* sql =
        "SELECT id, client_id, courier_id, weight_kg, transport, status, "
        "created_at, distance_m, duration_s "
        "FROM orders "
        "ORDER BY id;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetAllOrders prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return orders;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const int courierId = sqlite3_column_type(stmt, 2) == SQLITE_NULL
                                  ? 0
                                  : sqlite3_column_int(stmt, 2);

        const RouteResult route(
            sqlite3_column_double(stmt, 7),
            sqlite3_column_double(stmt, 8)
        );

        orders.emplace_back(
            sqlite3_column_int(stmt, 0),
            sqlite3_column_int(stmt, 1),
            courierId,
            sqlite3_column_double(stmt, 3),
            TransportFromString(ColumnText(stmt, 4)),
            StatusFromString(ColumnText(stmt, 5)),
            static_cast<std::time_t>(sqlite3_column_int64(stmt, 6)),
            route
        );
    }

    sqlite3_finalize(stmt);
    return orders;
}

bool Database::UpdateOrderStatus(int orderId, OrderStatus status) {
    const char* sql =
        "UPDATE orders "
        "SET status = ? "
        "WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "UpdateOrderStatus prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    sqlite3_bind_text(
        stmt, 1,
        toString(status),
        -1,
        SQLITE_TRANSIENT
    );

    sqlite3_bind_int(stmt, 2, orderId);

    const bool success = sqlite3_step(stmt) == SQLITE_DONE;

    if (!success) {
        std::cerr << "UpdateOrderStatus step error: "
                  << sqlite3_errmsg(db_) << '\n';
    }

    sqlite3_finalize(stmt);
    return success;
}

bool Database::AssignCourierToOrder(int orderId, int courierId) {
    const char* sql =
        "UPDATE orders "
        "SET courier_id = ?, status = ? "
        "WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "AssignCourierToOrder prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    sqlite3_bind_int(stmt, 1, courierId);

    sqlite3_bind_text(
        stmt, 2,
        toString(OrderStatus::Assigned),
        -1,
        SQLITE_TRANSIENT
    );

    sqlite3_bind_int(stmt, 3, orderId);

    const bool success = sqlite3_step(stmt) == SQLITE_DONE;

    if (!success) {
        std::cerr << "AssignCourierToOrder step error: "
                  << sqlite3_errmsg(db_) << '\n';
    }

    sqlite3_finalize(stmt);
    return success;
}

bool Database::UpdateCourierAvailability(int courierId, bool available) {
    const char* sql =
        "UPDATE couriers "
        "SET available = ? "
        "WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "UpdateCourierAvailability prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    sqlite3_bind_int(stmt, 1, available ? 1 : 0);
    sqlite3_bind_int(stmt, 2, courierId);

    const bool success = sqlite3_step(stmt) == SQLITE_DONE;

    if (!success) {
        std::cerr << "UpdateCourierAvailability step error: "
                  << sqlite3_errmsg(db_) << '\n';
    }

    sqlite3_finalize(stmt);
    return success;
}

bool Database::DeleteClient(int id) {
    const char* sql =
        "DELETE FROM clients "
        "WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "DeleteClient prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    sqlite3_bind_int(stmt, 1, id);

    const bool success = sqlite3_step(stmt) == SQLITE_DONE;

    if (!success) {
        std::cerr << "DeleteClient step error: "
                  << sqlite3_errmsg(db_) << '\n';
    }

    sqlite3_finalize(stmt);
    return success;
}

bool Database::DeleteCourier(int id) {
    const char* sql =
        "DELETE FROM couriers "
        "WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "DeleteCourier prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    sqlite3_bind_int(stmt, 1, id);

    const bool success = sqlite3_step(stmt) == SQLITE_DONE;

    if (!success) {
        std::cerr << "DeleteCourier step error: "
                  << sqlite3_errmsg(db_) << '\n';
    }

    sqlite3_finalize(stmt);
    return success;
}

bool Database::DeleteOrder(int id) {
    const char* sql =
        "DELETE FROM orders "
        "WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "DeleteOrder prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    sqlite3_bind_int(stmt, 1, id);

    const bool success = sqlite3_step(stmt) == SQLITE_DONE;

    if (!success) {
        std::cerr << "DeleteOrder step error: "
                  << sqlite3_errmsg(db_) << '\n';
    }

    sqlite3_finalize(stmt);
    return success;
}