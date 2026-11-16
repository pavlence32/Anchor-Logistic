#pragma once

#include <sqlite3.h>
#include <optional>
#include <string>
#include <vector>
#include <mutex>

#include "models.h"

class Database {
public:
    explicit Database(const std::string& dbPath);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool isOpen() const;

    int AddClient(const Client& client);
    std::optional<Client> GetClient(int id);
    std::vector<Client> GetAllClients();
    bool DeleteClient(int id);

    int AddCourier(const Courier& courier);
    std::optional<Courier> GetCourier(int id);
    std::optional<Courier> FindFreeCourier(Transport transport);
    std::vector<Courier> GetAllCouriers();
    bool UpdateCourierAvailability(int courierId, bool available);
    bool DeleteCourier(int id);

    int AddOrder(const Order& order);
    std::optional<Order> GetOrder(int id);
    std::vector<Order> GetAllOrders();
    std::vector<Order> GetOrdersByClient(int clientId);
    std::vector<Order> GetActiveOrders();
    bool UpdateOrderStatus(int orderId, OrderStatus status);
    bool AssignCourierToOrder(int orderId, int courierId);
    bool DeleteOrder(int id);

private:
    sqlite3* db_ = nullptr;
    mutable std::mutex mutex_;

    bool CreateTables();
    bool Execute(const std::string& sql);

    static OrderStatus StatusFromString(const std::string& value);
    static Transport TransportFromString(const std::string& value);
};
