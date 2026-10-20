#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "instance.hpp"

namespace evrp {

struct BhgaRunConfig {
  std::string vrp_path;   // CVRPLIB .vrp file used by the HGS core
  std::string evrp_path;  // matching .evrp file used for charging
  std::string decoder = "vanilla";  // "vanilla" (focusEnumeration) or "ss_fr_fla" (BatteryDecode, single CS)
  int seed = 0;
  int nb_iter = 30000;      // iterations without improvement before stopping
  double time_limit = 0.0;  // CPU seconds, 0 = no limit
  std::string mix = "12116";  // same meaning as BHGA's -mix flag
};

struct BhgaRunResult {
  double cost = 0.0;       // total distance of `routes` on the real instance
  double bhga_cost = 0.0;  // penalized cost of the best individual as seen by BHGA
  bool energy_feasible = false;
  std::vector<size_t> routes;           // flat, depots and charging stations included
  std::vector<size_t> customer_routes;  // flat, depots and customers only
  int iterations = 0;
  double elapsed_s = 0.0;  // wall time
  double cpu_s = 0.0;      // process CPU time, the clock BHGA itself uses
  uint64_t charge_calls = 0;
  uint64_t charge_ns = 0;
  std::vector<std::tuple<int, double, double>> progress;  // (iteration, best cost, CPU seconds)
};

// Runs the full BHGA genetic algorithm with the selected charging decoder.
// Uses process CPU time internally, so run one GA per process when using a time
// limit or when comparing CPU times.
auto run_bhga(BhgaRunConfig const &config, std::shared_ptr<Instance> const &instance) -> BhgaRunResult;

}  // namespace evrp
