#include "bhga_ga.hpp"

#include <algorithm>
#include <chrono>
#include <climits>
#include <ctime>
#include <limits>
#include <stdexcept>

#include "bhga_charge_hook.hpp"
#include "solution.hpp"
#include "split_decode.hpp"

// BHGA headers last: they pull in `using namespace std` and a few macros.
#include "Genetic.h"
#include "InstanceCVRPLIB.h"
#include "case.h"

namespace {

using Progress = std::vector<std::tuple<int, double, double>>;

// Population::searchProgress is private and there is no accessor upstream.
// Explicit template instantiation is allowed to name private members, which
// gives read access without modifying the submodule.
auto search_progress(Population &population) -> Progress &;

template <Progress Population::*Member>
struct SearchProgressAccess {
  friend auto search_progress(Population &population) -> Progress & { return population.*Member; }
};

template struct SearchProgressAccess<&Population::searchProgress>;

constexpr auto kInfeasibleCost = static_cast<double>(INT_MAX);

auto apply_mix(std::string const &mix, AlgorithmParameters &ap) -> int {
  // Same layout as BHGA's -mix flag (commandline.h):
  // rounding, hou, mode, preCharge, numMoves.
  if (mix.size() != 5 || !std::ranges::all_of(mix, [](char c) { return c >= '0' && c <= '9'; })) {
    throw std::invalid_argument("mix must be exactly 5 digits, got '" + mix + "'");
  }
  ap.hou = mix[1] - '0';
  ap.mode = mix[2] - '0';
  ap.preCharge = mix[3] - '0';
  ap.numMoves = mix[4] - '0';
  // Modes 0 and 2 evaluate individuals through chroSplit_new5, which calls the
  // upstream focusEnumeration directly and would bypass the decoder hook.
  if (ap.mode != 1) throw std::invalid_argument("only BHGA mode 1 is supported, got mix '" + mix + "'");
  return mix[0] - '0';
}

// Copy of the instance without a cargo limit. BHGA's split uses a soft
// capacity penalty, so the charging decoder also sees overloaded routes;
// focusEnumeration ignores cargo and so must the replacement.
auto uncapped_instance(evrp::Instance &instance) -> std::shared_ptr<evrp::Instance> {
  return std::make_shared<evrp::Instance>(instance.name(), 0.0, 0, std::numeric_limits<double>::infinity(),
                                          instance.battery_capacity(), instance.energy_consumption(),
                                          instance.customer_cnt(), instance.charging_station_cnt(), instance.nodes());
}

auto make_charge_fn(std::string const &decoder, std::shared_ptr<evrp::Instance> const &instance) -> evrp::ChargeFn {
  if (decoder == "vanilla") {
    return [](std::vector<int> const &route, Case &c) { return focusEnumeration(route, c); };
  }
  if (decoder == "ss_fr_fla") {
    auto uncapped = uncapped_instance(*instance);
    auto battery = std::make_shared<evrp::BatteryDecode>(uncapped);
    return [uncapped, battery](std::vector<int> const &route, Case &) {
      auto decoded = battery->decode(evrp::Solution(uncapped, std::vector<size_t>(route.begin(), route.end())),
                                     std::numeric_limits<double>::infinity(), true);
      if (!decoded.is_energy_and_cargo_valid()) return std::make_pair(route, kInfeasibleCost);
      auto const &nodes = decoded.routes();
      return std::make_pair(std::vector<int>(nodes.begin(), nodes.end()), decoded.cost());
    };
  }
  throw std::invalid_argument("unknown decoder '" + decoder + "', expected 'vanilla' or 'ss_fr_fla'");
}

}  // namespace

auto evrp::run_bhga(BhgaRunConfig const &config, std::shared_ptr<Instance> const &instance) -> BhgaRunResult {
  auto wall_t0 = std::chrono::steady_clock::now();
  auto cpu_t0 = std::clock();

  auto ap = default_algorithm_parameters();
  ap.seed = config.seed;
  ap.nbIter = config.nb_iter;
  ap.timeLimit = config.time_limit;
  auto is_rounding_integer = apply_mix(config.mix, ap);

  auto ctx = ChargeContext{.charge = make_charge_fn(config.decoder, instance)};
  auto result = BhgaRunResult();

  try {
    // Mirrors extern/bhga/Program/main.cpp.
    auto cvrp = InstanceCVRPLIB(config.vrp_path, is_rounding_integer);
    auto params = Params(cvrp.x_coords, cvrp.y_coords, cvrp.dist_mtx, cvrp.service_time, cvrp.demands,
                         cvrp.vehicleCapacity, cvrp.durationLimit, INT_MAX, cvrp.isDurationConstraint, false, ap);
    params.mode = ap.mode;
    params.hou = ap.hou;
    params.preCharge = ap.preCharge;
    params.numMoves = ap.numMoves;
    params.c_evrp = Case(config.evrp_path, 2);
    params.selected = ap.selected;

    auto scoped = ScopedChargeContext(ctx);
    auto solver = Genetic(params);
    solver.run();

    result.iterations = params.dai - 1;
    result.charge_calls = ctx.calls;
    result.charge_ns = ctx.elapsed_ns;
    result.progress = search_progress(solver.population);

    auto const *best = solver.population.getBestFound();
    if (best == nullptr) throw std::runtime_error("BHGA found no capacity-feasible solution");
    result.bhga_cost = best->eval.penalizedCost;

    // BHGA never stores the routes with stations, so decode the best routes once more.
    result.energy_feasible = true;
    result.routes.push_back(instance->depot_id());
    result.customer_routes.push_back(instance->depot_id());
    for (auto const &customers : best->chromR) {
      if (customers.empty()) continue;
      auto route = std::vector<int>{0};
      route.insert(route.end(), customers.begin(), customers.end());
      route.push_back(0);

      auto [charged, dist] = ctx.charge(route, params.c_evrp);
      if (dist >= kInfeasibleCost) {
        result.energy_feasible = false;
        charged = route;
      }
      result.routes.insert(result.routes.end(), charged.begin() + 1, charged.end());
      result.customer_routes.insert(result.customer_routes.end(), route.begin() + 1, route.end());
    }
  } catch (std::string const &e) {
    // BHGA reports errors by throwing std::string.
    throw std::runtime_error("BHGA: " + e);
  }

  result.cost = Solution(instance, result.routes).cost();
  result.elapsed_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall_t0).count();
  result.cpu_s = static_cast<double>(std::clock() - cpu_t0) / CLOCKS_PER_SEC;
  return result;
}
