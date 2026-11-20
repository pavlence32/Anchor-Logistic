#include "database.h"
#include "database_helpers.h"

#include <iostream>

namespace {

const char* kOrderSelect =
    "SELECT o.id, o.client_id, o.courier_id, o.weight_kg, o.transport, "
    "o.status, o.created_at, "
    "COALESCE(r.distance_meters, 0) AS distance_m, "
    "COALESCE(r.duration_seconds, 0) AS duration_s "
    "FROM orders o "
    "LEFT JOIN routes r ON r.order_id = o.id ";

}  // namespace

Order Database::ReadOrder(sqlite3_stmt* stmt) {
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

bool Database::InsertRoute(int orderId, const RouteResult& route) {
    const char* sql =
        "INSERT INTO routes (order_id, distance_meters, duration_seconds) "
        "VALUES (?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "InsertRoute prepare error: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_int(stmt, 1, orderId);
    sqlite3_bind_double(stmt, 2, route.distance_m);
    sqlite3_bind_double(stmt, 3, route.duration_s);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "InsertRoute step error: " << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    return true;
}

int Database::AddOrder(const Order& order) {
    if (!BeginTransaction()) {
        std::cerr << "AddOrder: cannot start transaction\n";
        return -1;
    }

    int orderId = -1;

    {
        const char* sql =
            "INSERT INTO orders "
            "(client_id, courier_id, weight_kg, transport, status, created_at) "
            "VALUES (?, ?, ?, ?, ?, ?);";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "AddOrder prepare error: " << sqlite3_errmsg(db_) << '\n';
            Rollback();
            return -1;
        }
        db_internal::StatementGuard guard(stmt);

        sqlite3_bind_int(stmt, 1, order.Get_client_id());

        if (order.Get_courier_id() > 0) {
            sqlite3_bind_int(stmt, 2, order.Get_courier_id());
        } else {
            sqlite3_bind_null(stmt, 2);
        }

        sqlite3_bind_double(stmt, 3, order.Get_weight_kg());
        sqlite3_bind_text(
            stmt, 4, db_internal::TransportToDb(order.Get_transport()),
            -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(
            stmt, 5, db_internal::StatusToDb(order.Get_status()),
            -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(
            stmt, 6, static_cast<sqlite3_int64>(order.Get_created_at()));

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "AddOrder step error: " << sqlite3_errmsg(db_) << '\n';
            Rollback();
            return -1;
        }

        orderId = static_cast<int>(sqlite3_last_insert_rowid(db_));
    }

    if (!InsertRoute(orderId, order.Get_route())) {
        Rollback();
        return -1;
    }

    if (!Commit()) {
        return -1;
    }

    return orderId;
}

std::optional<Order> Database::GetOrder(int id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const std::string sql = std::string(kOrderSelect) + "WHERE o.id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetOrder prepare error: " << sqlite3_errmsg(db_) << '\n';
        return std::nullopt;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return ReadOrder(stmt);
    }

    return std::nullopt;
}

std::vector<Order> Database::GetAllOrders() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<Order> orders;

    const std::string sql = std::string(kOrderSelect) + "ORDER BY o.id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetAllOrders prepare error: " << sqlite3_errmsg(db_) << '\n';
        return orders;
    }
    db_internal::StatementGuard guard(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        orders.push_back(ReadOrder(stmt));
    }

    return orders;
}

std::vector<Order> Database::GetOrdersByClient(int clientId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<Order> orders;

    const std::string sql =
        std::string(kOrderSelect) + "WHERE o.client_id = ? ORDER BY o.id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetOrdersByClient prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return orders;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_int(stmt, 1, clientId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        orders.push_back(ReadOrder(stmt));
    }

    return orders;
}

std::vector<Order> Database::GetActiveOrders() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<Order> orders;

    const std::string sql =
        std::string(kOrderSelect) +
        "WHERE o.status IN ('Assigned', 'InTransit') ORDER BY o.id;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetActiveOrders prepare error: " << sqlite3_errmsg(db_) << '\n';
        return orders;
    }
    db_internal::StatementGuard guard(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        orders.push_back(ReadOrder(stmt));
    }

    return orders;
}

bool Database::UpdateOrderStatus(int orderId, OrderStatus status) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const char* sql = "UPDATE orders SET status = ? WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "UpdateOrderStatus prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return false;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_text(
        stmt, 1, db_internal::StatusToDb(status), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, orderId);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "UpdateOrderStatus step error: "
                  << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    return sqlite3_changes(db_) > 0;
}

bool Database::AssignCourierToOrder(int orderId, int courierId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const char* sql =
        "UPDATE orders SET courier_id = ?, status = ? WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "AssignCourierToOrder prepare error: "
                  << sqlite3_errmsg(db_) << '\n';
        return false;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_int(stmt, 1, courierId);
    sqlite3_bind_text(
        stmt, 2, db_internal::StatusToDb(OrderStatus::Assigned),
        -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, orderId);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "AssignCourierToOrder step error: "
                  << sqlite3_errmsg(db_) << '\n';
        return false;
    }

    return sqlite3_changes(db_) > 0;
}

bool Database::DeleteOrder(int id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

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

    return sqlite3_changes(db_) > 0;
}

std::optional<RouteResult> Database::GetRoute(int orderId) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const char* sql =
        "SELECT distance_meters, duration_seconds "
        "FROM routes WHERE order_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "GetRoute prepare error: " << sqlite3_errmsg(db_) << '\n';
        return std::nullopt;
    }
    db_internal::StatementGuard guard(stmt);

    sqlite3_bind_int(stmt, 1, orderId);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return RouteResult(
            sqlite3_column_double(stmt, 0),
            sqlite3_column_double(stmt, 1)
        );
    }

    return std::nullopt;
}
