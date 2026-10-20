#pragma once

// Redirects BHGA's charging-station insertion to a pluggable decoder.
//
// The BHGA GA sources (target bhga_ga) are compiled with
//   -DfocusEnumeration=bhga_charge_route -DcalCost=bhga_cal_cost
// so every call they make to the upstream charging functions ends up here.
// case.cpp itself is compiled unchanged, which keeps the original
// focusEnumeration available for the vanilla configuration.

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

class Case;

// Same signatures as focusEnumeration / calCost in extern/bhga/Program/case.h.
std::pair<std::vector<int>, double> bhga_charge_route(std::vector<int> route, Case &instance);
double bhga_cal_cost(std::vector<std::vector<int>> &all_routes, Case &instance);

namespace evrp {

// Takes a route starting and ending at the depot ([0, c1, ..., ck, 0]) and
// returns the route with charging stations inserted and its distance. An
// infeasible route must return INT_MAX as the distance, which is what BHGA's
// local search expects.
using ChargeFn = std::function<std::pair<std::vector<int>, double>(std::vector<int> const &, Case &)>;

struct ChargeContext {
  ChargeFn charge;
  uint64_t calls = 0;
  uint64_t elapsed_ns = 0;
};

// Installs a charge context for the current thread while in scope. Without an
// active context bhga_charge_route falls back to the upstream focusEnumeration.
class ScopedChargeContext {
 public:
  explicit ScopedChargeContext(ChargeContext &ctx);
  ~ScopedChargeContext();
  ScopedChargeContext(ScopedChargeContext const &) = delete;
  auto operator=(ScopedChargeContext const &) -> ScopedChargeContext & = delete;

 private:
  ChargeContext *prev_;
};

}  // namespace evrp
