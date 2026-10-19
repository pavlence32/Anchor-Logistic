#pragma once
#include <string>
#include <ctime>
#include "events.h"    // OrderStatus, Transport

struct Coordinates
{
    double longitude;
    double latitude;
    Coordinates();
    Coordinates(double longitude, double latitude);
};

struct RouteResult
{
    double distance_m;
    double duration_s;
    RouteResult();
    RouteResult(double distance_m, double duration_s);
};

class Client
{
private:
    int id;
    std::string name;
    std::string phone;
    std::string address;
    Coordinates coords;
public:
    Client();
    Client(int id, std::string name, std::string phone, std::string address, Coordinates coords);
    int Get_id() const;
    std::string Get_name() const;
    std::string Get_phone() const;
    std::string Get_address() const;
    Coordinates Get_coords() const;
};

class Courier
{
private:
    int id;
    std::string name;
    Transport transport;
    bool available;
public:
    Courier();
    Courier(int id, std::string name, Transport transport, bool available);
    int Get_id() const;
    std::string Get_name() const;
    Transport Get_transport() const;
    bool Is_available() const;
    void Set_available(bool value);
};

class Order
{
private:
    int id;
    int client_id;
    int courier_id;
    double weight_kg;
    Transport transport;
    OrderStatus status;
    std::time_t created_at;
    RouteResult route;
public:
    Order();
    Order(int id, int client_id, int courier_id, double weight_kg, Transport transport,
          OrderStatus status, std::time_t created_at, RouteResult route);
    int Get_id() const;
    int Get_client_id() const;
    int Get_courier_id() const;
    double Get_weight_kg() const;
    Transport Get_transport() const;
    OrderStatus Get_status() const;
    std::time_t Get_created_at() const;
    RouteResult Get_route() const;
    void Set_status(OrderStatus status);
};