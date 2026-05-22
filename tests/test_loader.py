"""Tests for evrp_decoder.loader — .evrp file parsing and Instance construction."""

import math
from pathlib import Path

import pytest

from evrp_decoder._evrp_core import Instance, Node, NodeType
from evrp_decoder.loader import load_all, load_evrp

DATASET_DIR = Path(__file__).resolve().parent.parent / "dataset"
SMALL_INSTANCE = DATASET_DIR / "E-n22-k4.evrp"


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------


@pytest.fixture
def inst() -> Instance:
    """Load the small E-n22-k4 instance once per test."""
    return load_evrp(SMALL_INSTANCE)


# ---------------------------------------------------------------------------
# Header / metadata
# ---------------------------------------------------------------------------


class TestHeaderParsing:
    def test_name(self, inst: Instance):
        assert "Mavrovouniotis" in inst.name

    def test_cargo_capacity(self, inst: Instance):
        assert inst.cargo_capacity() == 6000.0

    def test_battery_capacity(self, inst: Instance):
        assert inst.battery_capacity() == 94.0

    def test_energy_consumption(self, inst: Instance):
        assert inst.energy_consumption() == pytest.approx(1.2)


# ---------------------------------------------------------------------------
# Node counts and classification
# ---------------------------------------------------------------------------


class TestNodeCounts:
    def test_total_node_count(self, inst: Instance):
        """DIMENSION (22) + STATIONS (8) = 30 total nodes."""
        assert inst.node_cnt() == 30

    def test_customer_count(self, inst: Instance):
        """21 customers (DIMENSION - 1 depot)."""
        assert inst.customer_cnt() == 21

    def test_charging_station_count(self, inst: Instance):
        assert inst.charging_station_cnt() == 8


# ---------------------------------------------------------------------------
# Node ordering: depot (0), customers (1..21), stations (22..29)
# ---------------------------------------------------------------------------


class TestNodeOrdering:
    def test_depot_is_first(self, inst: Instance):
        assert inst.node(0).type == NodeType.Depot

    def test_customers_are_middle(self, inst: Instance):
        for i in range(1, inst.customer_cnt() + 1):
            assert inst.node(i).type == NodeType.Customer
            assert inst.is_customer(i)
            assert not inst.is_charging_station(i)

    def test_stations_are_last(self, inst: Instance):
        start = inst.customer_cnt() + 1
        for i in range(start, inst.node_cnt()):
            assert inst.node(i).type == NodeType.ChargingStation
            assert inst.is_charging_station(i)
            assert not inst.is_customer(i)


# ---------------------------------------------------------------------------
# Depot coordinates and demand
# ---------------------------------------------------------------------------


class TestDepot:
    def test_depot_coordinates(self, inst: Instance):
        depot = inst.node(0)
        assert depot.x == 145.0
        assert depot.y == 215.0

    def test_depot_demand_is_zero(self, inst: Instance):
        assert inst.demand(0) == 0.0


# ---------------------------------------------------------------------------
# Customer demands
# ---------------------------------------------------------------------------


class TestDemands:
    def test_first_customer_demand(self, inst: Instance):
        """Node 2 in the file (first customer) has demand 1100."""
        assert inst.demand(1) == 1100.0

    def test_all_customers_have_positive_demand(self, inst: Instance):
        for i in range(1, inst.customer_cnt() + 1):
            assert inst.demand(i) > 0

    def test_stations_have_zero_demand(self, inst: Instance):
        start = inst.customer_cnt() + 1
        for i in range(start, inst.node_cnt()):
            assert inst.demand(i) == 0.0


# ---------------------------------------------------------------------------
# Distance / energy calculations
# ---------------------------------------------------------------------------


class TestDistances:
    def test_self_distance_is_zero(self, inst: Instance):
        assert inst.distance(0, 0) == 0.0
        assert inst.distance(5, 5) == 0.0

    def test_distance_symmetry(self, inst: Instance):
        for i in range(min(5, inst.node_cnt())):
            for j in range(i + 1, min(5, inst.node_cnt())):
                assert inst.distance(i, j) == pytest.approx(inst.distance(j, i))

    def test_depot_to_first_customer(self, inst: Instance):
        """Euclidean distance from (145,215) to (151,264) = sqrt(36+2401) ≈ 49.366."""
        expected = math.sqrt((151 - 145) ** 2 + (264 - 215) ** 2)
        assert inst.distance(0, 1) == pytest.approx(expected)

    def test_energy_required(self, inst: Instance):
        d = inst.distance(0, 1)
        assert inst.energy_required(0, 1) == pytest.approx(d * inst.energy_consumption())


# ---------------------------------------------------------------------------
# load_all
# ---------------------------------------------------------------------------


class TestLoadAll:
    def test_loads_all_files(self):
        instances = load_all(DATASET_DIR)
        assert len(instances) == 17

    def test_keys_are_stems(self):
        instances = load_all(DATASET_DIR)
        assert "E-n22-k4" in instances
        assert "X-n1001-k43" in instances

    def test_all_values_are_instances(self):
        instances = load_all(DATASET_DIR)
        for v in instances.values():
            assert isinstance(v, Instance)

    def test_all_instances_have_nodes(self):
        instances = load_all(DATASET_DIR)
        for v in instances.values():
            assert v.node_cnt() > 0
            assert v.customer_cnt() > 0
