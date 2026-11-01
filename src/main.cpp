#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <chrono>
#include "../include/database.h"
#include "../include/LogisticService.h"
#include "../include/interface.h"
#include "../include/models.h"
#include "../include/events.h"

class SqliteRepositoryAdapter : public IRepository {
private:
    Database& db_;

public:
    explicit SqliteRepositoryAdapter(Database& db) : db_(db) {}

    int addClient(const std::string &name, const std::string &phone,
                  const std::string &address, Coordinates coords) override {
        Client client(0, name, phone, address, coords);
        return db_.AddClient(client);
    }

    bool findClient(int id, Client &out) override {
        auto clientOpt = db_.GetClient(id);
        if (clientOpt.has_value()) {
            out = clientOpt.value();
            return true;
        }
        return false;
    }

    std::vector<Courier> getCouriers() override {
        return db_.GetAllCouriers();
    }

    void setCourierAvailable(int courier_id, bool value) override {
        db_.UpdateCourierAvailability(courier_id, value);
    }

    int addOrder(const Order &order) override {
        return db_.AddOrder(order);
    }

    bool findOrder(int id, Order &out) override {
        auto orderOpt = db_.GetOrder(id);
        if (orderOpt.has_value()) {
            out = orderOpt.value();
            return true;
        }
        return false;
    }

    void updateOrderStatus(int id, OrderStatus status) override {
        db_.UpdateOrderStatus(id, status);
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

    if (db.GetAllCouriers().empty()) {
        db.AddCourier(Courier(0, "Иван", Transport::Car, true));
        db.AddCourier(Courier(0, "Петр", Transport::Truck, true));
    }

    SqliteRepositoryAdapter repository(db);
    FixedGeocoder geocoder(Coordinates{27.5615, 53.9045});
    FixedRouteProvider router(RouteResult{5000.0, 600.0});
    LogisticService logistics(repository, geocoder, router, Coordinates{27.5615, 53.9045});

    SimulationEngine simulation(repository, std::chrono::milliseconds(2000));
    simulation.subscribe([](const StatusEvent &event) {
        std::cout << "\n[СИМУЛЯЦИЯ] Заказ #" << event.Get_order() 
                  << " сменил статус: " << toString(event.Get_oldstatus()) 
                  << " -> " << toString(event.Get_newstatus()) << "\n> ";
    });
    simulation.start();

    int choice = -1;
    while (choice != 0) {
        std::cout << "1. Создать заказ\n"
                  << "2. Текущее состояние заказов\n"
                  << "0. Выход\n"
                  << "Выберите пункт: ";
        
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
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
                    std::cin.ignore(10000, '\n');
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
                    std::cin.ignore();
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
                std::cout << "Введите вес груза (кг): ";
                std::cin >> weight;

                try {
                    Order order;
                    if (logistics.createOrder(clientId, weight, order)) {
                        std::cout << "Заказ успешно создан! ID заказа: " << order.Get_id() 
                                  << ", Курьер ID: " << order.Get_courier_id()
                                  << ", Транспорт: " << toString(order.Get_transport())
                                  << ", Статус: " << toString(order.Get_status()) << "\n";
                        
                        simulation.enqueue(order.Get_id());
                    }
                } catch (const std::exception& e) {
                    std::cout << "Ошибка создания заказа: " << e.what() << "\n";
                }
            } else if (choice == 2) {
                std::vector<Order> allOrders = db.GetAllOrders();
                std::vector<Order> clientOrders;
                for (const auto& ord : allOrders) {
                    if (ord.Get_client_id() == clientId) {
                        clientOrders.push_back(ord);
                    }
                }

                if (clientOrders.empty()) {
                    std::cout << "У данного клиента пока нет заказов.\n";
                    continue;
                }

                std::cout << "\n-------------------------------------------------------------------------\n"
                          << std::left << std::setw(6) << "ID"
                          << std::setw(12) << "Курьер ID"
                          << std::setw(14) << "Вес (кг)"
                          << std::setw(20) << "Транспорт"
                          << std::setw(15) << "Статус" << "\n"
                          << "-------------------------------------------------------------------------\n";

                for (const auto& ord : clientOrders) {
                    std::cout << std::left << std::setw(6) << ord.Get_id()
                              << std::setw(12) << ord.Get_courier_id()
                              << std::setw(14) << ord.Get_weight_kg()
                              << std::setw(20) << toString(ord.Get_transport())
                              << std::setw(15) << toString(ord.Get_status()) << "\n";
                }
                std::cout << "-------------------------------------------------------------------------\n";
            }
        }
    }

    simulation.stop();
    return 0;
}