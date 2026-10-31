#pragma once
#include <ctime>
#include <functional>
enum class OrderStatus
{
    Created,
    Assigned,
    InTransit,
    Delivered,
    Cancelled

};
enum class Transport
{
    Car,
    Truck
};
const char * toString(OrderStatus s);
const char * toString (Transport a);
class StatusEvent
{
    int order_id;
    time_t time;
    OrderStatus old_status;
    OrderStatus new_status;
    public:
    StatusEvent(int order_id, std::time_t time, OrderStatus old_status, OrderStatus new_status);
    int Get_order() const;
    std::time_t Get_time() const;
    OrderStatus Get_oldstatus() const ;
    OrderStatus Get_newstatus() const ;
};