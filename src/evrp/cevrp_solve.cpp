// Compiled into cevrp_lib (shared library, -fvisibility=hidden).
// All cevrp symbols — including the cevrp Case class — are hidden.
// Only the exported functions below are visible to the main binary.

#include <cfloat>
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

// Copy of cevrp's tryACertainN (simple-enumeration variant) with the route
// reconstruction restored; upstream has it commented out and only returns the
// fitness, which is all CACO::fixOneSolution needs.
void caco_try_n(int mlen, int nlen, int *chosenPos, std::vector<int> &finalRoute, double &finalfit, int curub,
                std::vector<int> &route, std::vector<double> &accumulateDis, Case *instance) {
  for (int i = mlen; i <= (int)route.size() - 1 - nlen; i++) {
    if (curub == nlen) {
      double onedis = instance->distances[route[i]][instance->bestStation[route[i]][route[i + 1]]];
      if (accumulateDis[i] + onedis > instance->maxDis) {
        break;
      }
    } else {
      int lastpos = chosenPos[curub - nlen - 1];
      double onedis = instance->distances[route[lastpos + 1]][instance->bestStation[route[lastpos]][route[lastpos + 1]]];
      double twodis = instance->distances[route[i]][instance->bestStation[route[i]][route[i + 1]]];
      if (accumulateDis[i] - accumulateDis[lastpos + 1] + onedis + twodis > instance->maxDis) {
        break;
      }
    }
    if (nlen == 1) {
      double onedis = accumulateDis.back() - accumulateDis[i + 1] +
                      instance->distances[instance->bestStation[route[i]][route[i + 1]]][route[i + 1]];
      if (onedis > instance->maxDis) {
        continue;
      }
    }

    chosenPos[curub - nlen] = i;
    if (nlen > 1) {
      caco_try_n(i + 1, nlen - 1, chosenPos, finalRoute, finalfit, curub, route, accumulateDis, instance);
    } else {
      double disum = accumulateDis.back();
      for (int j = 0; j < curub; j++) {
        int firstnode = route[chosenPos[j]];
        int secondnode = route[chosenPos[j] + 1];
        int thestation = instance->bestStation[firstnode][secondnode];
        disum -= instance->distances[firstnode][secondnode];
        disum += instance->distances[firstnode][thestation];
        disum += instance->distances[secondnode][thestation];
      }
      if (disum < finalfit) {
        finalRoute = route;
        for (int j = curub - 1; j >= 0; j--) {
          finalRoute.insert(finalRoute.begin() + chosenPos[j] + 1,
                            instance->bestStation[finalRoute[chosenPos[j]]][finalRoute[chosenPos[j] + 1]]);
        }
        finalfit = disum;
      }
    }
  }
}

// Copy of cevrp's insertStationBySimpleEnumeration that returns the route.
auto caco_simple_enumeration(std::vector<int> route, Case *instance) -> std::pair<std::vector<int>, double> {
  std::vector<double> accumulateDistance(route.size(), 0);
  for (int i = 1; i < (int)route.size(); i++) {
    accumulateDistance[i] = accumulateDistance[i - 1] + instance->distances[route[i]][route[i - 1]];
  }
  if (accumulateDistance.back() <= instance->maxDis) {
    return std::make_pair(route, accumulateDistance.back());
  }

  int ub = (int)(accumulateDistance.back() / instance->maxDis + 1);
  int lb = (int)(accumulateDistance.back() / instance->maxDis);
  std::vector<int> chosenPos(route.size());
  std::vector<int> finalRoute;
  double finalfit = DBL_MAX;
  for (int i = lb; i <= ub; i++) {
    caco_try_n(0, i, chosenPos.data(), finalRoute, finalfit, i, route, accumulateDistance, instance);
  }
  if (finalfit != DBL_MAX) {
    return std::make_pair(finalRoute, finalfit);
  }
  return std::make_pair(route, -1.0);
}

}  // namespace

// ---------------------------------------------------------------------------
// CACO decoder: insertStationBySimpleEnumeration, fallback insertStationByRemove
// ---------------------------------------------------------------------------
__attribute__((visibility("default"))) auto caco_decode_raw(CevrpCase *c, std::vector<size_t> const &routes)
    -> std::vector<size_t> {
  return decode_routes(c, routes, [](std::vector<int> tour, Case *cas) {
    auto [result, dist] = caco_simple_enumeration(tour, cas);
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
