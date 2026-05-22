#pragma once

#include <cstdint>

namespace evrp {

// DO NOT CHANGE THE ORDER!!!!!!!!!!!!
// The order is important in the instance class
enum class NodeType : uint8_t { Depot = 0, Customer = 1, ChargingStation = 2 };

struct Node {
  Node() = default;

  NodeType type;
  double x;
  double y;
  double demand;
};

} // namespace evrp