#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include "solution.hpp"

namespace evrp {

enum class JumpCase : uint8_t { None, Direct, Depot, Charging };

struct SolutionCandidate {
  // Solution quality
  double distance;
  double battery_used;
  double cargo_used;

  // Backtracking info
  size_t prev;
  uint8_t cs_ind_in;
  uint8_t cs_ind_out;
  JumpCase jump_case;

  SolutionCandidate();
  SolutionCandidate(double distance, double battery_used, double cargo_used);

  auto dominates(SolutionCandidate const &other) const -> bool;
  inline auto is_dominated_by(SolutionCandidate const &other) const { return other.dominates(*this); }
};

class ParetoFront {
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

class CsMatrix {
 public:
  CsMatrix(std::shared_ptr<Instance> instance);

  [[nodiscard]] inline auto distance(size_t ind1, size_t ind2) const { return dist_mat_[ind1][ind2]; };
  [[nodiscard]] inline auto size() const { return cs_cnt_; }
  [[nodiscard]] inline auto ind_to_id(size_t ind) const {
    return ind == 0 ? instance_->depot_id() : instance_->charging_station_ids()[ind - 1];
  }

  auto trace(std::vector<size_t> &route, size_t ind1, size_t ind2) -> void;

 private:
  auto floyd_warshall_() -> void;

  size_t cs_cnt_;
  std::shared_ptr<Instance> instance_;
  std::vector<std::vector<double>> dist_mat_;
  std::vector<std::vector<size_t>> prev_;
};

// For every node p a step can start from (the depot or a customer), the charging points sorted by energy from p.
// The charging points a label can still reach from p form a prefix of that order, so the cheapest way from p into
// the station graph and out at charging point g depends only on p, the prefix length and g.
class ChargingPrefixTable {
 public:
  struct Entry {
    double distance;  // min over the prefix of d(p, f) + F(f, g)
    uint8_t cs_ind;   // the entry charging point f achieving it
  };

  ChargingPrefixTable(Instance const &instance, CsMatrix const &cs_mat);

  // Number of charging points reachable from p after `battery_used` energy has been spent since the last charge.
  [[nodiscard]] auto reachable_cnt(size_t p, double battery_used) const -> size_t;
  // Index of the (r + 1)-th closest charging point to p, by energy.
  [[nodiscard]] auto closest(size_t p, size_t r) const -> size_t { return order_[energy_index_(p, r)]; }
  // Cheapest entry into charging point g using one of the `reach` closest charging points to p (reach >= 1).
  [[nodiscard]] auto best_entry(size_t p, size_t reach, size_t g) const -> Entry const & {
    return entries_[entry_index_(p, reach - 1, g)];
  }

 private:
  [[nodiscard]] auto energy_index_(size_t p, size_t r) const -> size_t { return p * cs_cnt_ + r; }
  [[nodiscard]] auto entry_index_(size_t p, size_t r, size_t g) const -> size_t { return (p * cs_cnt_ + r) * cs_cnt_ + g; }

  size_t cs_cnt_;
  double battery_capacity_;
  std::vector<double> energy_;   // energy from p to its r-th closest charging point
  std::vector<uint8_t> order_;   // index of p's r-th closest charging point
  std::vector<Entry> entries_;   // best entry over the r + 1 closest charging points, per exit point g
};

class OptimalDecode {
 public:
  OptimalDecode(std::shared_ptr<Instance> instance);

  auto decode(Solution const &solution) -> Solution;
  [[nodiscard]] auto front_size(size_t i) const { return fronts_[i].size(); }
  [[nodiscard]] auto front_cnt() const { return fronts_.size(); }
  [[nodiscard]] auto last_elapsed_ns() const -> uint64_t { return last_elapsed_ns_; }

 private:
  auto forward_pass_(Solution const &solution) -> void;
  auto backward_pass_(Solution const &solution) -> Solution;

  std::shared_ptr<Instance> instance_;
  CsMatrix cs_mat_;
  ChargingPrefixTable prefix_table_;
  std::vector<ParetoFront> fronts_;
  uint64_t last_elapsed_ns_ = 0;
  std::vector<size_t> exits_;
};

}  // namespace evrp