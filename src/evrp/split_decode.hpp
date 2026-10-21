#pragma once

#include <vector>
#include "optimal_decode.hpp"
#include "solution.hpp"

namespace evrp {

auto linear_split(Solution const &solution) -> Solution;

class ParetoFront2D {
 public:
  template <typename... Args>
  auto emplace(Args &&...args) -> void {
    // valid_ = false;
    front_.emplace_back(std::forward<Args>(args)...);
  }

  inline auto clear() -> void {
    valid_ = true;
    front_.clear();
  }
  [[nodiscard]] inline auto size() const { return front_.size(); }

  auto colapse() -> void;

  inline auto begin() const {
    if (!valid_) throw std::runtime_error("Call colapse before accesing elements after changing the data.");
    return front_.cbegin();
  }
  inline auto end() const {
    if (!valid_) throw std::runtime_error("Call colapse before accesing elements after changing the data.");
    return front_.cend();
  }

  inline auto operator[](size_t i) const -> SolutionCandidate const & {
    if (!valid_) throw std::runtime_error("Call colapse before accesing elements after changing the data.");
    return front_[i];
  }

 private:
  bool valid_;
  std::vector<SolutionCandidate> front_;
};

class BatteryDecode {
 public:
  BatteryDecode(std::shared_ptr<Instance> instance);

  auto decode(Solution const &solution, bool single_cs) -> Solution;
  [[nodiscard]] auto last_elapsed_ns() const -> uint64_t { return last_elapsed_ns_; }

 private:
  auto forward_pass_(Solution const &solution, bool single_cs) -> void;
  auto backward_pass_(Solution const &solution) -> Solution;

  std::shared_ptr<Instance> instance_;
  CsMatrix cs_mat_;
  ChargingPrefixTable prefix_table_;
  std::vector<ParetoFront2D> fronts_;
  uint64_t last_elapsed_ns_ = 0;

  // Shortest label of the current step per number of reachable charging points, and its index in the front
  std::vector<double> group_distance_;
  std::vector<size_t> group_label_;
};

}  // namespace evrp