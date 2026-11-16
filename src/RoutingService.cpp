#include "RoutingService.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace {

bool readNumber(const nlohmann::json &value, double &out)
{
    if (!value.is_number())
        return false;
    out = value.get<double>();
    return true;
}

}  // namespace

RoutingService::RoutingService(std::string base_url, int timeout_seconds)
    : base_url_(std::move(base_url)), timeout_seconds_(timeout_seconds)
{
    if (base_url_.empty())
        throw std::invalid_argument("Адрес OSRM не должен быть пустым");
    if (timeout_seconds_ <= 0)
        throw std::invalid_argument("Таймаут должен быть положительным");
}

bool RoutingService::parseResponse(const std::string &body, RouteResult &out)
{
    const nlohmann::json document =
        nlohmann::json::parse(body, nullptr, false);
    if (document.is_discarded() || !document.is_object())
        return false;
    if (!document.contains("code") || document.at("code") != "Ok")
        return false;
    if (!document.contains("routes") || !document.at("routes").is_array() ||
        document.at("routes").empty())
        return false;

    const nlohmann::json &route = document.at("routes").front();
    if (!route.is_object() || !route.contains("distance") ||
        !route.contains("duration"))
        return false;

    double distance = 0.0;
    double duration = 0.0;
    if (!readNumber(route.at("distance"), distance) ||
        !readNumber(route.at("duration"), duration))
        return false;

    out = RouteResult(distance, duration);
    return true;
}

bool RoutingService::getRoute(Coordinates from, Coordinates to, RouteResult &out)
{
    last_error_.clear();

    std::ostringstream path;
    path << std::fixed << std::setprecision(6)
         << "/route/v1/driving/" << from.longitude << ',' << from.latitude
         << ';' << to.longitude << ',' << to.latitude
         << "?overview=false&alternatives=false&steps=false";

    httplib::Client client(base_url_);
    client.set_connection_timeout(timeout_seconds_, 0);
    client.set_read_timeout(timeout_seconds_, 0);

    const httplib::Result result =
        client.Get(path.str(), {{"Accept", "application/json"}});
    if (!result)
    {
        last_error_ = "Не удалось подключиться к OSRM: " +
                      httplib::to_string(result.error());
        return false;
    }
    if (result->status != 200)
    {
        last_error_ = "OSRM вернул HTTP-код " +
                      std::to_string(result->status);
        return false;
    }

    if (!parseResponse(result->body, out))
    {
        last_error_ = "Маршрут между точками не найден";
        return false;
    }
    return true;
}
