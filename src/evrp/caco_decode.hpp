#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace evrp {

// Inserts charging stations into existing routes using CACO's
// insertStationBySimpleEnumeration (fallback: insertStationByRemove). The
// enumeration is a local copy with route reconstruction restored, since the
// upstream version only returns the fitness.
// Loads the instance directly from an EVRP file.
class CacoDecode {
 public:
  explicit CacoDecode(std::string const& evrp_path);
  ~CacoDecode();

  // Takes a flat route (depot=0 markers, customers without charging stations)
  // and returns a flat route with charging stations inserted, plus total cost.
  auto decode(std::vector<size_t> const& routes) -> std::vector<size_t>;
  [[nodiscard]] auto last_elapsed_ns() const -> uint64_t { return last_elapsed_ns_; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  uint64_t last_elapsed_ns_ = 0;
};

}  // namespace evrp
