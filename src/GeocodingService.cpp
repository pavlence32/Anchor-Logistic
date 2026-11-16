#include "GeocodingService.h"

#include <cctype>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <utility>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace {

const char *hex_digits = "0123456789ABCDEF";

// Nominatim отдаёт lat/lon строками, но на всякий случай поддерживаем числа.
bool readNumber(const nlohmann::json &value, double &out)
{
    if (value.is_number())
    {
        out = value.get<double>();
        return true;
    }
    if (value.is_string())
    {
        const std::string text = value.get<std::string>();
        try
        {
            std::size_t consumed = 0;
            const double parsed = std::stod(text, &consumed);
            if (consumed != text.size())
                return false;
            out = parsed;
            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }
    return false;
}

}  // namespace

GeocodingService::GeocodingService(std::string user_agent,
                                   std::string base_url,
                                   int timeout_seconds,
                                   std::chrono::milliseconds min_interval)
    : user_agent_(std::move(user_agent)), base_url_(std::move(base_url)),
      timeout_seconds_(timeout_seconds), min_interval_(min_interval)
{
    if (user_agent_.empty())
        throw std::invalid_argument("User-Agent не должен быть пустым");
    if (base_url_.empty())
        throw std::invalid_argument("Адрес Nominatim не должен быть пустым");
    if (timeout_seconds_ <= 0)
        throw std::invalid_argument("Таймаут должен быть положительным");
    if (min_interval_ < std::chrono::milliseconds(0))
        throw std::invalid_argument("Интервал между запросами не может быть отрицательным");
}

std::string GeocodingService::urlEncode(const std::string &value)
{
    std::string result;
    result.reserve(value.size() * 3);
    for (const unsigned char symbol : value)
    {
        if (std::isalnum(symbol) || symbol == '-' || symbol == '_' ||
            symbol == '.' || symbol == '~')
        {
            result.push_back(static_cast<char>(symbol));
        }
        else
        {
            result.push_back('%');
            result.push_back(hex_digits[symbol >> 4]);
            result.push_back(hex_digits[symbol & 0x0F]);
        }
    }
    return result;
}

bool GeocodingService::parseResponse(const std::string &body, Coordinates &out)
{
    const nlohmann::json document =
        nlohmann::json::parse(body, nullptr, false);
    if (document.is_discarded() || !document.is_array() || document.empty())
        return false;

    const nlohmann::json &place = document.front();
    if (!place.is_object() || !place.contains("lat") || !place.contains("lon"))
        return false;

    double latitude = 0.0;
    double longitude = 0.0;
    if (!readNumber(place.at("lat"), latitude) ||
        !readNumber(place.at("lon"), longitude))
        return false;

    out = Coordinates(longitude, latitude);
    return true;
}

void GeocodingService::waitForRateLimit()
{
    std::unique_lock<std::mutex> lock(request_mutex_);
    if (last_request_ != std::chrono::steady_clock::time_point{})
    {
        const auto elapsed = std::chrono::steady_clock::now() - last_request_;
        if (elapsed < min_interval_)
            std::this_thread::sleep_for(min_interval_ - elapsed);
    }
    last_request_ = std::chrono::steady_clock::now();
}

bool GeocodingService::geocode(const std::string &address, Coordinates &out)
{
    last_error_.clear();
    if (address.empty())
    {
        last_error_ = "Адрес не должен быть пустым";
        return false;
    }

    // Правила Nominatim: не чаще одного запроса в секунду.
    waitForRateLimit();

    const std::string path =
        "/search?q=" + urlEncode(address) +
        "&format=json&limit=1&countrycodes=by";

    httplib::Client client(base_url_);
    client.set_connection_timeout(timeout_seconds_, 0);
    client.set_read_timeout(timeout_seconds_, 0);

    const httplib::Result result =
        client.Get(path, {{"User-Agent", user_agent_},
                          {"Accept", "application/json"}});
    if (!result)
    {
        last_error_ = "Не удалось подключиться к Nominatim: " +
                      httplib::to_string(result.error());
        return false;
    }
    if (result->status != 200)
    {
        last_error_ = "Nominatim вернул HTTP-код " +
                      std::to_string(result->status);
        return false;
    }

    if (!parseResponse(result->body, out))
    {
        last_error_ = "Адрес не найден";
        return false;
    }
    return true;
}
