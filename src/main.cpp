#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <limits>
#include "../include/database.h"
#include "../include/LogisticService.h"
#include "../include/interface.h"
#include "../include/models.h"
#include "../include/events.h"
#include "../include/GeocodingService.h"
#include "../include/RoutingService.h"

class SqliteRepositoryAdapter : public IRepository {
private:
    Database& db_;
    // Симуляция работает в отдельном потоке, поэтому все обращения к базе
    // сериализуются одним мьютексом.
    std::mutex mutex_;

public:
    explicit SqliteRepositoryAdapter(Database& db) : db_(db) {}

    int addClient(const std::string &name, const std::string &phone,
                  const std::string &address, Coordinates coords) override {
        std::lock_guard<std::mutex> lock(mutex_);
        Client client(0, name, phone, address, coords);
        return db_.AddClient(client);
    }

    bool findClient(int id, Client &out) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto clientOpt = db_.GetClient(id);
        if (clientOpt.has_value()) {
            out = clientOpt.value();
            return true;
        }
        return false;
    }

    int addCourier(const std::string &name, Transport transport, bool available) override {
        std::lock_guard<std::mutex> lock(mutex_);
        Courier courier(0, name, transport, available);
        return db_.AddCourier(courier);
    }

    std::vector<Courier> getCouriers() override {
        std::lock_guard<std::mutex> lock(mutex_);
        return db_.GetAllCouriers();
    }

    void setCourierAvailable(int courier_id, bool value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        db_.UpdateCourierAvailability(courier_id, value);
    }

    int addOrder(const Order &order) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return db_.AddOrder(order);
    }

    bool findOrder(int id, Order &out) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto orderOpt = db_.GetOrder(id);
        if (orderOpt.has_value()) {
            out = orderOpt.value();
            return true;
        }
        return false;
    }

    void updateOrderStatus(int id, OrderStatus status) override {
        std::lock_guard<std::mutex> lock(mutex_);
        db_.UpdateOrderStatus(id, status);
    }

    std::vector<Order> getOrdersByClient(int client_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return db_.GetOrdersByClient(client_id);
    }

    std::vector<Order> getActiveOrders() override {
        std::lock_guard<std::mutex> lock(mutex_);
        return db_.GetActiveOrders();
    }
};

#ifdef _WIN32
#include <windows.h>
#endif

int main() {
    #ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    #endif

    Database db("logistics.db");
    if (!db.isOpen()) {
        std::cerr << "Ошибка открытия базы данных\n";
        return 1;
    }

    if (!db.SeedInitialData()) {
        std::cerr << "Не удалось заполнить стартовые данные\n";
    }

    SqliteRepositoryAdapter repository(db);

    // Склад в Минске; координаты хранятся в формате OSRM: longitude, latitude.
    const Coordinates warehouse{27.5615, 53.9045};
    GeocodingService geocoder;
    RoutingService router;
    LogisticService logistics(repository, geocoder, router, warehouse);

    SimulationEngine simulation(repository, std::chrono::milliseconds(2000));
    simulation.subscribe([](const StatusEvent &event) {
        std::cout << "\n[СИМУЛЯЦИЯ] Заказ #" << event.Get_order() 
                  << " сменил статус: " << toString(event.Get_oldstatus()) 
                  << " -> " << toString(event.Get_newstatus()) << "\n> ";
    });
    simulation.start();
    simulation.resumeActiveOrders();

    int choice = -1;
    while (choice != 0) {
        std::cout << "1. Создать заказ\n"
                  << "2. Текущее состояние заказов\n"
                  << "0. Выход\n"
                  << "Выберите пункт: ";
        
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        if (choice == 1 || choice == 2) {
            int clientId = 0;
            Client client;
            bool clientValid = false;

            while (!clientValid) {
                std::cout << "Введите ID клиента: ";
                if (!(std::cin >> clientId)) {
                    std::cin.clear();
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    std::cout << "Некорректный ввод. Попробуйте снова.\n";
                    continue;
                }

                if (logistics.findClient(clientId, client)) {
                    std::cout << "Клиент найден: " << client.Get_name() << "\n";
                    clientValid = true;
                } else {
                    std::cout << "Клиент с ID " << clientId << " не найден в базе.\n"
                              << "Пройдите регистрацию нового клиента.\n";
                    
                    std::string name, phone, address;
                    std::cout << "Введите имя: ";
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    std::getline(std::cin, name);
                    std::cout << "Введите телефон: ";
                    std::getline(std::cin, phone);
                    std::cout << "Введите адрес: ";
                    std::getline(std::cin, address);

                    try {
                        clientId = logistics.registerClient(name, phone, address);
                        std::cout << "Клиент успешно зарегистрирован! Ваш ID: " << clientId << " (запомните его)\n";
                        if (logistics.findClient(clientId, client)) {
                            clientValid = true;
                        }
                    } catch (const std::exception& e) {
                        std::cout << "Ошибка регистрации: " << e.what() << ". Попробуйте ввести ID снова.\n";
                    }
                }
            }

            if (choice == 1) {
                double weight = 0.0;
                while (true) {
                    std::cout << "Введите вес груза (кг): ";
                    if (!(std::cin >> weight)) {
                        std::cin.clear();
                        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                        std::cout << "Некорректный ввод. Попробуйте снова.\n";
                        continue;
                    }
                    if (weight <= 0.0) {
                        std::cout << "Вес должен быть больше нуля. Попробуйте снова.\n";
                        continue;
                    }
                    break;
                }

                try {
                    Order order;
                    if (logistics.createOrder(clientId, weight, order)) {
                        std::cout << "Заказ успешно создан! ID заказа: " << order.Get_id()
                                  << ", Курьер ID: " << order.Get_courier_id()
                                  << ", Транспорт: " << toString(order.Get_transport())
                                  << ", Расстояние: " << std::fixed << std::setprecision(1)
                                  << order.Get_route().distance_m / 1000.0 << " км"
                                  << ", Время: " << std::setprecision(0)
                                  << order.Get_route().duration_s / 60.0 << " мин"
                                  << ", Статус: " << toString(order.Get_status()) << "\n";
                        
                        simulation.enqueue(order.Get_id());
                    }
                } catch (const std::exception& e) {
                    std::cout << "Ошибка создания заказа: " << e.what() << "\n";
                }
            }           
            else if (choice == 2) {
                std::vector<Order> clientOrders = repository.getOrdersByClient(clientId);

                if (clientOrders.empty()) {
                    std::cout << "У данного клиента пока нет заказов.\n";
                    continue;
                }

                std::cout << "\n-------------------------------------------------------------------------------------------------------------------\n"
                          << std::left << std::setw(6) << "ID"
                          << std::setw(12) << "Курьер ID"
                          << std::setw(12) << "Вес (кг)"
                          << std::setw(22) << "Транспорт"
                          << std::setw(15) << "Статус"
                          << std::setw(14) << "Расст. (км)"
                          << std::setw(12) << "Время (мин)" << "\n"
                          << "-------------------------------------------------------------------------------------------------------------------\n";

                for (const auto& ord : clientOrders) {
                    const RouteResult route = ord.Get_route();
                    std::cout << std::left << std::setw(6) << ord.Get_id()
                              << std::setw(12) << ord.Get_courier_id()
                              << std::setw(12) << std::fixed << std::setprecision(1) << ord.Get_weight_kg()
                              << std::setw(22) << toString(ord.Get_transport())
                              << std::setw(15) << toString(ord.Get_status())
                              << std::setw(14) << std::fixed << std::setprecision(1) << route.distance_m / 1000.0
                              << std::setw(12) << std::fixed << std::setprecision(0) << route.duration_s / 60.0 << "\n";
                }
                std::cout << "-------------------------------------------------------------------------------------------------------------------\n";
            }
        }
    }

    simulation.stop();
    return 0;
}