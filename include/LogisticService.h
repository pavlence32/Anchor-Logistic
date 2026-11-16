#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "interface.h"

class LogisticService
{
public:
    static constexpr double car_capacity_kg = 20.0;
    static constexpr double truck_capacity_kg = 1000.0;

    LogisticService(IRepository &repository, IGeocoder &geocoder,
                    IRouteProvider &route_provider,
                    Coordinates warehouse = Coordinates{});

    Transport selectTransport(double weight_kg) const;
    bool findClient(int client_id, Client &client) const;
    int registerClient(const std::string &name, const std::string &phone,
                       const std::string &address);
    int createOrder(int client_id, double weight_kg);
    bool createOrder(int client_id, double weight_kg, Order &created_order);
    void cancelOrder(int order_id);

private:
    IRepository &repository_;
    IGeocoder &geocoder_;
    IRouteProvider &route_provider_;
    Coordinates warehouse_;
    mutable std::mutex mutex_;

    Courier reserveCourier(Transport transport);
};

class SimulationEngine
{
public:
    using StatusHandler = std::function<void(const StatusEvent &)>;

    static constexpr double truck_duration_factor = 1.3;
    static constexpr double default_time_scale = 60.0;

    explicit SimulationEngine(IRepository &repository,
                              std::chrono::milliseconds step =
                                  std::chrono::milliseconds(1000),
                              double time_scale = default_time_scale);
    ~SimulationEngine();

    void subscribe(StatusHandler handler);
    void start();
    void enqueue(int order_id);
    void start(int order_id);
    void stop();
    bool running() const;

    
    std::chrono::milliseconds deliveryDelay(const Order &order) const;

    
    void resumeActiveOrders();

   
    void waitUntilIdle();

private:
    void run();
    void processOrder(int order_id);
    void changeStatus(Order &order, OrderStatus status);
    bool sleepWhileActive(std::chrono::milliseconds duration);

    IRepository &repository_;
    std::chrono::milliseconds step_;
    double time_scale_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<int> pending_orders_;
    std::vector<StatusHandler> handlers_;
    std::thread worker_;
    bool stopping_{false};
    bool running_{false};
    bool processing_{false};
};
