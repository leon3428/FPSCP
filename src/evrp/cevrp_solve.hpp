#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct CevrpCase;

CevrpCase *cevrp_case_create(std::string const &evrp_path);
void cevrp_case_destroy(CevrpCase *c);

auto caco_decode_raw(CevrpCase *c, std::vector<size_t> const &routes) -> std::vector<size_t>;
auto baco2_decode_raw(CevrpCase *c, std::vector<size_t> const &routes) -> std::vector<size_t>;
