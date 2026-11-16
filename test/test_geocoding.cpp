// Тесты разбора ответов Nominatim и OSRM, а также кодирования URL.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "GeocodingService.h"
#include "RoutingService.h"

TEST_CASE("Адрес кодируется для query-параметра", "[geocoding]") {
    CHECK(GeocodingService::urlEncode("Minsk") == "Minsk");
    CHECK(GeocodingService::urlEncode("Minsk, Lenina 1") ==
          "Minsk%2C%20Lenina%201");
    CHECK(GeocodingService::urlEncode("a-b_c.d~e") == "a-b_c.d~e");
    // Кириллица в UTF-8 кодируется побайтно.
    CHECK(GeocodingService::urlEncode("Минск") ==
          "%D0%9C%D0%B8%D0%BD%D1%81%D0%BA");
}

TEST_CASE("Ответ Nominatim переводится в координаты", "[geocoding]") {
    const std::string body = R"([
        {
            "place_id": 123,
            "lat": "53.9024716",
            "lon": "27.5618225",
            "display_name": "Минск, Беларусь"
        }
    ])";

    Coordinates coordinates;
    REQUIRE(GeocodingService::parseResponse(body, coordinates));
    // Nominatim отдаёт lat, lon, а Coordinates хранит longitude, latitude.
    CHECK(coordinates.longitude == 27.5618225);
    CHECK(coordinates.latitude == 53.9024716);
}

TEST_CASE("Некорректный ответ Nominatim отклоняется", "[geocoding]") {
    Coordinates coordinates;

    CHECK_FALSE(GeocodingService::parseResponse("[]", coordinates));
    CHECK_FALSE(GeocodingService::parseResponse("{}", coordinates));
    CHECK_FALSE(GeocodingService::parseResponse("не json", coordinates));
    CHECK_FALSE(GeocodingService::parseResponse(
        R"([{"lat": "53.9"}])", coordinates));
}

TEST_CASE("Ответ OSRM переводится в расстояние и время", "[routing]") {
    const std::string body = R"({
        "code": "Ok",
        "routes": [
            {"distance": 12345.6, "duration": 987.6}
        ],
        "waypoints": []
    })";

    RouteResult route;
    REQUIRE(RoutingService::parseResponse(body, route));
    CHECK(route.distance_m == 12345.6);
    CHECK(route.duration_s == 987.6);
}

TEST_CASE("Ответ OSRM без маршрута отклоняется", "[routing]") {
    RouteResult route;

    CHECK_FALSE(RoutingService::parseResponse(
        R"({"code": "NoRoute", "routes": []})", route));
    CHECK_FALSE(RoutingService::parseResponse(
        R"({"code": "Ok", "routes": []})", route));
    CHECK_FALSE(RoutingService::parseResponse(
        R"({"code": "Ok"})", route));
    CHECK_FALSE(RoutingService::parseResponse("не json", route));
}
