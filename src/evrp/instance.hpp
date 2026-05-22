#pragma once

#include <ranges>
#include <string>
#include <vector>
#include "node.hpp"

namespace evrp {

class Instance {
 public:
  Instance() = default;
  Instance(std::string name, double optimal_value, size_t minimum_route_cnt, double cargo_capacity,
           double battery_capacity, double energy_consumption, size_t customer_cnt, size_t charging_station_cnt,
           std::vector<Node> nodes);

  [[nodiscard]] auto &nodes() const { return nodes_; }
  [[nodiscard]] auto &node(size_t ind) const { return nodes_[ind]; }
  [[nodiscard]] auto node_cnt() const { return nodes_.size(); }
  [[nodiscard]] inline auto customer_ids() const { return std::views::iota(1UZ, customer_cnt_ + 1); }
  [[nodiscard]] inline auto charging_station_ids() const { return std::views::iota(customer_cnt_ + 1, nodes_.size()); }
  [[nodiscard]] constexpr inline auto depot_id() const -> size_t { return 0UZ; }
  [[nodiscard]] inline auto cargo_capacity() const { return cargo_capacity_; }
  [[nodiscard]] inline auto battery_capacity() const { return battery_capacity_; }
  [[nodiscard]] inline auto distance(size_t node1_id, size_t node2_id) const -> double {
    if (node1_id == node2_id) [[unlikely]] {
      return 0.f;
    }
    if (node1_id > node2_id) std::swap(node1_id, node2_id);

    auto ind = node2_id * (node2_id + 1) / 2 + node1_id;
    return distance_cache_[ind];
  }
  [[nodiscard]] inline auto energy_required(size_t node1_id, size_t node2_id) const {
    return energy_consumption_ * distance(node1_id, node2_id);
  }
  [[nodiscard]] inline auto energy_consumption() const { return energy_consumption_; }
  [[nodiscard]] inline auto customer_cnt() const { return customer_cnt_; }
  [[nodiscard]] inline auto demand(size_t ind) const { return nodes_[ind].demand; }
  [[nodiscard]] inline auto charging_station_cnt() const { return charging_station_cnt_; }
  [[nodiscard]] inline auto is_customer(size_t ind) const { return ind > 0 && ind <= customer_cnt_; }
  [[nodiscard]] inline auto is_charging_station(size_t ind) const { return ind == 0 || ind > customer_cnt_; }
  [[nodiscard]] inline auto &name() { return name_; }

 private:
  auto update_distance_cache_() -> void;

  std::string name_;
  double optimal_value_;
  size_t minimum_route_cnt_;
  double cargo_capacity_;
  double battery_capacity_;
  double energy_consumption_;
  size_t customer_cnt_;
  size_t charging_station_cnt_;

  std::vector<Node> nodes_;
  std::vector<double> distance_cache_;
};

}  // namespace evrp
