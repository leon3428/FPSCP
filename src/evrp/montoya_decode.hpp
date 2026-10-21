#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include "optimal_decode.hpp"
#include "solution.hpp"

namespace evrp {

// Exact split-then-repair decoder in the style of
//   Montoya, Gueret, Mendoza, Villegas (2016). A multi-space sampling heuristic for the green
//   vehicle routing problem. Transportation Research Part C 70, 113-128.
//
// The customer permutation is split with Prins' split: every block of consecutive customers whose
// demand fits in the vehicle is a candidate route, and the best set of routes is a shortest path over
// these candidates. Each candidate route is priced as follows (Montoya et al., Sec. 3.3-3.4):
//   * if the route is battery-feasible without charging, its cost is its plain length;
//   * otherwise it is "repaired" by inserting charging stops optimally. The repair is a shortest path
//     in Montoya's repair graph, whose node (j, f) means "fully charged at charging node f right after
//     the j-th customer of the route" and whose arcs are stretches driven on a single charge.
// Montoya et al. need the pulse algorithm for the repair because their problem also has a route
// duration limit. Without it, the repair is an ordinary shortest path in a layered DAG. Chains of
// charging stations between two customers use the same Floyd-Warshall matrix as OptimalDecode, and the
// depot may be used as a charging stop, exactly as in OptimalDecode. The decoder therefore returns a
// solution with the same (optimal) cost as OptimalDecode for every permutation.
//
// Running time per permutation: O(n * b^2 * w * m^2), where b is the largest number of consecutive
// customers that fit in one vehicle, w the largest number that can be served on one charge, and m the
// number of charging nodes including the depot.
class MontoyaDecode {
 public:
  MontoyaDecode(std::shared_ptr<Instance> instance);

  // Same contract as OptimalDecode::decode: `solution.routes()` is a permutation of all customers.
  // Returns a flat route (depot-delimited, charging stations included). If no feasible decoding exists,
  // the input solution is returned unchanged.
  auto decode(Solution const &solution) -> Solution;

  [[nodiscard]] auto last_elapsed_ns() const -> uint64_t { return last_elapsed_ns_; }
  // Candidate routes priced during the last split, and how many of them needed a repair.
  [[nodiscard]] auto last_routes_evaluated() const -> size_t { return last_routes_evaluated_; }
  [[nodiscard]] auto last_routes_repaired() const -> size_t { return last_routes_repaired_; }

 private:
  auto split_(Solution const &solution) -> Solution;
  [[nodiscard]] auto plain_route_feasible_(std::vector<size_t> const &perm, size_t s, size_t t) const -> bool;
  auto repair_(std::vector<size_t> const &perm, size_t s, size_t t) -> double;
  auto close_layer_(size_t layer) -> void;
  auto append_repaired_route_(std::vector<size_t> const &perm, size_t s, size_t t, std::vector<size_t> &out)
      -> void;
  auto append_cs_path_(std::vector<size_t> &out, size_t from_ind, size_t to_ind) -> void;

  std::shared_ptr<Instance> instance_;
  CsMatrix cs_mat_;

  // Split buffers (indexed by position in the permutation)
  std::vector<double> cum_dist_;
  std::vector<double> best_;
  std::vector<size_t> pred_;

  // Repair-graph buffers for the route currently being priced, stored layer-major: [layer * m + cs_ind].
  // arrive_: cheapest way to reach charging node cs directly from the layer-th customer.
  // closed_: cheapest way to be fully charged at cs after the layer-th customer (after a station chain).
  std::vector<double> arrive_;
  std::vector<size_t> arrive_from_layer_;
  std::vector<size_t> arrive_from_cs_;
  std::vector<double> closed_;
  std::vector<size_t> closed_from_cs_;
  std::vector<size_t> path_buf_;

  uint64_t last_elapsed_ns_ = 0;
  size_t last_routes_evaluated_ = 0;
  size_t last_routes_repaired_ = 0;
};

}  // namespace evrp
