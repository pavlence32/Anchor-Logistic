#include "events.h"

const char* toString(OrderStatus s)
{
    switch (s)
    {
        case OrderStatus::Created:
            return "Создан";
        case OrderStatus::Assigned:
            return "Назначен";
        case OrderStatus::InTransit:
            return "В пути";
        case OrderStatus::Delivered:
            return "Доставлен";
        case OrderStatus::Cancelled:
            return "Отменён";
    }
    return "?";
}

const char* toString(Transport a)
{
    switch (a)
    {
        case Transport::Car:
            return "Легковой автомобиль";
        case Transport::Truck:
            return "Грузовой автомобиль";
    }
    return "?";
}

StatusEvent::StatusEvent(int order_id, std::time_t time, OrderStatus old_status, OrderStatus new_status)
{
    this->order_id = order_id;
    this->time = time;
    this->old_status = old_status;
    this->new_status = new_status;
}

int StatusEvent::Get_order() const
{
    return order_id;
}

std::time_t StatusEvent::Get_time() const
{
    return time;
}

OrderStatus StatusEvent::Get_oldstatus() const
{
    return old_status;
}

OrderStatus StatusEvent::Get_newstatus() const
{
    return new_status;
}