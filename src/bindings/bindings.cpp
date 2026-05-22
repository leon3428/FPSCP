#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "evrp/baco_decode.hpp"
#include "evrp/bhga_decode.hpp"
#include "evrp/caco_decode.hpp"
#include "evrp/instance.hpp"
#include "evrp/node.hpp"
#include "evrp/optimal_decode.hpp"
#include "evrp/solution.hpp"
#include "evrp/split_decode.hpp"

namespace py = pybind11;

PYBIND11_MODULE(_evrp_core, m) {
  m.doc() = "EVRP optimal decoder C++ core";

  py::enum_<evrp::NodeType>(m, "NodeType")
      .value("Depot", evrp::NodeType::Depot)
      .value("Customer", evrp::NodeType::Customer)
      .value("ChargingStation", evrp::NodeType::ChargingStation)
      .export_values();

  py::class_<evrp::Node>(m, "Node")
      .def(py::init<>())
      .def_readwrite("type", &evrp::Node::type)
      .def_readwrite("x", &evrp::Node::x)
      .def_readwrite("y", &evrp::Node::y)
      .def_readwrite("demand", &evrp::Node::demand);

  py::class_<evrp::Instance, std::shared_ptr<evrp::Instance>>(m, "Instance")
      .def(py::init<std::string, double, size_t, double, double, double, size_t, size_t, std::vector<evrp::Node>>(),
           py::arg("name"), py::arg("optimal_value"), py::arg("minimum_route_cnt"), py::arg("cargo_capacity"),
           py::arg("battery_capacity"), py::arg("energy_consumption"), py::arg("customer_cnt"),
           py::arg("charging_station_cnt"), py::arg("nodes"))
      .def_property_readonly("name", &evrp::Instance::name)
      .def("node_cnt", &evrp::Instance::node_cnt)
      .def("customer_cnt", &evrp::Instance::customer_cnt)
      .def("charging_station_cnt", &evrp::Instance::charging_station_cnt)
      .def("cargo_capacity", &evrp::Instance::cargo_capacity)
      .def("battery_capacity", &evrp::Instance::battery_capacity)
      .def("energy_consumption", &evrp::Instance::energy_consumption)
      .def("distance", &evrp::Instance::distance, py::arg("node1_id"), py::arg("node2_id"))
      .def("energy_required", &evrp::Instance::energy_required, py::arg("node1_id"), py::arg("node2_id"))
      .def("demand", &evrp::Instance::demand, py::arg("ind"))
      .def("is_customer", &evrp::Instance::is_customer, py::arg("ind"))
      .def("is_charging_station", &evrp::Instance::is_charging_station, py::arg("ind"))
      .def("node", &evrp::Instance::node, py::arg("ind"), py::return_value_policy::reference_internal);

  py::class_<evrp::Solution>(m, "Solution")
      .def(py::init<std::shared_ptr<evrp::Instance>>(), py::arg("instance"))
      .def(py::init<std::shared_ptr<evrp::Instance>, std::vector<size_t> const &>(), py::arg("instance"),
           py::arg("routes"))
      .def("is_cargo_valid", &evrp::Solution::is_cargo_valid)
      .def("is_energy_and_cargo_valid", &evrp::Solution::is_energy_and_cargo_valid)
      .def("is_valid", &evrp::Solution::is_valid)
      .def("cost", &evrp::Solution::cost)
      .def_property(
          "routes", [](evrp::Solution const &s) -> std::vector<size_t> const & { return s.routes(); },
          [](evrp::Solution &s, std::vector<size_t> const &v) { s.routes() = v; });

  m.def("linear_split", &evrp::linear_split, py::arg("solution"), py::call_guard<py::gil_scoped_release>());

  py::class_<evrp::OptimalDecode>(m, "OptimalDecode")
      .def(py::init<std::shared_ptr<evrp::Instance>>(), py::arg("instance"))
      .def("decode", &evrp::OptimalDecode::decode, py::arg("solution"), py::arg("upper_bound"),
           py::call_guard<py::gil_scoped_release>())
      .def("front_size", &evrp::OptimalDecode::front_size, py::arg("i"))
      .def("front_cnt", &evrp::OptimalDecode::front_cnt)
      .def("last_elapsed_ns", &evrp::OptimalDecode::last_elapsed_ns);

  py::class_<evrp::BatteryDecode>(m, "BatteryDecode")
      .def(py::init<std::shared_ptr<evrp::Instance>>(), py::arg("instance"))
      .def("decode", &evrp::BatteryDecode::decode, py::arg("solution"), py::arg("upper_bound"), py::arg("single_cs"),
           py::call_guard<py::gil_scoped_release>())
      .def("last_elapsed_ns", &evrp::BatteryDecode::last_elapsed_ns);

  py::class_<evrp::BhgaDecode>(m, "BhgaDecode")
      .def(py::init<std::string const &>(), py::arg("evrp_path"))
      .def("decode", &evrp::BhgaDecode::decode, py::arg("routes"),
           py::call_guard<py::gil_scoped_release>())
      .def("last_elapsed_ns", &evrp::BhgaDecode::last_elapsed_ns);

  py::class_<evrp::BacoDecode>(m, "BacoDecode")
      .def(py::init<std::string const &>(), py::arg("evrp_path"))
      .def("decode", &evrp::BacoDecode::decode, py::arg("routes"),
           py::call_guard<py::gil_scoped_release>())
      .def("last_elapsed_ns", &evrp::BacoDecode::last_elapsed_ns);

  py::class_<evrp::CacoDecode>(m, "CacoDecode")
      .def(py::init<std::string const &>(), py::arg("evrp_path"))
      .def("decode", &evrp::CacoDecode::decode, py::arg("routes"),
           py::call_guard<py::gil_scoped_release>())
      .def("last_elapsed_ns", &evrp::CacoDecode::last_elapsed_ns);
}
