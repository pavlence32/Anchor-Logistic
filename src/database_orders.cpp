#include "database.h"
#include "database_helpers.h"

#include <iostream>

int Database::AddOrder(const Order& order) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql =
        "INSERT INTO orders "
        "(client_id, courier_id, weight_kg, transport, status, "
        " created_at, distance_m, duration_s) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "AddOrder prepare error: " << sqlite3_errmsg(db_) << '\n';
        return -1;
    }
    db_internal::StatementGuard guard(stmt);

    const RouteResult route = order.Get_route();

    sqlite3_bind_int(stmt, 1, order.Get_client_id());
    if (order.Get_courier_id() > 0) {
        sqlite3_bind_int(stmt, 2, order.Get_courier_id());
    } else {
        sqlite3_bind_null(stmt, 2);
    }

    sqlite3_bind_double(stmt, 3, order.Get_weight_kg());
    sqlite3_bind_text(stmt, 4, toString(order.Get_transport()), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, toString(order.Get_status()), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(order.Get_created_at()));
    sqlite3_bind_double(stmt, 7, route.distance_m);
    sqlite3_bind_double(stmt, 8, route.duration_s);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        return static_cast<int>(sqlite3_last_insert_rowid(db_));
    }

    std::cerr << "AddOrder step error: " << sqlite3_errmsg(db_) << '\n';
    return -1;
}

std::optional<Order> Database::GetOrder(int id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql =
        "SELECT id, client_id, courier_id, weight_kg, transport, status, "
        "created_at, distance_m, duration_s "
        "FROM orders WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetOrder prepare error: " << sqlite3_errmsg(db_) << '\n';
        return std::nullopt;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const int courierId = sqlite3_column_type(stmt, 2) == SQLITE_NULL
                                  ? 0
                                  : sqlite3_column_int(stmt, 2);

        const RouteResult route(
            sqlite3_column_double(stmt, 7),
            sqlite3_column_double(stmt, 8)
        );

        return Order(
            sqlite3_column_int(stmt, 0),
            sqlite3_column_int(stmt, 1),
            courierId,
            sqlite3_column_double(stmt, 3),
            TransportFromString(db_internal::ColumnText(stmt, 4)),
            StatusFromString(db_internal::ColumnText(stmt, 5)),
            static_cast<std::time_t>(sqlite3_column_int64(stmt, 6)),
            route
        );
    }

    return std::nullopt;
}

std::vector<Order> Database::GetAllOrders() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Order> orders;

    const char* sql =
        "SELECT id, client_id, courier_id, weight_kg, transport, status, "
        "created_at, distance_m, duration_s "
        "FROM orders ORDER BY id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetAllOrders prepare error: " << sqlite3_errmsg(db_) << '\n';
        return orders;
    }
    db_internal::StatementGuard guard(stmt);

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
            TransportFromString(db_internal::ColumnText(stmt, 4)),
            StatusFromString(db_internal::ColumnText(stmt, 5)),
            static_cast<std::time_t>(sqlite3_column_int64(stmt, 6)),
            route
        );
    }

    return orders;
}

bool Database::UpdateOrderStatus(int orderId, OrderStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "UPDATE orders SET status = ? WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "UpdateOrderStatus prepare error: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_text(stmt, 1, toString(status), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, orderId);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "UpdateOrderStatus step error: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    return true;
}

bool Database::AssignCourierToOrder(int orderId, int courierId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql =
        "UPDATE orders SET courier_id = ?, status = ? WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "AssignCourierToOrder prepare error: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_int(stmt, 1, courierId);
    sqlite3_bind_text(stmt, 2, toString(OrderStatus::Assigned), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, orderId);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "AssignCourierToOrder step error: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    return true;
}

bool Database::DeleteOrder(int id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "DELETE FROM orders WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "DeleteOrder prepare error: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "DeleteOrder step error: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    return true;
}