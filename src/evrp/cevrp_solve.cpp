// Compiled into cevrp_lib (shared library, -fvisibility=hidden).
// All cevrp symbols — including the cevrp Case class — are hidden.
// Only the exported functions below are visible to the main binary.

#include <climits>
#include <cstddef>

#include "utilities.h"

// ---------------------------------------------------------------------------
// Opaque Case handle
// ---------------------------------------------------------------------------
struct CevrpCase {
  Case case_;
  explicit CevrpCase(std::string const &evrp_path) : case_(evrp_path, 0) {}
};

__attribute__((visibility("default"))) CevrpCase *cevrp_case_create(std::string const &evrp_path) {
  return new CevrpCase(evrp_path);
}

__attribute__((visibility("default"))) void cevrp_case_destroy(CevrpCase *c) { delete c; }

// ---------------------------------------------------------------------------
// Shared helper: split flat routes into sub-tours and assemble CevrpResult
// ---------------------------------------------------------------------------
namespace {

// Calls insert_fn(tour, case) for each sub-tour, assembles the flat result.
template <typename InsertFn>
auto decode_routes(CevrpCase *c, std::vector<size_t> const &routes, InsertFn insert_fn) -> std::vector<size_t> {
  auto depot = c->case_.depot;

  // Split flat routes into individual sub-tours (strips between depot markers).
  auto sub_tours = std::vector<std::vector<int>>();
  auto current = std::vector<int>();
  current.push_back(depot);
  for (auto node : routes) {
    if (node == depot) {
      if (current.size() > 1) {
        current.push_back(depot);
        sub_tours.push_back(std::move(current));
        current.clear();
        current.push_back(depot);
      }
    } else {
      current.push_back(static_cast<int>(node));
    }
  }
  if (current.size() > 1) {
    current.push_back(depot);
    sub_tours.push_back(std::move(current));
  }

  auto new_routes = std::vector<size_t>();
  auto first = true;

  for (auto &sub : sub_tours) {
    auto [result, dist] = insert_fn(sub, &c->case_);
    auto valid = dist >= 0 && dist < static_cast<double>(INFEASIBLE);

    std::size_t start = first ? 0 : 1;
    first = false;
    if (valid) {
      for (std::size_t i = start; i < result.size(); ++i) new_routes.push_back(result[i]);
    } else {
      for (std::size_t i = start; i < sub.size(); ++i) new_routes.push_back(sub[i]);
    }
  }

  if (!new_routes.empty() && new_routes.back() != depot) new_routes.push_back(depot);
  return new_routes;
}

}  // namespace

// ---------------------------------------------------------------------------
// CACO decoder: insertStationBySimpleEnumeration, fallback insertStationByRemove
// ---------------------------------------------------------------------------
__attribute__((visibility("default"))) auto caco_decode_raw(CevrpCase *c, std::vector<size_t> const &routes)
    -> std::vector<size_t> {
  return decode_routes(c, routes, [](std::vector<int> tour, Case *cas) {
    auto [result, dist] = insertStationBySimpleEnumeration(tour, cas);
    if (dist < 0 || dist >= static_cast<double>(INFEASIBLE)) return insertStationByRemove(tour, cas);
    return std::make_pair(result, dist);
  });
}

// ---------------------------------------------------------------------------
// BACO2 decoder: insertStationByRemove
// ---------------------------------------------------------------------------
__attribute__((visibility("default"))) auto baco2_decode_raw(CevrpCase *c, std::vector<size_t> const &routes)
    -> std::vector<size_t> {
  return decode_routes(c, routes, [](std::vector<int> tour, Case *cas) { return insertStationByRemove(tour, cas); });
}
