#pragma once

#include <memory>
#include <vector>
#include "instance.hpp"

namespace evrp {

class Solution {
 public:
  Solution(std::shared_ptr<Instance> instance);
  Solution(std::shared_ptr<Instance> instance, std::vector<size_t> const &routes);
  Solution(std::shared_ptr<Instance> instance, std::vector<size_t> &&routes);

  [[nodiscard]] inline auto &routes() const { return routes_; }
  [[nodiscard]] inline auto &routes() { return routes_; }
  [[nodiscard]] inline auto &instance() const { return *instance_; }
  [[nodiscard]] inline auto instance_ptr() const { return instance_; }

  [[nodiscard]] auto is_cargo_valid() const -> bool;
  [[nodiscard]] auto is_energy_and_cargo_valid() const -> bool;
  [[nodiscard]] auto is_valid() const -> bool;
  [[nodiscard]] auto cost() const -> double;

 private:
  std::shared_ptr<Instance> instance_;
  std::vector<size_t> routes_;
};

}  // namespace evrp