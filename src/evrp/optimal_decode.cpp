#include "optimal_decode.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <limits>
#include <map>
#include <numeric>
#include <ranges>
#include <span>

evrp::SolutionCandidate::SolutionCandidate()
    : distance(0.0),
      battery_used(0.0),
      cargo_used(0.0),
      prev(std::numeric_limits<size_t>::max()),
      cs_ind_in(0),
      cs_ind_out(0),
      jump_case(JumpCase::None) {}
evrp::SolutionCandidate::SolutionCandidate(double distance_, double battery_used_, double cargo_used_)
    : distance(distance_),
      battery_used(battery_used_),
      cargo_used(cargo_used_),
      prev(std::numeric_limits<size_t>::max()),
      cs_ind_in(0),
      cs_ind_out(0),
      jump_case(JumpCase::None) {}

auto evrp::SolutionCandidate::dominates(SolutionCandidate const &other) const -> bool {
  if (battery_used == other.battery_used && cargo_used == other.cargo_used && distance == other.distance) return false;

  return battery_used <= other.battery_used && cargo_used <= other.cargo_used && distance <= other.distance;
}

auto evrp::ParetoFront::colapse() -> void {
  std::ranges::sort(front_, [](const auto &a, const auto &b) {
    if (a.distance != b.distance) return a.distance < b.distance;
    if (a.battery_used != b.battery_used) return a.battery_used < b.battery_used;

    return a.cargo_used < b.cargo_used;
  });

  auto skyline = std::map<double, double>();
  auto new_front = std::vector<SolutionCandidate>();

  for (const auto &f : front_) {
    // std::println("{} {} {}", f.distance, f.battery_used, f.cargo_used);

    auto it = skyline.upper_bound(f.battery_used);
    if (!skyline.empty() && it != skyline.begin()) {
      auto prev = std::prev(it);
      if (prev->second <= f.cargo_used) {
        continue;
      }
    }

    new_front.push_back(f);

    while (it != skyline.end() && it->second >= f.cargo_used) {
      it = skyline.erase(it);
    }

    skyline[f.battery_used] = f.cargo_used;
  }

  front_ = std::move(new_front);
  valid_ = true;
}

evrp::CsMatrix::CsMatrix(std::shared_ptr<Instance> instance)
    : cs_cnt_(instance->charging_station_cnt() + 1),
      instance_(instance),
      dist_mat_(cs_cnt_, std::vector(cs_cnt_, std::numeric_limits<double>::infinity())),
      prev_(cs_cnt_, std::vector(cs_cnt_, std::numeric_limits<size_t>::max())) {
  floyd_warshall_();
}

auto evrp::CsMatrix::floyd_warshall_() -> void {
  for (auto i = 0UZ; i < cs_cnt_; ++i) {
    dist_mat_[i][i] = 0.0;
    prev_[i][i] = i;

    for (auto j = i + 1; j < cs_cnt_; ++j) {
      auto node1 = ind_to_id(i);
      auto node2 = ind_to_id(j);

      auto distance = instance_->distance(node1, node2);
      if (instance_->energy_required(node1, node2) <= instance_->battery_capacity()) {
        dist_mat_[i][j] = distance;
        dist_mat_[j][i] = distance;
        prev_[i][j] = i;
        prev_[j][i] = j;
      }
    }
  }

  for (auto k = 0UZ; k < cs_cnt_; ++k) {
    for (auto i = 0UZ; i < cs_cnt_; ++i) {
      for (auto j = 0UZ; j < cs_cnt_; ++j) {
        if (dist_mat_[i][j] > dist_mat_[i][k] + dist_mat_[k][j]) {
          dist_mat_[i][j] = dist_mat_[i][k] + dist_mat_[k][j];
          prev_[i][j] = prev_[k][j];
        }
      }
    }
  }
}

auto evrp::CsMatrix::trace(std::vector<size_t> &route, size_t ind1, size_t ind2) -> void {
  if (prev_[ind1][ind2] == std::numeric_limits<size_t>::max()) {
    return;
  }

  // Collect path in reverse (ind2 -> ... -> ind1)
  auto path = std::vector<size_t>();
  auto curr = ind2;
  path.push_back(ind_to_id(curr));
  while (curr != ind1) {
    curr = prev_[ind1][curr];
    path.push_back(ind_to_id(curr));
  }

  // Append in forward order (ind1 -> ... -> ind2)
  for (auto ind : std::views::reverse(path)) {
    route.push_back(ind);
  }
}

evrp::ChargingPrefixTable::ChargingPrefixTable(Instance const &instance, CsMatrix const &cs_mat)
    : cs_cnt_(cs_mat.size()), battery_capacity_(instance.battery_capacity()) {
  const auto node_cnt = instance.customer_cnt() + 1;
  assert(cs_cnt_ <= std::numeric_limits<uint8_t>::max() + 1UZ);

  energy_.resize(node_cnt * cs_cnt_);
  order_.resize(node_cnt * cs_cnt_);
  entries_.resize(node_cnt * cs_cnt_ * cs_cnt_);

  auto order = std::vector<size_t>(cs_cnt_);
  for (auto p = 0UZ; p < node_cnt; ++p) {
    std::ranges::iota(order, 0UZ);
    std::ranges::stable_sort(order, {}, [&](size_t f) { return instance.energy_required(p, cs_mat.ind_to_id(f)); });

    for (auto r = 0UZ; r < cs_cnt_; ++r) {
      auto f = order[r];
      auto f_id = cs_mat.ind_to_id(f);
      energy_[energy_index_(p, r)] = instance.energy_required(p, f_id);
      order_[energy_index_(p, r)] = static_cast<uint8_t>(f);

      for (auto g = 0UZ; g < cs_cnt_; ++g) {
        auto &entry = entries_[entry_index_(p, r, g)];
        entry = {instance.distance(p, f_id) + cs_mat.distance(f, g), static_cast<uint8_t>(f)};
        // One more entry point is available than in the previous prefix; keep the previous best if it is not worse
        if (r > 0 && entries_[entry_index_(p, r - 1, g)].distance <= entry.distance) {
          entry = entries_[entry_index_(p, r - 1, g)];
        }
      }
    }
  }
}

auto evrp::ChargingPrefixTable::reachable_cnt(size_t p, double battery_used) const -> size_t {
  auto energies = std::span(energy_).subspan(energy_index_(p, 0), cs_cnt_);
  auto end = std::ranges::partition_point(energies, [&](double e) { return e + battery_used <= battery_capacity_; });
  return static_cast<size_t>(end - energies.begin());
}

evrp::OptimalDecode::OptimalDecode(std::shared_ptr<Instance> instance)
    : instance_(instance),
      cs_mat_(instance),
      prefix_table_(*instance, cs_mat_),
      fronts_(instance_->customer_cnt() + 2) {}

auto evrp::OptimalDecode::decode(Solution const &solution) -> Solution {
  auto t0 = std::chrono::steady_clock::now();
  for (auto &front : fronts_) front.clear();
  forward_pass_(solution);
  auto result = backward_pass_(solution);
  last_elapsed_ns_ =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
  return result;
}

auto evrp::OptimalDecode::forward_pass_(Solution const &solution) -> void {
  assert(solution.routes().size() == instance_->customer_cnt());
  const auto &perm = solution.routes();
  const auto battery = instance_->battery_capacity();
  const auto cargo_capacity = instance_->cargo_capacity();
  constexpr auto inf = std::numeric_limits<double>::infinity();

  fronts_[0].emplace(0.0, 0.0, 0.0);
  fronts_[0].colapse();

  for (auto i = 0UZ; i <= instance_->customer_cnt(); ++i) {
    auto prev_node = i == 0 ? instance_->depot_id() : perm[i - 1];
    auto curr_node = i == instance_->customer_cnt() ? instance_->depot_id() : perm[i];
    auto demand = instance_->demand(curr_node);

    auto &prev_front = fronts_[i];
    auto &curr_front = fronts_[i + 1];

    // Exit charging points that reach the next node on a full battery. These do not depend on the label.
    exits_.clear();
    for (auto g = 0UZ; g < cs_mat_.size(); ++g) {
      if (instance_->energy_required(cs_mat_.ind_to_id(g), curr_node) <= battery) exits_.push_back(g);
    }

    // Depot detours reset the cargo, so all labels lead to the same candidates up to distance: keep only the best.
    auto depot_distance = inf;
    auto depot_prev = 0UZ;
    auto depot_cs_in = uint8_t{0};

    for (auto sol_id = 0UZ; sol_id < prev_front.size(); ++sol_id) {
      const auto &sol = prev_front[sol_id];

      // Option 1 - Direct rute
      {
        auto prop = sol;
        prop.distance += instance_->distance(prev_node, curr_node);
        prop.cargo_used += instance_->demand(curr_node);
        prop.battery_used += instance_->energy_required(prev_node, curr_node);
        prop.prev = sol_id;
        prop.jump_case = JumpCase::Direct;

        if (prop.cargo_used <= cargo_capacity && prop.battery_used <= battery) {
          curr_front.emplace(prop);
        }
      }

      // Charging points reachable with the remaining battery form a prefix of the sorted order
      auto reach = prefix_table_.reachable_cnt(prev_node, sol.battery_used);
      if (reach == 0) continue;

      // Option 2 - Charging detour, best entry point for every exit point
      if (sol.cargo_used + demand <= cargo_capacity) {
        for (auto g : exits_) {
          const auto &entry = prefix_table_.best_entry(prev_node, reach, g);
          if (entry.distance == inf) continue;
          auto g_id = cs_mat_.ind_to_id(g);

          auto prop = sol;
          prop.distance += entry.distance + instance_->distance(g_id, curr_node);
          prop.battery_used = instance_->energy_required(g_id, curr_node);
          prop.cargo_used += demand;
          prop.prev = sol_id;
          prop.jump_case = JumpCase::Charging;
          prop.cs_ind_in = entry.cs_ind;
          prop.cs_ind_out = static_cast<uint8_t>(g);
          curr_front.emplace(prop);
        }
      }

      // Option 3 - Depot detour, the entry point leading to the depot
      const auto &to_depot = prefix_table_.best_entry(prev_node, reach, 0);
      if (sol.distance + to_depot.distance < depot_distance) {
        depot_distance = sol.distance + to_depot.distance;
        depot_prev = sol_id;
        depot_cs_in = to_depot.cs_ind;
      }
    }

    if (depot_distance < inf && demand <= cargo_capacity) {
      for (auto g : exits_) {
        if (cs_mat_.distance(0, g) == inf) continue;
        auto g_id = cs_mat_.ind_to_id(g);

        auto prop = SolutionCandidate(depot_distance + cs_mat_.distance(0, g) + instance_->distance(g_id, curr_node),
                                      instance_->energy_required(g_id, curr_node), demand);
        prop.prev = depot_prev;
        prop.jump_case = JumpCase::Depot;
        prop.cs_ind_in = depot_cs_in;
        prop.cs_ind_out = static_cast<uint8_t>(g);
        curr_front.emplace(prop);
      }
    }

    // Remove dominated solutions
    curr_front.colapse();
  }
}

auto evrp::OptimalDecode::backward_pass_(Solution const &solution) -> Solution {
  const auto &perm = solution.routes();

  // Find the best (lowest distance) solution in the last front
  auto sol_ind = 0UZ;
  auto &last_front = fronts_.back();
  if (last_front.size() == 0) return solution;
  for (auto i = 0UZ; i < last_front.size(); ++i) {
    if (last_front[i].distance < last_front[sol_ind].distance) {
      sol_ind = i;
    }
  }

  auto indices = std::vector<size_t>();

  for (size_t i = fronts_.size() - 1; i >= 1; --i) {
    indices.push_back(sol_ind);
    const auto &sol = fronts_[i][sol_ind];
    sol_ind = sol.prev;
  }

  // Build route forward: depot, [cs_path], customer, [cs_path], customer, ..., [cs_path], depot
  auto new_routes = std::vector<size_t>();
  new_routes.push_back(instance_->depot_id());

  for (auto [step, ind] : std::views::enumerate(std::views::reverse(indices))) {
    auto curr_node = step < perm.size() ? perm[step] : instance_->depot_id();
    const auto &sol = fronts_[step + 1][ind];

    if (sol.jump_case == JumpCase::Charging) {
      cs_mat_.trace(new_routes, sol.cs_ind_in, sol.cs_ind_out);
    } else if (sol.jump_case == JumpCase::Depot) {
      cs_mat_.trace(new_routes, sol.cs_ind_in, 0);
      cs_mat_.trace(new_routes, 0, sol.cs_ind_out);
    }

    new_routes.push_back(curr_node);
  }

  return Solution(solution.instance_ptr(), std::move(new_routes));
}