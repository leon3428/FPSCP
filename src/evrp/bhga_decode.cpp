#include "bhga_decode.hpp"

#include <chrono>
#include <climits>
#include <print>

#include "case.h"

namespace evrp {

struct BhgaDecode::Impl {
  Case case_;
};

BhgaDecode::BhgaDecode(std::string const &evrp_path) : impl_(std::make_unique<Impl>()) {
  impl_->case_ = Case(evrp_path, 0);
}

BhgaDecode::~BhgaDecode() = default;

auto BhgaDecode::decode(std::vector<size_t> const &routes) -> std::vector<size_t> {
  auto t0 = std::chrono::steady_clock::now();
  auto &c = impl_->case_;

  // Split flat routes into individual sub-tours (strips between depot markers).
  auto sub_tours = std::vector<std::vector<int>>();
  auto current = std::vector<int>();
  current.push_back(c.depot);
  for (auto node : routes) {
    if (node == c.depot) {
      if (current.size() > 1) {
        current.push_back(c.depot);
        sub_tours.push_back(std::move(current));
        current.clear();
        current.push_back(c.depot);
      }
    } else {
      current.push_back(static_cast<int>(node));
    }
  }
  if (current.size() > 1) {
    current.push_back(c.depot);
    sub_tours.push_back(std::move(current));
  }

  auto new_routes = std::vector<size_t>();
  bool first = true;

  for (auto &sub : sub_tours) {
    auto [result, dist] = focusEnumeration(sub, c);
    // Append to flat route; skip the leading depot of all but the first
    // sub-tour to avoid consecutive depot markers at boundaries.
    std::size_t start = first ? 0 : 1;
    first = false;
    for (std::size_t i = start; i < result.size(); ++i) {
      new_routes.push_back(static_cast<size_t>(result[i]));
    }
  }

  if (!new_routes.empty() && new_routes.back() != c.depot) {
    new_routes.push_back(static_cast<size_t>(c.depot));
  }

  last_elapsed_ns_ =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
  return new_routes;
}

}  // namespace evrp
