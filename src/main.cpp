#include "ImGuiApp.h"

#include "GeocodingService.h"
#include "LogisticService.h"
#include "RoutingService.h"
#include "database.h"
#include "interface.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

class SqliteRepositoryAdapter final : public IRepository {
public:
    explicit SqliteRepositoryAdapter(Database& db) : db_(db) {}

    int addClient(const std::string& name, const std::string& phone,
                  const std::string& address, Coordinates coords) override {
        return db_.AddClient(Client(0, name, phone, address, coords));
    }

    bool findClient(int id, Client& out) override {
        auto found = db_.GetClient(id);
        if (!found.has_value()) {
            return false;
        }
        out = *found;
        return true;
    }

    std::vector<Courier> getCouriers() override {
        return db_.GetAllCouriers();
    }

    void setCourierAvailable(int courier_id, bool value) override {
        db_.UpdateCourierAvailability(courier_id, value);
    }

    int addOrder(const Order& order) override {
        return db_.AddOrder(order);
    }

    bool findOrder(int id, Order& out) override {
        auto found = db_.GetOrder(id);
        if (!found.has_value()) {
            return false;
        }
        out = *found;
        return true;
    }

    void updateOrderStatus(int id, OrderStatus status) override {
        db_.UpdateOrderStatus(id, status);
    }

    int addCourier(const std::string& name, Transport transport,
                   bool available) override {
        return db_.AddCourier(Courier(0, name, transport, available));
    }

    std::vector<Order> getOrdersByClient(int client_id) override {
        return db_.GetOrdersByClient(client_id);
    }

    std::vector<Order> getActiveOrders() override {
        return db_.GetActiveOrders();
    }

private:
    Database& db_;
};

} 

int main() {
    Database db("logistics.db");
    if (!db.isOpen()) {
        std::cerr << "Не удалось открыть базу данных logistics.db\n";
        return 1;
    }

    db.SeedInitialData();

    SqliteRepositoryAdapter repo(db);
    GeocodingService geocoder;
    RoutingService router;
    LogisticService logistics(repo, geocoder, router);
    SimulationEngine simulation(repo);

    ImGuiApp app(1024, 720, "Anchor Logistic", logistics, &simulation);
    if (!app.init()) {
        std::cerr << "Не удалось инициализировать интерфейс ImGui\n";
        return 1;
    }

   simulation.subscribe([&app](const StatusEvent& event) {
        app.pushStatusLog("Заказ #" + std::to_string(event.Get_order()) +
                          " | статус #" +
                          std::to_string(static_cast<int>(event.Get_oldstatus())) +
                          " -> #" +
                          std::to_string(static_cast<int>(event.Get_newstatus())));
    });

    app.run();
    simulation.stop();
    return 0;
}