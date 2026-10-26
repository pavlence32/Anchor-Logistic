#pragma once
#include <string>
#include <vector>
#include "events.h"   
#include "models.h"    

class IGeocoder
{
public:
    virtual ~IGeocoder() {}
    virtual bool geocode(const std::string &address, Coordinates &out) = 0;
};
class IRouteProvider
{
public:
    virtual ~IRouteProvider() {}
    virtual bool getRoute(Coordinates from, Coordinates to, RouteResult &out) = 0;
};
class IRepository
{
public:
    virtual ~IRepository() {}
    virtual int addClient(const std::string &name, const std::string &phone,
                          const std::string &address, Coordinates coords) = 0;
    virtual bool findClient(int id, Client &out) = 0;
    virtual std::vector<Courier> getCouriers() = 0;
    virtual void setCourierAvailable(int courier_id, bool value) = 0;
    virtual int addOrder(const Order &order) = 0;
    virtual bool findOrder(int id, Order &out) = 0;
    virtual void updateOrderStatus(int id, OrderStatus status) = 0;
};

class FixedGeocoder final : public IGeocoder
{
    Coordinates coordinates_;
public:
    explicit FixedGeocoder(Coordinates coordinates) : coordinates_(coordinates) {}
    bool geocode(const std::string &, Coordinates &out) override
    {
        out = coordinates_;
        return true;
    }
};

class FixedRouteProvider final : public IRouteProvider
{
    RouteResult route_;
    bool succeeds_;
public:
    explicit FixedRouteProvider(RouteResult route, bool succeeds = true)
        : route_(route), succeeds_(succeeds) {}
    bool getRoute(Coordinates, Coordinates, RouteResult &out) override
    {
        if (!succeeds_)
            return false;
        out = route_;
        return true;
    }
};