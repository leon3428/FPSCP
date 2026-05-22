#include "caco_decode.hpp"
#include "cevrp_solve.hpp"

#include <chrono>

namespace evrp {

struct CacoDecode::Impl {
  CevrpCase *case_;
  explicit Impl(std::string const &evrp_path) : case_(cevrp_case_create(evrp_path)) {}
  ~Impl() { cevrp_case_destroy(case_); }
};

CacoDecode::CacoDecode(std::string const &evrp_path) : impl_(std::make_unique<Impl>(evrp_path)) {}

CacoDecode::~CacoDecode() = default;

auto CacoDecode::decode(std::vector<size_t> const &routes) -> std::vector<size_t> {
  auto t0 = std::chrono::steady_clock::now();
  auto result = caco_decode_raw(impl_->case_, routes);
  last_elapsed_ns_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - t0).count();
  return result;
}

}  // namespace evrp
