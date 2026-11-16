#pragma once

#include <string>

#include "interface.h"
#include "models.h"

// Маршрутизация через публичный демо-сервер OSRM (Open Source Routing Machine).
// Сервис возвращает расстояние в метрах и длительность в секундах.
class RoutingService final : public IRouteProvider
{
public:
    static constexpr const char *default_base_url =
        "https://router.project-osrm.org";

    explicit RoutingService(
        std::string base_url = default_base_url,
        int timeout_seconds = 10);

    bool getRoute(Coordinates from, Coordinates to, RouteResult &out) override;

    const std::string &lastError() const { return last_error_; }

    // Разбор JSON-ответа OSRM: routes[0].distance и routes[0].duration.
    static bool parseResponse(const std::string &body, RouteResult &out);

private:
    std::string base_url_;
    int timeout_seconds_;
    std::string last_error_;
};
