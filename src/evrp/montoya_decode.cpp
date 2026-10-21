#include "montoya_decode.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <limits>
#include <utility>

namespace {
constexpr auto kInf = std::numeric_limits<double>::infinity();
constexpr auto kNone = std::numeric_limits<size_t>::max();
}  // namespace

evrp::MontoyaDecode::MontoyaDecode(std::shared_ptr<Instance> instance) : instance_(instance), cs_mat_(instance) {}

auto evrp::MontoyaDecode::decode(Solution const &solution) -> Solution {
  auto t0 = std::chrono::steady_clock::now();
  last_routes_evaluated_ = 0;
  last_routes_repaired_ = 0;
  auto result = split_(solution);
  last_elapsed_ns_ =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
  return result;
}

// Prins' split over the permutation. Block (s, t] is the route depot -> perm[s] -> ... -> perm[t-1] -> depot.
auto evrp::MontoyaDecode::split_(Solution const &solution) -> Solution {
  const auto &inst = *instance_;
  const auto &perm = solution.routes();
  const auto n = perm.size();
  assert(n == inst.customer_cnt());
  const auto depot = inst.depot_id();
  const auto battery = inst.battery_capacity();

  // cum_dist_[i] = length of perm[0] -> perm[1] -> ... -> perm[i]
  cum_dist_.assign(n, 0.0);
  for (auto i = 1UZ; i < n; ++i) cum_dist_[i] = cum_dist_[i - 1] + inst.distance(perm[i - 1], perm[i]);

  best_.assign(n + 1, kInf);
  pred_.assign(n + 1, kNone);
  best_[0] = 0.0;

  for (auto s = 0UZ; s < n; ++s) {
    if (best_[s] == kInf) continue;

    auto load = 0.0;
    auto energy = 0.0;     // energy used on depot -> perm[s] -> ... -> perm[t-1] without charging
    auto open_ok = true;   // that open path is battery-feasible

    for (auto t = s + 1; t <= n; ++t) {
      auto prev = t == s + 1 ? depot : perm[t - 2];
      auto cust = perm[t - 1];

      load += inst.demand(cust);
      if (load > inst.cargo_capacity()) break;

      if (open_ok) {
        energy += inst.energy_required(prev, cust);
        open_ok = energy <= battery;
      }

      ++last_routes_evaluated_;
      auto plain_len = inst.distance(depot, perm[s]) + (cum_dist_[t - 1] - cum_dist_[s]) + inst.distance(cust, depot);
      auto cost = plain_len;
      if (!(open_ok && inst.energy_required(cust, depot) + energy <= battery)) {
        ++last_routes_repaired_;
        cost = repair_(perm, s, t);
      }

      auto candidate = best_[s] + cost;
      if (candidate < best_[t]) {
        best_[t] = candidate;
        pred_[t] = s;
      }
    }
  }

  if (best_[n] == kInf) return solution;

  // Collect the chosen blocks and rebuild the flat route.
  auto blocks = std::vector<std::pair<size_t, size_t>>();
  for (auto t = n; t > 0; t = pred_[t]) blocks.emplace_back(pred_[t], t);
  std::ranges::reverse(blocks);

  auto routes = std::vector<size_t>{depot};
  for (auto [s, t] : blocks) {
    if (plain_route_feasible_(perm, s, t)) {
      for (auto l = s; l < t; ++l) routes.push_back(perm[l]);
      routes.push_back(depot);
    } else {
      append_repaired_route_(perm, s, t, routes);
    }
  }

  return Solution(solution.instance_ptr(), std::move(routes));
}

auto evrp::MontoyaDecode::plain_route_feasible_(std::vector<size_t> const &perm, size_t s, size_t t) const -> bool {
  const auto &inst = *instance_;
  auto energy = 0.0;
  auto prev = inst.depot_id();
  for (auto l = s; l < t; ++l) {
    energy += inst.energy_required(prev, perm[l]);
    if (energy > inst.battery_capacity()) return false;
    prev = perm[l];
  }
  return inst.energy_required(prev, inst.depot_id()) + energy <= inst.battery_capacity();
}

// Optimal charging-stop insertion for the route depot -> perm[s] -> ... -> perm[t-1] -> depot.
// Layer j (0..k) is "after the j-th customer of the route"; layer 0 is the depot at the start.
// Returns +inf if no feasible plan exists. Leaves the predecessor tables filled for reconstruction.
auto evrp::MontoyaDecode::repair_(std::vector<size_t> const &perm, size_t s, size_t t) -> double {
  const auto &inst = *instance_;
  const auto m = cs_mat_.size();
  const auto k = t - s;
  const auto battery = inst.battery_capacity();

  arrive_.assign((k + 1) * m, kInf);
  arrive_from_layer_.assign((k + 1) * m, kNone);
  arrive_from_cs_.assign((k + 1) * m, kNone);
  closed_.assign((k + 1) * m, kInf);
  closed_from_cs_.assign((k + 1) * m, kNone);

  arrive_[0] = 0.0;  // start at the depot (charging index 0) with a full battery

  for (auto i = 0UZ; i < k; ++i) {
    close_layer_(i);

    for (auto f = 0UZ; f < m; ++f) {
      auto base = closed_[i * m + f];
      if (base == kInf) continue;

      // Drive f -> perm[s+i] -> perm[s+i+1] -> ... on one charge, stopping after the j-th customer.
      auto f_id = cs_mat_.ind_to_id(f);
      auto first = perm[s + i];
      auto energy = inst.energy_required(f_id, first);
      if (energy > battery) continue;
      auto dist = base + inst.distance(f_id, first);

      for (auto j = i + 1; j <= k; ++j) {
        auto cust = perm[s + j - 1];
        if (j > i + 1) {
          auto prev = perm[s + j - 2];
          energy += inst.energy_required(prev, cust);
          if (energy > battery) break;
          dist += inst.distance(prev, cust);
        }

        // Leave the j-th customer towards charging node g (for j == k, g = depot closes the route).
        for (auto g = 0UZ; g < m; ++g) {
          auto g_id = cs_mat_.ind_to_id(g);
          if (inst.energy_required(cust, g_id) + energy > battery) continue;
          auto candidate = dist + inst.distance(cust, g_id);
          auto idx = j * m + g;
          if (candidate < arrive_[idx]) {
            arrive_[idx] = candidate;
            arrive_from_layer_[idx] = i;
            arrive_from_cs_[idx] = f;
          }
        }
      }
    }
  }

  close_layer_(k);
  return closed_[k * m + 0];  // back at the depot after the last customer
}

// closed[layer][g] = min over f of arrive[layer][f] + F(f, g): follow a shortest chain of charging nodes.
auto evrp::MontoyaDecode::close_layer_(size_t layer) -> void {
  const auto m = cs_mat_.size();
  for (auto g = 0UZ; g < m; ++g) {
    auto best = arrive_[layer * m + g];
    auto best_from = g;
    for (auto f = 0UZ; f < m; ++f) {
      auto value = arrive_[layer * m + f] + cs_mat_.distance(f, g);
      if (value < best) {
        best = value;
        best_from = f;
      }
    }
    closed_[layer * m + g] = best;
    closed_from_cs_[layer * m + g] = best == kInf ? kNone : best_from;
  }
}

// Appends the optimally charged route for perm[s..t-1]. `out` must already end with the depot.
auto evrp::MontoyaDecode::append_repaired_route_(std::vector<size_t> const &perm, size_t s, size_t t,
                                                 std::vector<size_t> &out) -> void {
  repair_(perm, s, t);  // refill the predecessor tables for this route
  const auto m = cs_mat_.size();

  // After customer `layer`, the vehicle arrives at `arrive_cs` and follows a station chain to `leave_cs`.
  struct Stop {
    size_t layer;
    size_t arrive_cs;
    size_t leave_cs;
  };

  auto stops = std::vector<Stop>();
  auto layer = t - s;
  auto leave = 0UZ;  // the route ends at the depot
  while (true) {
    auto arrive = closed_from_cs_[layer * m + leave];
    assert(arrive != kNone);
    stops.push_back({layer, arrive, leave});
    if (layer == 0) break;
    auto idx = layer * m + arrive;
    leave = arrive_from_cs_[idx];
    layer = arrive_from_layer_[idx];
  }

  auto prev_layer = 0UZ;
  for (auto it = stops.rbegin(); it != stops.rend(); ++it) {
    if (it->layer == 0) {
      append_cs_path_(out, 0, it->leave_cs);  // depot -> ... -> first charging node used (often the depot itself)
      continue;
    }
    for (auto j = prev_layer; j < it->layer; ++j) out.push_back(perm[s + j]);
    out.push_back(cs_mat_.ind_to_id(it->arrive_cs));
    append_cs_path_(out, it->arrive_cs, it->leave_cs);
    prev_layer = it->layer;
  }
}

// Appends the shortest charging-node chain from `from_ind` to `to_ind`, excluding `from_ind` itself.
auto evrp::MontoyaDecode::append_cs_path_(std::vector<size_t> &out, size_t from_ind, size_t to_ind) -> void {
  if (from_ind == to_ind) return;
  path_buf_.clear();
  cs_mat_.trace(path_buf_, from_ind, to_ind);
  if (path_buf_.size() > 1) out.insert(out.end(), path_buf_.begin() + 1, path_buf_.end());
}
