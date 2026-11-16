// Тесты фоновой симуляции: статусы, скорость, отмена, остановка, восстановление.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "test_fakes.h"

using namespace std::chrono_literals;

using test_support::FakeRepository;
using test_support::Fixture;
using test_support::makeOrder;

TEST_CASE_METHOD(Fixture, "Симуляция доводит заказ до доставки", "[simulation]") {
    const int car_id = addCourier("Иван", Transport::Car);

    Order order;
    REQUIRE(service.createOrder(client_id, 10.0, order));

    SimulationEngine simulation(repository, 2ms, 1e9);
    std::mutex events_mutex;
    std::vector<StatusEvent> events;
    simulation.subscribe([&](const StatusEvent &event) {
        std::lock_guard<std::mutex> lock(events_mutex);
        events.push_back(event);
    });

    simulation.start();
    CHECK(simulation.running());
    simulation.enqueue(order.Get_id());
    simulation.waitUntilIdle();
    simulation.stop();
    CHECK_FALSE(simulation.running());

    CHECK(repository.order(order.Get_id()).Get_status() ==
          OrderStatus::Delivered);
    CHECK(repository.courier(car_id).Is_available());

    std::lock_guard<std::mutex> lock(events_mutex);
    REQUIRE(events.size() == 2);
    CHECK(events[0].Get_order() == order.Get_id());
    CHECK(events[0].Get_oldstatus() == OrderStatus::Assigned);
    CHECK(events[0].Get_newstatus() == OrderStatus::InTransit);
    CHECK(events[1].Get_oldstatus() == OrderStatus::InTransit);
    CHECK(events[1].Get_newstatus() == OrderStatus::Delivered);
}

TEST_CASE("Скорость симуляции зависит от транспорта и времени OSRM",
          "[simulation]") {
    FakeRepository repository;
    SimulationEngine simulation(repository, 10ms, 60.0);

    const Order car = makeOrder(Transport::Car, RouteResult{1000.0, 600.0});
    const Order truck = makeOrder(Transport::Truck, RouteResult{1000.0, 600.0});
    const Order instant = makeOrder(Transport::Car, RouteResult{0.0, 0.0});

    // 600 секунд маршрута при масштабе 60 — это 10 секунд симуляции,
    // грузовик медленнее легкового автомобиля.
    CHECK(simulation.deliveryDelay(car) == 10000ms);
    CHECK(simulation.deliveryDelay(truck) == 13000ms);
    CHECK(simulation.deliveryDelay(truck) > simulation.deliveryDelay(car));
    // Минимальная длительность этапа не меньше базового такта.
    CHECK(simulation.deliveryDelay(instant) == 10ms);
}

TEST_CASE_METHOD(Fixture, "Отмена во время симуляции не перезаписывается",
                 "[simulation]") {
    const int car_id = addCourier("Иван", Transport::Car);

    Order order;
    REQUIRE(service.createOrder(client_id, 10.0, order));

    SimulationEngine simulation(repository, 300ms, 1e9);
    simulation.start();
    simulation.enqueue(order.Get_id());
    std::this_thread::sleep_for(20ms);
    service.cancelOrder(order.Get_id());
    simulation.waitUntilIdle();
    simulation.stop();

    CHECK(repository.order(order.Get_id()).Get_status() ==
          OrderStatus::Cancelled);
    CHECK(repository.courier(car_id).Is_available());
}

TEST_CASE_METHOD(Fixture, "Восстановление незавершённых заказов после перезапуска",
                 "[simulation]") {
    const int car_id = addCourier("Иван", Transport::Car);

    Order order;
    REQUIRE(service.createOrder(client_id, 10.0, order));
    // Имитируем заказ, который был «в пути» до перезапуска приложения.
    repository.updateOrderStatus(order.Get_id(), OrderStatus::InTransit);

    SimulationEngine simulation(repository, 2ms, 1e9);
    simulation.start();
    simulation.resumeActiveOrders();
    simulation.waitUntilIdle();
    simulation.stop();

    CHECK(repository.order(order.Get_id()).Get_status() ==
          OrderStatus::Delivered);
    CHECK(repository.courier(car_id).Is_available());
}

TEST_CASE_METHOD(Fixture, "Завершённые и отменённые заказы не возобновляются",
                 "[simulation]") {
    addCourier("Иван", Transport::Car);

    Order delivered;
    REQUIRE(service.createOrder(client_id, 10.0, delivered));
    repository.updateOrderStatus(delivered.Get_id(), OrderStatus::Delivered);

    addCourier("Петр", Transport::Car);
    Order cancelled;
    REQUIRE(service.createOrder(client_id, 10.0, cancelled));
    service.cancelOrder(cancelled.Get_id());

    SimulationEngine simulation(repository, 2ms, 1e9);
    simulation.start();
    simulation.resumeActiveOrders();
    simulation.waitUntilIdle();
    simulation.stop();

    CHECK(repository.order(delivered.Get_id()).Get_status() ==
          OrderStatus::Delivered);
    CHECK(repository.order(cancelled.Get_id()).Get_status() ==
          OrderStatus::Cancelled);
}

TEST_CASE_METHOD(Fixture, "Остановка фонового потока", "[simulation]") {
    addCourier("Иван", Transport::Car);

    Order order;
    REQUIRE(service.createOrder(client_id, 10.0, order));

    // Большой шаг: без прерывания остановка ждала бы несколько секунд.
    SimulationEngine simulation(repository, 5s, 1e9);
    simulation.start();
    simulation.enqueue(order.Get_id());
    std::this_thread::sleep_for(20ms);

    const auto begin = std::chrono::steady_clock::now();
    simulation.stop();
    const auto elapsed = std::chrono::steady_clock::now() - begin;

    CHECK_FALSE(simulation.running());
    CHECK(elapsed < 2s);
    // Остановка не должна доводить заказ до доставки.
    CHECK(repository.order(order.Get_id()).Get_status() !=
          OrderStatus::Delivered);
}

TEST_CASE("Проверка аргументов симуляции", "[simulation]") {
    FakeRepository repository;

    SECTION("нулевой шаг") {
        REQUIRE_THROWS_AS(SimulationEngine(repository, 0ms),
                          std::invalid_argument);
    }

    SECTION("некорректный масштаб времени") {
        REQUIRE_THROWS_AS(SimulationEngine(repository, 1ms, 0.0),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(SimulationEngine(repository, 1ms, -1.0),
                          std::invalid_argument);
    }

    SECTION("некорректный ID заказа") {
        SimulationEngine simulation(repository, 1ms);
        REQUIRE_THROWS_AS(simulation.enqueue(0), std::invalid_argument);
        REQUIRE_THROWS_AS(simulation.enqueue(-1), std::invalid_argument);
    }

    SECTION("пустой обработчик события") {
        SimulationEngine simulation(repository, 1ms);
        REQUIRE_THROWS_AS(simulation.subscribe(nullptr),
                          std::invalid_argument);
    }
}
