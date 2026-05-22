"""Tests for evrp_decoder.Solution — validation and cost methods."""

import math
from pathlib import Path

import pytest

from evrp_decoder._evrp_core import Instance, Solution
from evrp_decoder.loader import load_evrp

DATASET_DIR = Path(__file__).resolve().parent.parent / "dataset"
SMALL_INSTANCE = DATASET_DIR / "E-n22-k4.evrp"


@pytest.fixture
def inst() -> Instance:
    return load_evrp(SMALL_INSTANCE)


# ---------------------------------------------------------------------------
# cost()
# ---------------------------------------------------------------------------


class TestCost:
    def test_single_leg(self, inst: Instance):
        """depot → customer 12 → depot — cost equals two distances."""
        sol = Solution(inst, [0, 12, 0])
        expected = inst.distance(0, 12) + inst.distance(12, 0)
        assert sol.cost() == pytest.approx(expected)

    def test_multi_leg(self, inst: Instance):
        """depot → 14 → 12 → depot — sum of consecutive distances."""
        sol = Solution(inst, [0, 14, 12, 0])
        expected = (
            inst.distance(0, 14)
            + inst.distance(14, 12)
            + inst.distance(12, 0)
        )
        assert sol.cost() == pytest.approx(expected)

    def test_multi_route_cost(self, inst: Instance):
        """Two sub-routes, cost is total distance over both."""
        route = [0, 12, 0, 14, 0]
        sol = Solution(inst, route)
        expected = (
            inst.distance(0, 12)
            + inst.distance(12, 0)
            + inst.distance(0, 14)
            + inst.distance(14, 0)
        )
        assert sol.cost() == pytest.approx(expected)


# ---------------------------------------------------------------------------
# is_cargo_valid()
# ---------------------------------------------------------------------------


class TestIsCargoValid:
    def test_single_customer_within_capacity(self, inst: Instance):
        """Customer 14 has demand 300, well within capacity 6000."""
        sol = Solution(inst, [0, 14, 0])
        assert sol.is_cargo_valid()

    def test_multiple_customers_within_capacity(self, inst: Instance):
        """Customers 14 (300) + 8 (100) + 12 (1300) = 1700 < 6000."""
        sol = Solution(inst, [0, 14, 8, 12, 0])
        assert sol.is_cargo_valid()

    def test_cargo_exactly_at_capacity(self, inst: Instance):
        """Customers 5 (2100) + 1 (1100) + 2 (700) + 5th=... pick combo summing to 6000.
        Customers 5 (2100) + 16 (2100) + 1 (1100) + 2 (700) = 6000."""
        sol = Solution(inst, [0, 5, 16, 1, 2, 0])
        assert sol.is_cargo_valid()

    def test_cargo_exceeds_capacity(self, inst: Instance):
        """Customers 19 (2500) + 5 (2100) + 4 (1400) + 1 (1100) = 7100 > 6000."""
        sol = Solution(inst, [0, 19, 5, 4, 1, 0])
        assert not sol.is_cargo_valid()

    def test_cargo_resets_at_depot(self, inst: Instance):
        """Each sub-route individually valid even if combined would exceed.
        Route 1: 19 (2500) + 5 (2100) = 4600 < 6000
        Route 2: 4 (1400) + 1 (1100) = 2500 < 6000"""
        sol = Solution(inst, [0, 19, 5, 0, 4, 1, 0])
        assert sol.is_cargo_valid()

    def test_stations_dont_affect_cargo(self, inst: Instance):
        """Visiting a charging station shouldn't reset cargo."""
        # 19 (2500) + station + 5 (2100) + 4 (1400) = 6000 (no reset at station)
        sol = Solution(inst, [0, 19, 5, 22, 4, 0])
        assert sol.is_cargo_valid()  # 2500+2100+1400=6000, exactly at cap

        # Now push over: add customer 8 (100) -> 6100 > 6000
        sol2 = Solution(inst, [0, 19, 5, 22, 4, 8, 0])
        assert not sol2.is_cargo_valid()


# ---------------------------------------------------------------------------
# is_energy_and_cargo_valid()
# ---------------------------------------------------------------------------


class TestIsEnergyAndCargoValid:
    def test_short_route_valid(self, inst: Instance):
        """depot → 14 → depot: round-trip energy ≈ 17, battery = 94."""
        sol = Solution(inst, [0, 14, 0])
        assert sol.is_energy_and_cargo_valid()

    def test_energy_resets_at_station(self, inst: Instance):
        """A long chain with a charging station in the middle should be valid
        if each segment fits within battery."""
        sol = Solution(inst, [0, 14, 27, 12, 0])  # station 27 at (155,213)
        assert sol.is_energy_and_cargo_valid()

    def test_energy_resets_at_depot(self, inst: Instance):
        """Returning to depot resets energy."""
        sol = Solution(inst, [0, 14, 0, 12, 0])
        assert sol.is_energy_and_cargo_valid()

    def test_cargo_violation_detected(self, inst: Instance):
        """Energy is fine but cargo exceeds capacity."""
        # Short routes but too much demand in one leg
        # 19 (2500) + 5 (2100) + 4 (1400) + 1 (1100) = 7100 > 6000
        sol = Solution(inst, [0, 19, 22, 5, 22, 4, 22, 1, 0])
        assert not sol.is_energy_and_cargo_valid()


# ---------------------------------------------------------------------------
# is_valid()
# ---------------------------------------------------------------------------


class TestIsValid:
    def test_must_start_at_depot(self, inst: Instance):
        """Route not starting at depot is invalid."""
        sol = Solution(inst, [1, 2, 0])
        assert not sol.is_valid()

    def test_must_end_at_depot(self, inst: Instance):
        """Route not ending at depot is invalid."""
        sol = Solution(inst, [0, 1, 2])
        assert not sol.is_valid()

    def test_missing_customers_invalid(self, inst: Instance):
        """Not visiting all customers makes solution invalid."""
        sol = Solution(inst, [0, 1, 2, 3, 0])
        assert not sol.is_valid()

    def test_duplicate_customer_invalid(self, inst: Instance):
        """Visiting a customer twice is invalid (count != unique count)."""
        all_customers = list(range(1, inst.customer_cnt() + 1))
        # Duplicate customer 1
        route = [0] + all_customers + [1, 0]
        sol = Solution(inst, route)
        assert not sol.is_valid()

    def test_routes_property(self, inst: Instance):
        """The routes property should return the same route."""
        route = [0, 14, 12, 0]
        sol = Solution(inst, route)
        assert list(sol.routes) == route

    def test_routes_property_settable(self, inst: Instance):
        """The routes property should be settable."""
        sol = Solution(inst, [0, 14, 0])
        new_route = [0, 12, 0]
        sol.routes = new_route
        assert list(sol.routes) == new_route
