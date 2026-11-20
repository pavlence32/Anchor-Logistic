#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "database.h"

using Catch::Approx;

namespace {

Client makeClient(const std::string& name = "Иван",
                  const std::string& phone = "+375291112233",
                  const std::string& address = "Минск, ул. Ленина 1",
                  double longitude = 27.5615,
                  double latitude = 53.9045) {
    return Client(0, name, phone, address,
                  Coordinates{longitude, latitude});
}

Courier makeCourier(const std::string& name, Transport transport,
                    bool available = true) {
    return Courier(0, name, transport, available);
}

Vehicle makeVehicle(Transport type, double capacityKg = 20.0,
                    double speedKmh = 60.0, bool available = true) {
    return Vehicle(0, type, capacityKg, speedKmh, available);
}

Order makeOrder(int clientId, int courierId, double weightKg,
                Transport transport, OrderStatus status, RouteResult route) {
    return Order(0, clientId, courierId, weightKg, transport, status,
                 std::time(nullptr), route);
}

std::filesystem::path makeTempDbPath() {
    static std::atomic<int> counter{0};
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();

    return std::filesystem::temp_directory_path() /
           ("anchor_logistic_db_test_" + std::to_string(stamp) + "_" +
            std::to_string(counter.fetch_add(1)) + ".db");
}

void removeDbFiles(const std::filesystem::path& path) {
    std::filesystem::remove(path);
    std::filesystem::remove(path.string() + "-journal");
    std::filesystem::remove(path.string() + "-wal");
    std::filesystem::remove(path.string() + "-shm");
}

}  // namespace

TEST_CASE("Database открывает соединение и создаёт пустую схему",
          "[database]") {
    Database db(":memory:");

    REQUIRE(db.isOpen());
    CHECK(db.GetAllClients().empty());
    CHECK(db.GetAllVehicles().empty());
    CHECK(db.GetAllCouriers().empty());
    CHECK(db.GetAllOrders().empty());
}

TEST_CASE("Некорректный путь к базе данных не открывает соединение",
          "[database]") {
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("anchor_logistic_missing_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));

    std::filesystem::remove_all(directory);

    Database db((directory / "logistics.db").string());
    CHECK_FALSE(db.isOpen());

    std::filesystem::remove_all(directory);
}

TEST_CASE("CRUD клиентов", "[database]") {
    Database db(":memory:");
    REQUIRE(db.isOpen());

    const int first = db.AddClient(
        makeClient("Иван", "+375291112233", "Минск, ул. Ленина 1",
                   27.5615, 53.9045));
    const int second = db.AddClient(
        makeClient("Мария", "+375331234567", "Гомель, ул. Советская 2",
                   30.9930, 52.4340));

    REQUIRE(first > 0);
    REQUIRE(second > 0);
    CHECK(first != second);

    const auto saved = db.GetClient(first);
    REQUIRE(saved.has_value());
    CHECK(saved->Get_id() == first);
    CHECK(saved->Get_name() == "Иван");
    CHECK(saved->Get_phone() == "+375291112233");
    CHECK(saved->Get_address() == "Минск, ул. Ленина 1");
    CHECK(saved->Get_coords().longitude == Approx(27.5615));
    CHECK(saved->Get_coords().latitude == Approx(53.9045));

    CHECK(db.GetAllClients().size() == 2);
    CHECK_FALSE(db.GetClient(9999).has_value());

    CHECK(db.DeleteClient(first));
    CHECK_FALSE(db.GetClient(first).has_value());
    CHECK(db.GetAllClients().size() == 1);
    CHECK_FALSE(db.DeleteClient(first));
}

TEST_CASE("CRUD транспорта", "[database]") {
    Database db(":memory:");
    REQUIRE(db.isOpen());

    const int car = db.AddVehicle(makeVehicle(Transport::Car, 20.0, 60.0));
    const int truck =
        db.AddVehicle(makeVehicle(Transport::Truck, 1000.0, 50.0, false));

    REQUIRE(car > 0);
    REQUIRE(truck > 0);

    const auto savedCar = db.GetVehicle(car);
    REQUIRE(savedCar.has_value());
    CHECK(savedCar->Get_id() == car);
    CHECK(savedCar->Get_type() == Transport::Car);
    CHECK(savedCar->Get_capacity_kg() == Approx(20.0));
    CHECK(savedCar->Get_speed_kmh() == Approx(60.0));
    CHECK(savedCar->Is_available());

    const auto savedTruck = db.GetVehicle(truck);
    REQUIRE(savedTruck.has_value());
    CHECK(savedTruck->Get_type() == Transport::Truck);
    CHECK(savedTruck->Get_capacity_kg() == Approx(1000.0));
    CHECK(savedTruck->Get_speed_kmh() == Approx(50.0));
    CHECK_FALSE(savedTruck->Is_available());

    CHECK(db.GetAllVehicles().size() == 2);
    CHECK_FALSE(db.GetVehicle(9999).has_value());

    CHECK(db.UpdateVehicleAvailability(truck, true));
    CHECK(db.GetVehicle(truck)->Is_available());
    CHECK_FALSE(db.UpdateVehicleAvailability(9999, true));

    CHECK(db.DeleteVehicle(car));
    CHECK_FALSE(db.GetVehicle(car).has_value());
    CHECK(db.GetAllVehicles().size() == 1);
    CHECK_FALSE(db.DeleteVehicle(car));
}

TEST_CASE("CRUD курьеров и поиск свободного курьера", "[database]") {
    Database db(":memory:");
    REQUIRE(db.isOpen());

    const int freeCar = db.AddCourier(makeCourier("Иван", Transport::Car));
    const int busyCar =
        db.AddCourier(makeCourier("Сергей", Transport::Car, false));
    const int truck = db.AddCourier(makeCourier("Петр", Transport::Truck));

    REQUIRE(freeCar > 0);
    REQUIRE(busyCar > 0);
    REQUIRE(truck > 0);

    const auto saved = db.GetCourier(truck);
    REQUIRE(saved.has_value());
    CHECK(saved->Get_name() == "Петр");
    CHECK(saved->Get_transport() == Transport::Truck);
    CHECK(saved->Is_available());

    CHECK(db.GetAllCouriers().size() == 3);
    CHECK_FALSE(db.GetCourier(9999).has_value());

    const auto foundCar = db.FindFreeCourier(Transport::Car);
    REQUIRE(foundCar.has_value());
    CHECK(foundCar->Get_id() == freeCar);

    CHECK(db.UpdateCourierAvailability(freeCar, false));
    CHECK_FALSE(db.FindFreeCourier(Transport::Car).has_value());

    CHECK(db.UpdateCourierAvailability(busyCar, true));
    const auto nextCar = db.FindFreeCourier(Transport::Car);
    REQUIRE(nextCar.has_value());
    CHECK(nextCar->Get_id() == busyCar);

    CHECK_FALSE(db.UpdateCourierAvailability(9999, true));

    CHECK(db.DeleteCourier(truck));
    CHECK_FALSE(db.GetCourier(truck).has_value());
    CHECK_FALSE(db.DeleteCourier(truck));
}

TEST_CASE("Стартовые транспорт и курьеры создаются один раз", "[database]") {
    Database db(":memory:");
    REQUIRE(db.isOpen());

    REQUIRE(db.SeedInitialData());

    const auto vehicles = db.GetAllVehicles();
    REQUIRE(vehicles.size() == 2);
    CHECK(vehicles[0].Get_type() == Transport::Car);
    CHECK(vehicles[0].Get_capacity_kg() == Approx(20.0));
    CHECK(vehicles[0].Get_speed_kmh() == Approx(60.0));
    CHECK(vehicles[0].Is_available());
    CHECK(vehicles[1].Get_type() == Transport::Truck);
    CHECK(vehicles[1].Get_capacity_kg() == Approx(1000.0));
    CHECK(vehicles[1].Get_speed_kmh() == Approx(50.0));

    const auto couriers = db.GetAllCouriers();
    REQUIRE(couriers.size() == 4);
    CHECK(std::count_if(couriers.begin(), couriers.end(),
                        [](const Courier& courier) {
                            return courier.Get_transport() == Transport::Car;
                        }) == 2);
    CHECK(std::count_if(couriers.begin(), couriers.end(),
                        [](const Courier& courier) {
                            return courier.Get_transport() == Transport::Truck;
                        }) == 2);

    REQUIRE(db.SeedInitialData());
    CHECK(db.GetAllVehicles().size() == 2);
    CHECK(db.GetAllCouriers().size() == 4);
}

TEST_CASE("Заказ и его маршрут сохраняются и читаются", "[database]") {
    Database db(":memory:");
    REQUIRE(db.isOpen());

    const int client = db.AddClient(makeClient());
    const int courier = db.AddCourier(makeCourier("Иван", Transport::Car));
    REQUIRE(client > 0);
    REQUIRE(courier > 0);

    const RouteResult route{5400.0, 720.0};
    const int order = db.AddOrder(makeOrder(client, courier, 12.5,
                                            Transport::Car,
                                            OrderStatus::Assigned, route));
    REQUIRE(order > 0);

    const auto saved = db.GetOrder(order);
    REQUIRE(saved.has_value());
    CHECK(saved->Get_id() == order);
    CHECK(saved->Get_client_id() == client);
    CHECK(saved->Get_courier_id() == courier);
    CHECK(saved->Get_weight_kg() == Approx(12.5));
    CHECK(saved->Get_transport() == Transport::Car);
    CHECK(saved->Get_status() == OrderStatus::Assigned);
    CHECK(saved->Get_route().distance_m == Approx(5400.0));
    CHECK(saved->Get_route().duration_s == Approx(720.0));

    const auto storedRoute = db.GetRoute(order);
    REQUIRE(storedRoute.has_value());
    CHECK(storedRoute->distance_m == Approx(5400.0));
    CHECK(storedRoute->duration_s == Approx(720.0));

    CHECK(db.GetAllOrders().size() == 1);
    CHECK(db.GetActiveOrders().size() == 1);
    CHECK(db.GetOrdersByClient(client).size() == 1);
    CHECK(db.GetOrdersByClient(9999).empty());
    CHECK_FALSE(db.GetRoute(9999).has_value());
}

TEST_CASE("Заказ без курьера сохраняется с пустым courier_id", "[database]") {
    Database db(":memory:");
    REQUIRE(db.isOpen());

    const int client = db.AddClient(makeClient());
    REQUIRE(client > 0);

    const int order = db.AddOrder(
        makeOrder(client, 0, 5.0, Transport::Car, OrderStatus::Created,
                  RouteResult{}));
    REQUIRE(order > 0);

    const auto saved = db.GetOrder(order);
    REQUIRE(saved.has_value());
    CHECK(saved->Get_courier_id() == 0);
    CHECK(saved->Get_status() == OrderStatus::Created);

    const auto route = db.GetRoute(order);
    REQUIRE(route.has_value());
    CHECK(route->distance_m == Approx(0.0));
    CHECK(route->duration_s == Approx(0.0));
}

TEST_CASE("Выборки заказов фильтруются по клиенту и статусу", "[database]") {
    Database db(":memory:");
    REQUIRE(db.isOpen());

    const int firstClient = db.AddClient(makeClient("Иван"));
    const int secondClient = db.AddClient(makeClient("Мария"));
    const int courier = db.AddCourier(makeCourier("Иван", Transport::Car));
    REQUIRE(firstClient > 0);
    REQUIRE(secondClient > 0);
    REQUIRE(courier > 0);

    const RouteResult route{1000.0, 120.0};
    const int assigned = db.AddOrder(makeOrder(firstClient, courier, 1.0,
                                               Transport::Car,
                                               OrderStatus::Assigned, route));
    const int delivered = db.AddOrder(makeOrder(firstClient, courier, 2.0,
                                                Transport::Car,
                                                OrderStatus::Delivered, route));
    const int inTransit = db.AddOrder(makeOrder(secondClient, courier, 3.0,
                                                Transport::Car,
                                                OrderStatus::InTransit, route));
    const int created = db.AddOrder(makeOrder(secondClient, courier, 4.0,
                                              Transport::Car,
                                              OrderStatus::Created, route));

    REQUIRE(assigned > 0);
    REQUIRE(delivered > 0);
    REQUIRE(inTransit > 0);
    REQUIRE(created > 0);

    CHECK(db.GetAllOrders().size() == 4);
    CHECK(db.GetOrdersByClient(firstClient).size() == 2);
    CHECK(db.GetOrdersByClient(secondClient).size() == 2);

    const auto active = db.GetActiveOrders();
    REQUIRE(active.size() == 2);
    CHECK(active[0].Get_id() == assigned);
    CHECK(active[1].Get_id() == inTransit);
    CHECK(active[0].Get_status() == OrderStatus::Assigned);
    CHECK(active[1].Get_status() == OrderStatus::InTransit);
}

TEST_CASE("Обновление статуса, назначение курьера и удаление заказа",
          "[database]") {
    Database db(":memory:");
    REQUIRE(db.isOpen());

    const int client = db.AddClient(makeClient());
    const int firstCourier =
        db.AddCourier(makeCourier("Иван", Transport::Car));
    const int secondCourier =
        db.AddCourier(makeCourier("Сергей", Transport::Car));
    REQUIRE(client > 0);
    REQUIRE(firstCourier > 0);
    REQUIRE(secondCourier > 0);

    const int order = db.AddOrder(
        makeOrder(client, firstCourier, 10.0, Transport::Car,
                  OrderStatus::Assigned, RouteResult{4200.0, 600.0}));
    REQUIRE(order > 0);

    CHECK(db.UpdateOrderStatus(order, OrderStatus::InTransit));
    CHECK(db.GetOrder(order)->Get_status() == OrderStatus::InTransit);
    CHECK_FALSE(db.UpdateOrderStatus(9999, OrderStatus::Delivered));

    CHECK(db.AssignCourierToOrder(order, secondCourier));
    const auto reassigned = db.GetOrder(order);
    REQUIRE(reassigned.has_value());
    CHECK(reassigned->Get_courier_id() == secondCourier);
    CHECK(reassigned->Get_status() == OrderStatus::Assigned);
    CHECK_FALSE(db.AssignCourierToOrder(9999, secondCourier));

    CHECK(db.DeleteOrder(order));
    CHECK_FALSE(db.GetOrder(order).has_value());
    CHECK_FALSE(db.GetRoute(order).has_value());
    CHECK_FALSE(db.DeleteOrder(order));
}

TEST_CASE("Удаление клиента каскадно удаляет его заказы и маршруты",
          "[database]") {
    Database db(":memory:");
    REQUIRE(db.isOpen());

    const int client = db.AddClient(makeClient());
    const int otherClient = db.AddClient(makeClient("Мария"));
    const int courier = db.AddCourier(makeCourier("Иван", Transport::Car));
    REQUIRE(client > 0);
    REQUIRE(otherClient > 0);
    REQUIRE(courier > 0);

    const int firstOrder = db.AddOrder(
        makeOrder(client, courier, 1.0, Transport::Car,
                  OrderStatus::Assigned, RouteResult{1000.0, 100.0}));
    const int secondOrder = db.AddOrder(
        makeOrder(otherClient, courier, 2.0, Transport::Car,
                  OrderStatus::Assigned, RouteResult{2000.0, 200.0}));
    REQUIRE(firstOrder > 0);
    REQUIRE(secondOrder > 0);

    CHECK(db.DeleteClient(client));
    CHECK_FALSE(db.GetOrder(firstOrder).has_value());
    CHECK_FALSE(db.GetRoute(firstOrder).has_value());
    REQUIRE(db.GetOrder(secondOrder).has_value());
    CHECK(db.GetAllOrders().size() == 1);
}

TEST_CASE("Транзакции фиксируются и откатываются", "[database]") {
    Database db(":memory:");
    REQUIRE(db.isOpen());

    REQUIRE(db.BeginTransaction());
    CHECK(db.AddClient(makeClient("Откатится")) > 0);
    CHECK(db.GetAllClients().size() == 1);
    REQUIRE(db.Rollback());
    CHECK(db.GetAllClients().empty());

    REQUIRE(db.BeginTransaction());
    CHECK(db.AddClient(makeClient("Сохранится")) > 0);
    REQUIRE(db.Commit());
    CHECK(db.GetAllClients().size() == 1);

    CHECK_FALSE(db.Commit());
    CHECK_FALSE(db.Rollback());

    REQUIRE(db.BeginTransaction());
    CHECK_FALSE(db.BeginTransaction());
    REQUIRE(db.Rollback());
}

TEST_CASE("Неудачная вставка заказа откатывает транзакцию", "[database]") {
    Database db(":memory:");
    REQUIRE(db.isOpen());

    const int order =
        db.AddOrder(makeOrder(9999, 0, 10.0, Transport::Car,
                              OrderStatus::Created, RouteResult{1000.0, 100.0}));

    CHECK(order == -1);
    CHECK(db.GetAllOrders().empty());
    CHECK_FALSE(db.GetRoute(1).has_value());
}

TEST_CASE("Параллельное добавление клиентов потокобезопасно", "[database]") {
    Database db(":memory:");
    REQUIRE(db.isOpen());

    constexpr int threadCount = 8;
    constexpr int clientsPerThread = 25;

    std::vector<std::thread> threads;
    std::vector<int> ids(threadCount * clientsPerThread, 0);

    for (int thread = 0; thread < threadCount; ++thread) {
        threads.emplace_back([&db, &ids, thread] {
            for (int index = 0; index < clientsPerThread; ++index) {
                ids[thread * clientsPerThread + index] = db.AddClient(
                    makeClient("Клиент " + std::to_string(thread) + "-" +
                               std::to_string(index)));
            }
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    CHECK(db.GetAllClients().size() ==
          static_cast<std::size_t>(threadCount * clientsPerThread));

    const std::set<int> uniqueIds(ids.begin(), ids.end());
    CHECK(uniqueIds.size() == ids.size());
    CHECK(*uniqueIds.begin() > 0);
}

TEST_CASE("Данные сохраняются после переоткрытия базы", "[database]") {
    const auto path = makeTempDbPath();
    removeDbFiles(path);

    int clientId = 0;
    {
        Database db(path.string());
        REQUIRE(db.isOpen());
        clientId = db.AddClient(makeClient("Постоянный клиент"));
        REQUIRE(clientId > 0);
    }

    {
        Database db(path.string());
        REQUIRE(db.isOpen());

        const auto client = db.GetClient(clientId);
        REQUIRE(client.has_value());
        CHECK(client->Get_name() == "Постоянный клиент");
    }

    removeDbFiles(path);
}
