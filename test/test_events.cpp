// Тесты доменных перечислений и события смены статуса.

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <ctime>
#include <string>

#include "events.h"

using Catch::Generators::table;

TEST_CASE("Строковое представление статусов заказа", "[events]") {
    const auto [status, text] = GENERATE(table<OrderStatus, const char *>({
        {OrderStatus::Created, "Создан"},
        {OrderStatus::Assigned, "Назначен"},
        {OrderStatus::InTransit, "В пути"},
        {OrderStatus::Delivered, "Доставлен"},
        {OrderStatus::Cancelled, "Отменён"},
    }));

    CHECK(std::string(toString(status)) == text);
}

TEST_CASE("Строковое представление транспорта", "[events]") {
    const auto [transport, text] = GENERATE(table<Transport, const char *>({
        {Transport::Car, "Легковой автомобиль"},
        {Transport::Truck, "Грузовой автомобиль"},
    }));

    CHECK(std::string(toString(transport)) == text);
}

TEST_CASE("Событие смены статуса хранит все поля", "[events]") {
    const std::time_t timestamp = 1700000000;
    const StatusEvent event(42, timestamp, OrderStatus::InTransit,
                            OrderStatus::Delivered);

    CHECK(event.Get_order() == 42);
    CHECK(event.Get_time() == timestamp);
    CHECK(event.Get_oldstatus() == OrderStatus::InTransit);
    CHECK(event.Get_newstatus() == OrderStatus::Delivered);
}
