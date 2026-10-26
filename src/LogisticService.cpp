#include "LogisticService.h"

#include <algorithm>
#include <utility>

LogisticService::LogisticService(IRepository &repository, IGeocoder &geocoder,
                                 IRouteProvider &route_provider,
                                 Coordinates warehouse)
    : repository_(repository), geocoder_(geocoder),
      route_provider_(route_provider), warehouse_(warehouse) {}

Transport LogisticService::selectTransport(double weight_kg) const
{
    if (!(weight_kg > 0.0))
        throw std::invalid_argument("Вес груза должен быть больше нуля");
    if (weight_kg <= car_capacity_kg)
        return Transport::Car;
    if (weight_kg <= truck_capacity_kg)
        return Transport::Truck;
    throw std::invalid_argument("Вес груза превышает грузоподъемность транспорта");
}

bool LogisticService::findClient(int client_id, Client &client) const
{
    if (client_id <= 0)
        return false;
    return repository_.findClient(client_id, client);
}

int LogisticService::registerClient(const std::string &name,
                                    const std::string &phone,
                                    const std::string &address)
{
    if (name.empty() || phone.empty() || address.empty())
        throw std::invalid_argument("Данные клиента не должны быть пустыми");

    Coordinates coordinates;
    if (!geocoder_.geocode(address, coordinates))
        throw std::runtime_error("Не удалось определить координаты адреса");
    return repository_.addClient(name, phone, address, coordinates);
}

Courier LogisticService::reserveCourier(Transport transport)
{
    const std::vector<Courier> couriers = repository_.getCouriers();
    for (const Courier &courier : couriers)
    {
        if (courier.Is_available() && courier.Get_transport() == transport)
        {
            repository_.setCourierAvailable(courier.Get_id(), false);
            return courier;
        }
    }
    throw std::runtime_error("Свободный подходящий курьер отсутствует");
}

bool LogisticService::createOrder(int client_id, double weight_kg,
                                  Order &created_order)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Client client;
    if (!findClient(client_id, client))
        return false;

    const Transport transport = selectTransport(weight_kg);
    const Courier courier = reserveCourier(transport);
    RouteResult route;
    if (!route_provider_.getRoute(warehouse_, client.Get_coords(), route))
    {
        repository_.setCourierAvailable(courier.Get_id(), true);
        throw std::runtime_error("Не удалось построить маршрут");
    }

    Order order(0, client_id, courier.Get_id(), weight_kg, transport,
                OrderStatus::Assigned, std::time(nullptr), route);
    try
    {
        order.Set_id(repository_.addOrder(order));
    }
    catch (...)
    {
        repository_.setCourierAvailable(courier.Get_id(), true);
        throw;
    }
    created_order = order;
    return true;
}

int LogisticService::createOrder(int client_id, double weight_kg)
{
    Order order;
    if (!createOrder(client_id, weight_kg, order))
        throw std::invalid_argument("Клиент с указанным ID не найден");
    return order.Get_id();
}

void LogisticService::cancelOrder(int order_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Order order;
    if (!repository_.findOrder(order_id, order))
        throw std::invalid_argument("Заказ не найден");
    if (order.Get_status() != OrderStatus::Created &&
        order.Get_status() != OrderStatus::Assigned)
        throw std::logic_error("Заказ уже начал доставляться");
    repository_.updateOrderStatus(order_id, OrderStatus::Cancelled);
    repository_.setCourierAvailable(order.Get_courier_id(), true);
}

SimulationEngine::SimulationEngine(IRepository &repository,
                                   std::chrono::milliseconds step)
    : repository_(repository), step_(step) {}

SimulationEngine::~SimulationEngine() { stop(); }

void SimulationEngine::subscribe(StatusHandler handler)
{
    if (!handler)
        throw std::invalid_argument("Обработчик события пуст");
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_.push_back(std::move(handler));
}

void SimulationEngine::start()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_)
        return;
    stopping_ = false;
    running_ = true;
    worker_ = std::thread(&SimulationEngine::run, this);
}

void SimulationEngine::enqueue(int order_id)
{
    if (order_id <= 0)
        throw std::invalid_argument("Некорректный ID заказа");
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_orders_.push(order_id);
    }
    condition_.notify_one();
}

void SimulationEngine::start(int order_id)
{
    start();
    enqueue(order_id);
}

void SimulationEngine::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_)
            return;
        stopping_ = true;
    }
    condition_.notify_all();
    if (worker_.joinable())
        worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
}

bool SimulationEngine::running() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

void SimulationEngine::run()
{
    for (;;)
    {
        int order_id;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] {
                return stopping_ || !pending_orders_.empty();
            });
            if (stopping_ && pending_orders_.empty())
                return;
            order_id = pending_orders_.front();
            pending_orders_.pop();
        }
        processOrder(order_id);
    }
}

void SimulationEngine::changeStatus(Order &order, OrderStatus status)
{
    const OrderStatus old_status = order.Get_status();
    if (old_status == status)
        return;
    repository_.updateOrderStatus(order.Get_id(), status);
    order.Set_status(status);
    StatusEvent event(order.Get_id(), std::time(nullptr), old_status, status);

    std::vector<StatusHandler> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers = handlers_;
    }
    for (const StatusHandler &handler : handlers)
        handler(event);
}

void SimulationEngine::processOrder(int order_id)
{
    Order order;
    if (!repository_.findOrder(order_id, order))
        return;
    if (order.Get_status() == OrderStatus::Assigned)
    {
        std::this_thread::sleep_for(step_);
        changeStatus(order, OrderStatus::InTransit);
    }
    if (order.Get_status() == OrderStatus::InTransit)
    {
        std::this_thread::sleep_for(step_);
        changeStatus(order, OrderStatus::Delivered);
        repository_.setCourierAvailable(order.Get_courier_id(), true);
    }
}
