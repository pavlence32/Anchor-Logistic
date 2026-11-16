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

    int AddVehicle(const Vehicle& vehicle);
    std::optional<Vehicle> GetVehicle(int id);
    std::vector<Vehicle> GetAllVehicles();

    int AddCourier(const Courier& courier);
    std::optional<Courier> GetCourier(int id);
    std::optional<Courier> FindFreeCourier(int vehicleId);
    std::vector<Courier> GetAllCouriers();
    bool UpdateCourierAvailability(int courierId, bool available);
    bool DeleteCourier(int id);

    int AddOrder(const Order& order, double distanceMeters, double durationSeconds);
    std::optional<Order> GetOrder(int id);
    std::vector<Order> GetAllOrders();
    bool UpdateOrderStatus(int orderId, const std::string& status);
    bool AssignCourierToOrder(int orderId, int courierId);
    bool DeleteOrder(int id);

private:
    sqlite3* db_ = nullptr;
    mutable std::mutex mutex_;

    bool CreateTables();
    bool Execute(const std::string& sql);
};