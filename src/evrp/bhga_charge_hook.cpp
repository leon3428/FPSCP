#include "bhga_charge_hook.hpp"

#include <chrono>

#include "case.h"

namespace {

thread_local evrp::ChargeContext *g_charge_ctx = nullptr;

}  // namespace

evrp::ScopedChargeContext::ScopedChargeContext(ChargeContext &ctx) : prev_(g_charge_ctx) { g_charge_ctx = &ctx; }

evrp::ScopedChargeContext::~ScopedChargeContext() { g_charge_ctx = prev_; }

std::pair<std::vector<int>, double> bhga_charge_route(std::vector<int> route, Case &instance) {
  auto *ctx = g_charge_ctx;
  if (ctx == nullptr) return focusEnumeration(std::move(route), instance);

  auto t0 = std::chrono::steady_clock::now();
  auto result = ctx->charge(route, instance);
  ctx->elapsed_ns +=
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
  ++ctx->calls;
  return result;
}

// Mirrors calCost in extern/bhga/Program/case.cpp.
double bhga_cal_cost(std::vector<std::vector<int>> &all_routes, Case &instance) {
  double cost = 0;
  for (auto route : all_routes) {
    if (route.empty()) continue;
    route.insert(route.begin(), 0);
    route.push_back(0);
    cost += bhga_charge_route(route, instance).second;
  }
  return cost;
}
