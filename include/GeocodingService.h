#pragma once

#include <chrono>
#include <mutex>
#include <string>

#include "interface.h"
#include "models.h"

// Геокодирование адреса через Nominatim (OpenStreetMap).
// HTTP-запросы выполняются через cpp-httplib, ответ разбирается
// библиотекой nlohmann/json.
class GeocodingService final : public IGeocoder
{
public:
    static constexpr const char *default_base_url =
        "https://nominatim.openstreetmap.org";
    static constexpr const char *default_user_agent =
        "Anchor-Logistic/1.0 (educational project)";

    explicit GeocodingService(
        std::string user_agent = default_user_agent,
        std::string base_url = default_base_url,
        int timeout_seconds = 10,
        std::chrono::milliseconds min_interval = std::chrono::milliseconds(1000));

    bool geocode(const std::string &address, Coordinates &out) override;

    const std::string &lastError() const { return last_error_; }

    // Percent-кодирование строки (UTF-8) для query-параметра.
    static std::string urlEncode(const std::string &value);

    // Разбор JSON-ответа Nominatim: массив объектов с полями lat и lon.
    // Nominatim возвращает порядок "lat, lon", а Coordinates хранит
    // "longitude, latitude", поэтому значения меняются местами.
    static bool parseResponse(const std::string &body, Coordinates &out);

private:
    void waitForRateLimit();

    std::string user_agent_;
    std::string base_url_;
    int timeout_seconds_;
    std::chrono::milliseconds min_interval_;

    std::mutex request_mutex_;
    std::chrono::steady_clock::time_point last_request_{};
    std::string last_error_;
};
