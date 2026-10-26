#include "models.h"

Coordinates::Coordinates() : longitude(0.0), latitude(0.0) {}

Coordinates::Coordinates(double longitude_value, double latitude_value)
    : longitude(longitude_value), latitude(latitude_value) {}

RouteResult::RouteResult() : distance_m(0.0), duration_s(0.0) {}

RouteResult::RouteResult(double distance, double duration)
    : distance_m(distance), duration_s(duration) {}

Client::Client() : id(0), coords() {}

Client::Client(int id_value, std::string name_value, std::string phone_value,
               std::string address_value, Coordinates coordinates)
    : id(id_value), name(std::move(name_value)), phone(std::move(phone_value)),
      address(std::move(address_value)), coords(coordinates) {}

int Client::Get_id() const { return id; }
std::string Client::Get_name() const { return name; }
std::string Client::Get_phone() const { return phone; }
std::string Client::Get_address() const { return address; }
Coordinates Client::Get_coords() const { return coords; }

Courier::Courier() : id(0), transport(Transport::Car), available(false) {}

Courier::Courier(int id_value, std::string name_value, Transport transport_value,
                 bool available_value)
    : id(id_value), name(std::move(name_value)), transport(transport_value),
      available(available_value) {}

int Courier::Get_id() const { return id; }
std::string Courier::Get_name() const { return name; }
Transport Courier::Get_transport() const { return transport; }
bool Courier::Is_available() const { return available; }
void Courier::Set_available(bool value) { available = value; }

Order::Order()
    : id(0), client_id(0), courier_id(0), weight_kg(0.0),
      transport(Transport::Car), status(OrderStatus::Created), created_at(0),
      route() {}

Order::Order(int id_value, int client_id_value, int courier_id_value,
             double weight, Transport transport_value, OrderStatus status_value,
             std::time_t created, RouteResult route_value)
    : id(id_value), client_id(client_id_value), courier_id(courier_id_value),
      weight_kg(weight), transport(transport_value), status(status_value),
      created_at(created), route(route_value) {}

int Order::Get_id() const { return id; }
int Order::Get_client_id() const { return client_id; }
int Order::Get_courier_id() const { return courier_id; }
double Order::Get_weight_kg() const { return weight_kg; }
Transport Order::Get_transport() const { return transport; }
OrderStatus Order::Get_status() const { return status; }
std::time_t Order::Get_created_at() const { return created_at; }
RouteResult Order::Get_route() const { return route; }
void Order::Set_status(OrderStatus value) { status = value; }
void Order::Set_id(int value) { id = value; }
