// Тесты бизнес-логики: LogisticService (клиенты, заказы, курьеры, отмена).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <stdexcept>
#include <string>

#include "test_fakes.h"

using Catch::Generators::table;
using test_support::FakeRepository;
using test_support::Fixture;
using test_support::StubGeocoder;
using test_support::StubRouteProvider;

TEST_CASE("Выбор транспорта по весу груза", "[service]") {
    FakeRepository repository;
    StubGeocoder geocoder;
    StubRouteProvider router;
    LogisticService service(repository, geocoder, router);

    const auto [weight, expected] = GENERATE(table<double, Transport>({
        {1.0, Transport::Car},
        {LogisticService::car_capacity_kg, Transport::Car},
        {LogisticService::car_capacity_kg + 0.1, Transport::Truck},
        {LogisticService::truck_capacity_kg, Transport::Truck},
    }));

    CAPTURE(weight);
    CHECK(service.selectTransport(weight) == expected);
}

TEST_CASE("Некорректный вес груза отклоняется", "[service]") {
    FakeRepository repository;
    StubGeocoder geocoder;
    StubRouteProvider router;
    LogisticService service(repository, geocoder, router);

    const double weight = GENERATE(
        -5.0,
        0.0,
        LogisticService::truck_capacity_kg + 1.0
    );

    CAPTURE(weight);
    REQUIRE_THROWS_AS(service.selectTransport(weight), std::invalid_argument);
}

TEST_CASE_METHOD(Fixture, "Регистрация клиента", "[service]") {
    SECTION("успешная регистрация сохраняет координаты геокодера") {
        const int id = service.registerClient("Мария", "+375331234567",
                                              "Гомель, ул. Ленина 1");
        CHECK(id > 0);
        CHECK(geocoder.calls == 1);
        CHECK(geocoder.last_address == "Гомель, ул. Ленина 1");

        Client client;
        REQUIRE(repository.findClient(id, client));
        CHECK(client.Get_name() == "Мария");
        CHECK(client.Get_coords().longitude ==
              Catch::Approx(geocoder.coordinates.longitude));
        CHECK(client.Get_coords().latitude ==
              Catch::Approx(geocoder.coordinates.latitude));
    }

    SECTION("пустые данные отклоняются без обращения к геокодеру") {
        REQUIRE_THROWS_AS(service.registerClient("", "+375", "Минск"),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(service.registerClient("Имя", "", "Минск"),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(service.registerClient("Имя", "+375", ""),
                          std::invalid_argument);
        CHECK(geocoder.calls == 0);
    }

    SECTION("ошибка геокодера не создаёт клиента") {
        const std::size_t before = repository.clientCount();
        geocoder.succeeds = false;
        REQUIRE_THROWS_AS(service.registerClient("Имя", "+375", "Неизвестный адрес"),
                          std::runtime_error);
        CHECK(repository.clientCount() == before);
    }
}

TEST_CASE_METHOD(Fixture, "Поиск клиента по ID", "[service]") {
    Client client;
    CHECK(service.findClient(client_id, client));
    CHECK(client.Get_id() == client_id);

    CHECK_FALSE(service.findClient(0, client));
    CHECK_FALSE(service.findClient(-1, client));
    CHECK_FALSE(service.findClient(client_id + 100, client));
}

TEST_CASE_METHOD(Fixture, "Создание заказа", "[service]") {
    const int car_id = addCourier("Иван", Transport::Car);

    SECTION("заказ получает транспорт, свободного курьера и маршрут") {
        Order order;
        REQUIRE(service.createOrder(client_id, 10.0, order));

        CHECK(order.Get_id() > 0);
        CHECK(order.Get_client_id() == client_id);
        CHECK(order.Get_courier_id() == car_id);
        CHECK(order.Get_status() == OrderStatus::Assigned);
        CHECK(order.Get_transport() == Transport::Car);
        CHECK(order.Get_route().distance_m ==
              Catch::Approx(router.route.distance_m));
        CHECK(order.Get_route().duration_s ==
              Catch::Approx(router.route.duration_s));

        CHECK_FALSE(repository.courier(car_id).Is_available());
        CHECK(repository.order(order.Get_id()).Get_status() ==
              OrderStatus::Assigned);
    }

    SECTION("заказ на грузовик выбирает грузовик, а не легковой") {
        const int truck_id = addCourier("Петр", Transport::Truck);
        Order order;
        REQUIRE(service.createOrder(client_id, 500.0, order));

        CHECK(order.Get_transport() == Transport::Truck);
        CHECK(order.Get_courier_id() == truck_id);
        CHECK(repository.courier(car_id).Is_available());
        CHECK_FALSE(repository.courier(truck_id).Is_available());
    }

    SECTION("занятый курьер не назначается") {
        repository.setCourierAvailable(car_id, false);
        const int second_car = addCourier("Сергей", Transport::Car);

        Order order;
        REQUIRE(service.createOrder(client_id, 5.0, order));
        CHECK(order.Get_courier_id() == second_car);
    }

    SECTION("неизвестный клиент не создаёт заказ") {
        Order order;
        CHECK_FALSE(service.createOrder(9999, 5.0, order));
        REQUIRE_THROWS_AS(service.createOrder(9999, 5.0),
                          std::invalid_argument);
        CHECK(repository.orderCount() == 0);
    }

    SECTION("превышение грузоподъёмности не создаёт заказ") {
        REQUIRE_THROWS_AS(
            service.createOrder(client_id, LogisticService::truck_capacity_kg + 1.0),
            std::invalid_argument);
        CHECK(repository.orderCount() == 0);
        CHECK(repository.courier(car_id).Is_available());
    }
}

TEST_CASE_METHOD(Fixture, "Неудачи при создании заказа освобождают курьера",
                 "[service]") {
    const int car_id = addCourier("Иван", Transport::Car);

    SECTION("нет свободного подходящего курьера") {
        Order order;
        REQUIRE_THROWS_AS(service.createOrder(client_id, 500.0, order),
                          std::runtime_error);
        CHECK(repository.orderCount() == 0);
        CHECK(repository.courier(car_id).Is_available());
    }

    SECTION("маршрут не построен") {
        router.succeeds = false;
        Order order;
        REQUIRE_THROWS_AS(service.createOrder(client_id, 10.0, order),
                          std::runtime_error);
        CHECK(repository.orderCount() == 0);
        CHECK(repository.courier(car_id).Is_available());
    }
}

TEST_CASE_METHOD(Fixture, "Отмена заказа", "[service]") {
    const int car_id = addCourier("Иван", Transport::Car);

    Order order;
    REQUIRE(service.createOrder(client_id, 10.0, order));

    SECTION("отмена до начала доставки освобождает курьера") {
        service.cancelOrder(order.Get_id());
        CHECK(repository.order(order.Get_id()).Get_status() ==
              OrderStatus::Cancelled);
        CHECK(repository.courier(car_id).Is_available());
    }

    SECTION("повторная отмена безопасна") {
        service.cancelOrder(order.Get_id());
        CHECK_NOTHROW(service.cancelOrder(order.Get_id()));
        CHECK(repository.courier(car_id).Is_available());
    }

    SECTION("заказ в пути отменить нельзя") {
        repository.updateOrderStatus(order.Get_id(), OrderStatus::InTransit);
        REQUIRE_THROWS_AS(service.cancelOrder(order.Get_id()),
                          std::logic_error);
        CHECK(repository.order(order.Get_id()).Get_status() ==
              OrderStatus::InTransit);
        CHECK_FALSE(repository.courier(car_id).Is_available());
    }

    SECTION("несуществующий заказ") {
        REQUIRE_THROWS_AS(service.cancelOrder(12345), std::invalid_argument);
    }
}
