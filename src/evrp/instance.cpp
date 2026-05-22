#include "instance.hpp"
#include <cmath>
#include <utility>

evrp::Instance::Instance(std::string name, double optimal_value, size_t minimum_route_cnt, double cargo_capacity,
                         double battery_capacity, double energy_consumption, size_t customer_cnt,
                         size_t charging_station_cnt, std::vector<Node> nodes)
    : name_(std::move(name)),
      optimal_value_(optimal_value),
      minimum_route_cnt_(minimum_route_cnt),
      cargo_capacity_(cargo_capacity),
      battery_capacity_(battery_capacity),
      energy_consumption_(energy_consumption),
      customer_cnt_(customer_cnt),
      charging_station_cnt_(charging_station_cnt),
      nodes_(std::move(nodes)) {
  update_distance_cache_();
}

auto evrp::Instance::update_distance_cache_() -> void {
  distance_cache_.resize(nodes_.size() * (nodes_.size() + 1) / 2);

  for (auto node1_id = 0UZ; node1_id < nodes_.size(); ++node1_id) {
    for (auto node2_id = node1_id + 1UZ; node2_id < nodes_.size(); ++node2_id) {
      auto delta_x = nodes_[node1_id].x - nodes_[node2_id].x;
      auto delta_y = nodes_[node1_id].y - nodes_[node2_id].y;
      auto dist = std::sqrt(delta_x * delta_x + delta_y * delta_y);

      distance_cache_[node2_id * (node2_id + 1) / 2 + node1_id] = dist;
    }
  }
}
