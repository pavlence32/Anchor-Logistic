#pragma once

#include <sqlite3.h>

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

    int AddClient(const Client& client);
    int AddCourier(const Courier& courier);
    int AddOrder(const Order& order);

    std::optional<Client> GetClient(int id);
    std::optional<Courier> GetCourier(int id);
    std::optional<Order> GetOrder(int id);

    std::vector<Client> GetAllClients();
    std::vector<Courier> GetAllCouriers();
    std::vector<Order> GetAllOrders();

    bool UpdateOrderStatus(int orderId, OrderStatus status);
    bool AssignCourierToOrder(int orderId, int courierId);
    bool UpdateCourierAvailability(int courierId, bool available);

    bool DeleteClient(int id);
    bool DeleteCourier(int id);
    bool DeleteOrder(int id);

private:
    sqlite3* db_ = nullptr;

    bool CreateTables();
    bool Execute(const std::string& sql);

    static OrderStatus StatusFromString(const std::string& value);
    static Transport TransportFromString(const std::string& value);
};