#pragma once

// Общие тестовые двойники для модульных тестов.
// Хранилище заменено потокобезопасной заглушкой, геокодер и маршрутизатор —
// управляемыми заглушками интерфейсов IGeocoder / IRouteProvider.

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "LogisticService.h"
#include "interface.h"
#include "models.h"

namespace test_support {

class FakeRepository final : public IRepository {
public:
    int addClient(const std::string &name, const std::string &phone,
                  const std::string &address, Coordinates coords) override {
        std::lock_guard<std::mutex> lock(mutex_);
        const int id = next_client_++;
        clients_[id] = Client(id, name, phone, address, coords);
        return id;
    }

    bool findClient(int id, Client &out) override {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = clients_.find(id);
        if (it == clients_.end())
            return false;
        out = it->second;
        return true;
    }

    std::vector<Courier> getCouriers() override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Courier> result;
        result.reserve(couriers_.size());
        for (const auto &entry : couriers_)
            result.push_back(entry.second);
        return result;
    }

    void setCourierAvailable(int courier_id, bool value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        couriers_[courier_id].Set_available(value);
    }

    int addOrder(const Order &order) override {
        std::lock_guard<std::mutex> lock(mutex_);
        const int id = next_order_++;
        Order stored = order;
        stored.Set_id(id);
        orders_[id] = stored;
        return id;
    }

    bool findOrder(int id, Order &out) override {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = orders_.find(id);
        if (it == orders_.end())
            return false;
        out = it->second;
        return true;
    }

    void updateOrderStatus(int id, OrderStatus status) override {
        std::lock_guard<std::mutex> lock(mutex_);
        orders_[id].Set_status(status);
    }

    int addCourier(const std::string &name, Transport transport,
                   bool available) override {
        std::lock_guard<std::mutex> lock(mutex_);
        const int id = next_courier_++;
        couriers_[id] = Courier(id, name, transport, available);
        return id;
    }

    std::vector<Order> getOrdersByClient(int client_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Order> result;
        for (const auto &entry : orders_) {
            if (entry.second.Get_client_id() == client_id)
                result.push_back(entry.second);
        }
        return result;
    }

    std::vector<Order> getActiveOrders() override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Order> result;
        for (const auto &entry : orders_) {
            const OrderStatus status = entry.second.Get_status();
            if (status == OrderStatus::Assigned || status == OrderStatus::InTransit)
                result.push_back(entry.second);
        }
        return result;
    }

    // Вспомогательные методы для проверок в тестах.
    Order order(int id) {
        std::lock_guard<std::mutex> lock(mutex_);
        return orders_.at(id);
    }

    Courier courier(int id) {
        std::lock_guard<std::mutex> lock(mutex_);
        return couriers_.at(id);
    }

    std::size_t orderCount() {
        std::lock_guard<std::mutex> lock(mutex_);
        return orders_.size();
    }

    std::size_t clientCount() {
        std::lock_guard<std::mutex> lock(mutex_);
        return clients_.size();
    }

private:
    std::mutex mutex_;
    std::map<int, Client> clients_;
    std::map<int, Courier> couriers_;
    std::map<int, Order> orders_;
    int next_client_{1};
    int next_courier_{1};
    int next_order_{1};
};

class StubGeocoder final : public IGeocoder {
public:
    bool geocode(const std::string &address, Coordinates &out) override {
        ++calls;
        last_address = address;
        if (!succeeds)
            return false;
        out = coordinates;
        return true;
    }

    bool succeeds{true};
    Coordinates coordinates{30.123, 50.456};
    std::string last_address;
    int calls{0};
};

class StubRouteProvider final : public IRouteProvider {
public:
    bool getRoute(Coordinates, Coordinates, RouteResult &out) override {
        ++calls;
        if (!succeeds)
            return false;
        out = route;
        return true;
    }

    bool succeeds{true};
    RouteResult route{4200.0, 600.0};
    int calls{0};
};

// Готовая связка «сервис + заглушки + один клиент» для тестов бизнес-логики.
struct Fixture {
    FakeRepository repository;
    StubGeocoder geocoder;
    StubRouteProvider router;
    LogisticService service{repository, geocoder, router,
                            Coordinates{27.5615, 53.9045}};
    int client_id{0};

    Fixture() {
        client_id = repository.addClient("Иван", "+375291112233", "Минск",
                                         Coordinates{27.6, 53.9});
    }

    int addCourier(const std::string &name, Transport transport,
                   bool available = true) {
        return repository.addCourier(name, transport, available);
    }
};

inline Order makeOrder(Transport transport, RouteResult route) {
    return Order(1, 1, 1, 10.0, transport, OrderStatus::Assigned,
                 std::time(nullptr), route);
}

}  // namespace test_support
