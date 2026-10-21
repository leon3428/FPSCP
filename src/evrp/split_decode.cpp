#include "split_decode.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <deque>
#include <limits>
#include <ranges>

auto evrp::linear_split(Solution const &solution) -> Solution {
  const auto &instance = solution.instance();
  assert(solution.routes().size() == instance.customer_cnt());
  auto &tour = solution.routes();

  auto lambda = std::deque<size_t>();
  lambda.push_back(0UZ);

  auto d = std::vector(instance.customer_cnt(), 0.0);
  auto q = std::vector(instance.customer_cnt(), 0.0);
  q[0] = instance.demand(tour[0]);

  for (auto i = 1UZ; i < instance.customer_cnt(); ++i) {
    d[i] = d[i - 1] + instance.distance(tour[i - 1], tour[i]);
    q[i] = q[i - 1] + instance.demand(tour[i]);
  }

  auto p = std::vector(instance.customer_cnt() + 1, std::numeric_limits<double>::infinity());
  auto pred = std::vector(instance.customer_cnt() + 1, 0UZ);
  p[0] = 0;

  auto cost = [&](size_t i, size_t j) {
    auto ret = instance.distance(instance.depot_id(), tour[i]);
    ret += d[j - 1] - d[i];
    if (j != 0) {
      ret += instance.distance(tour[j - 1], instance.depot_id());
    }
    return ret;
  };

  auto dominates = [&](size_t i, size_t j) {
    if (p[i] + instance.distance(instance.depot_id(), tour[i]) - d[i] <=
        p[j] + instance.distance(instance.depot_id(), tour[j]) - d[j]) {
      if ((i <= j && q[i] == q[j]) || i > j) return true;
    }
    return false;
  };

  for (auto t = 1UZ; t <= instance.customer_cnt(); ++t) {
    p[t] = p[lambda.front()] + cost(lambda.front(), t);
    pred[t] = lambda.front();

    if (t < instance.customer_cnt()) {
      if (!dominates(lambda.back(), t)) {
        while (!lambda.empty() && dominates(t, lambda.back())) {
          lambda.pop_back();
        }
        lambda.push_back(t);
      }

      while (q[t] > instance.cargo_capacity() + (lambda.front() == 0 ? 0.0 : q[lambda.front() - 1])) {
        lambda.pop_front();
      }
    }
  }

  // Collect split points by walking pred backwards
  auto splits = std::vector<size_t>();
  splits.push_back(instance.customer_cnt());
  auto i = pred.back();
  while (i != 0) {
    splits.push_back(i);
    i = pred[i];
  }

  // Build new routes vector with depot IDs between segments
  auto new_routes = std::vector<size_t>();
  size_t seg_start = 0;
  for (auto seg_end : std::views::reverse(splits)) {
    new_routes.push_back(instance.depot_id());
    for (auto j = seg_start; j < seg_end; ++j) {
      new_routes.push_back(tour[j]);
    }
    seg_start = seg_end;
  }
  new_routes.push_back(instance.depot_id());

  return Solution(solution.instance_ptr(), std::move(new_routes));
}

auto evrp::ParetoFront2D::colapse() -> void {
  std::ranges::sort(front_, [](const auto &a, const auto &b) {
    if (a.distance != b.distance) return a.distance < b.distance;
    if (a.battery_used != b.battery_used) return a.battery_used < b.battery_used;

    return a.cargo_used < b.cargo_used;
  });

  auto new_front = std::vector<SolutionCandidate>();
  auto min_battery = std::numeric_limits<double>::infinity();
  for (const auto &f : front_) {
    if (f.battery_used < min_battery) {
      new_front.push_back(f);
      min_battery = f.battery_used;
    }
  }

  front_ = std::move(new_front);
  valid_ = true;
}

evrp::BatteryDecode::BatteryDecode(std::shared_ptr<Instance> instance) : instance_(instance), cs_mat_(instance) {}

auto evrp::BatteryDecode::decode(Solution const &solution, bool single_cs) -> Solution {
  auto t0 = std::chrono::steady_clock::now();
  fronts_.resize(solution.routes().size());
  for (auto &front : fronts_) front.clear();
  forward_pass_(solution, single_cs);
  auto result = backward_pass_(solution);
  last_elapsed_ns_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - t0).count();
  return result;
}

auto evrp::BatteryDecode::forward_pass_(Solution const &solution, bool single_cs) -> void {
  const auto &perm = solution.routes();

  fronts_[0].emplace(0.0, 0.0, 0.0);
  fronts_[0].colapse();

  for (auto i = 1UZ; i < solution.routes().size(); ++i) {
    auto prev_node = perm[i - 1];
    auto curr_node = perm[i];

    auto &prev_front = fronts_[i - 1];
    auto &curr_front = fronts_[i];

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

        if (prop.cargo_used <= instance_->cargo_capacity() && prop.battery_used <= instance_->battery_capacity()) {
          if (curr_node == instance_->depot_id()) {
            prop.battery_used = 0;
            prop.cargo_used = 0;
          }
          curr_front.emplace(prop);
        }
      }

      for (auto cs_ind_in = 0UZ; cs_ind_in < cs_mat_.size(); ++cs_ind_in) {
        // Skip this entry cs if we can't reach it with the remaining battery
        auto cs_id_in = cs_mat_.ind_to_id(cs_ind_in);
        if (instance_->energy_required(prev_node, cs_id_in) + sol.battery_used > instance_->battery_capacity()) {
          continue;
        }

        for (auto cs_ind_out = single_cs ? cs_ind_in : 0UZ; cs_ind_out < (single_cs ? cs_ind_in + 1 : cs_mat_.size()); ++cs_ind_out) {
          // Skip this exit cs if we cannot reach the next node. Remember that we leave the cs with a full battery
          auto cs_id_out = cs_mat_.ind_to_id(cs_ind_out);
          if (instance_->energy_required(cs_id_out, curr_node) > instance_->battery_capacity()) {
            continue;
          }

          // Option 2 - Charging detour
          {
            auto prop = sol;
            prop.distance += instance_->distance(prev_node, cs_id_in);
            prop.distance += cs_mat_.distance(cs_ind_in, cs_ind_out);
            prop.distance += instance_->distance(cs_id_out, curr_node);
            prop.battery_used = instance_->energy_required(cs_id_out, curr_node);
            prop.cargo_used += instance_->demand(curr_node);
            prop.prev = sol_id;
            prop.jump_case = JumpCase::Charging;
            prop.cs_ind_in = cs_ind_in;
            prop.cs_ind_out = cs_ind_out;

            if (prop.cargo_used <= instance_->cargo_capacity() && prop.battery_used <= instance_->battery_capacity()) {
              if (curr_node == instance_->depot_id()) {
                prop.battery_used = 0;
                prop.cargo_used = 0;
              }
              curr_front.emplace(prop);
            }
          }
        }
      }
    }
    // Remove dominated solutions
    curr_front.colapse();
  }
}

auto evrp::BatteryDecode::backward_pass_(Solution const &solution) -> Solution {
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
    auto curr_node = perm[step + 1];
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