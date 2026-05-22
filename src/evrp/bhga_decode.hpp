#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace evrp {

// Inserts charging stations into existing routes using bhga's focusEnumeration.
// Loads the instance directly from an EVRP file.
class BhgaDecode {
 public:
  explicit BhgaDecode(std::string const& evrp_path);
  ~BhgaDecode();

  // Takes a flat route (depot=0 markers, customers and charging stations as
  // node indices) and returns an improved flat route with charging stations
  // inserted optimally, together with the total route cost.
  auto decode(std::vector<size_t> const& routes) -> std::vector<size_t>;
  [[nodiscard]] auto last_elapsed_ns() const -> uint64_t { return last_elapsed_ns_; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  uint64_t last_elapsed_ns_ = 0;
};

}  // namespace evrp
