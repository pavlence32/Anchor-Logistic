#pragma once

#include <sqlite3.h>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "models.h"

class Database {
public:
    explicit Database(const std::string& dbPath);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool isOpen() const;

    // Клиенты.
    int AddClient(const Client& client);
    std::optional<Client> GetClient(int id);
    std::vector<Client> GetAllClients();
    bool DeleteClient(int id);

    // Транспорт.
    int AddVehicle(const Vehicle& vehicle);
    std::optional<Vehicle> GetVehicle(int id);
    std::vector<Vehicle> GetAllVehicles();
    bool UpdateVehicleAvailability(int vehicleId, bool available);
    bool DeleteVehicle(int id);

    // Курьеры.
    int AddCourier(const Courier& courier);
    std::optional<Courier> GetCourier(int id);
    std::optional<Courier> FindFreeCourier(Transport transport);
    std::vector<Courier> GetAllCouriers();
    bool UpdateCourierAvailability(int courierId, bool available);
    bool DeleteCourier(int id);

    // Заказы.
    int AddOrder(const Order& order);
    std::optional<Order> GetOrder(int id);
    std::vector<Order> GetAllOrders();
    std::vector<Order> GetOrdersByClient(int clientId);
    std::vector<Order> GetActiveOrders();
    bool UpdateOrderStatus(int orderId, OrderStatus status);
    bool AssignCourierToOrder(int orderId, int courierId);
    bool DeleteOrder(int id);

    // Маршруты.
    std::optional<RouteResult> GetRoute(int orderId);

    // Стартовые данные (транспорт и курьеры) при первом запуске.
    bool SeedInitialData();

    // Транзакции. Пока транзакция активна, остальные потоки ждут её завершения.
    bool BeginTransaction();
    bool Commit();
    bool Rollback();

private:
    sqlite3* db_ = nullptr;
    mutable std::recursive_mutex mutex_;
    std::unique_lock<std::recursive_mutex> transactionLock_;
    bool transactionActive_ = false;

    bool CreateTables();
    bool Execute(const std::string& sql);
    bool InsertRoute(int orderId, const RouteResult& route);

    static OrderStatus StatusFromString(const std::string& value);
    static Transport TransportFromString(const std::string& value);
    static Order ReadOrder(sqlite3_stmt* stmt);
};
