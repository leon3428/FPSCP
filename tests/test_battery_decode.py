"""Tests for evrp_decoder.BatteryDecode — battery-feasible EVRP decoding with charging.

BatteryDecode only fixes battery constraints, so linear_split must be called
first to fix cargo constraints.
"""

import random
from pathlib import Path

import pytest

from evrp_decoder._evrp_core import BatteryDecode, Instance, Solution, linear_split
from evrp_decoder.loader import load_all, load_evrp

DATASET_DIR = Path(__file__).resolve().parent.parent / "dataset"
SMALL_INSTANCE = DATASET_DIR / "E-n22-k4.evrp"

SHUFFLE_ITERATIONS = 10
UPPER_BOUND = 1e9  # large bound so all solutions are accepted


@pytest.fixture
def inst() -> Instance:
    return load_evrp(SMALL_INSTANCE)


@pytest.fixture
def decoder(inst: Instance) -> BatteryDecode:
    return BatteryDecode(inst)


@pytest.fixture
def all_instances() -> dict[str, Instance]:
    return load_all(DATASET_DIR)


# ---------------------------------------------------------------------------
# Core property: BatteryDecode always produces valid solutions
# ---------------------------------------------------------------------------


class TestValidity:
    def test_sequential_customers_valid(self, inst: Instance, decoder: BatteryDecode):
        """Sequential customer order should produce a valid solution."""
        customers = list(range(1, inst.customer_cnt() + 1))
        sol = Solution(inst, customers)
        split = linear_split(sol)
        result = decoder.decode(split, UPPER_BOUND, False)

        assert result.is_valid()

    def test_shuffled_customers_valid(self, inst: Instance, decoder: BatteryDecode):
        """10 random permutations should all produce valid solutions."""
        rng = random.Random(42)
        customers = list(range(1, inst.customer_cnt() + 1))

        for _ in range(SHUFFLE_ITERATIONS):
            rng.shuffle(customers)
            sol = Solution(inst, list(customers))
            split = linear_split(sol)
            result = decoder.decode(split, UPPER_BOUND, False)

            assert result.is_valid()

    def test_all_instances_valid(self, all_instances: dict[str, Instance]):
        """For every dataset instance, shuffled permutations produce valid solutions."""
        rng = random.Random(42)

        for name, inst in all_instances.items():
            print(name)
            decoder = BatteryDecode(inst)
            customers = list(range(1, inst.customer_cnt() + 1))

            for _ in range(SHUFFLE_ITERATIONS):
                rng.shuffle(customers)
                sol = Solution(inst, list(customers))
                split = linear_split(sol)
                result = decoder.decode(split, UPPER_BOUND, False)
                
                assert result.is_valid(), (
                    f"Validity violation on instance {name}"
                )


# ---------------------------------------------------------------------------
# Structural properties of the decoded result
# ---------------------------------------------------------------------------


class TestStructure:
    def test_starts_at_depot(self, inst: Instance, decoder: BatteryDecode):
        """Decoded result should start with the depot."""
        customers = list(range(1, inst.customer_cnt() + 1))
        sol = Solution(inst, customers)
        split = linear_split(sol)
        result = decoder.decode(split, UPPER_BOUND, False)
        assert result.routes[0] == 0

    def test_ends_at_depot(self, inst: Instance, decoder: BatteryDecode):
        """Decoded result should end with the depot."""
        customers = list(range(1, inst.customer_cnt() + 1))
        sol = Solution(inst, customers)
        split = linear_split(sol)
        result = decoder.decode(split, UPPER_BOUND, False)
        assert result.routes[-1] == 0

    def test_preserves_all_customers(self, inst: Instance, decoder: BatteryDecode):
        """All customers from the input should appear in the output."""
        customers = list(range(1, inst.customer_cnt() + 1))
        sol = Solution(inst, customers)
        split = linear_split(sol)
        result = decoder.decode(split, UPPER_BOUND, False)

        output_customers = [
            nid for nid in result.routes if inst.is_customer(nid)
        ]
        assert sorted(output_customers) == sorted(customers)

    def test_preserves_customer_order(self, inst: Instance, decoder: BatteryDecode):
        """Customer order from the input should be preserved in the output."""
        rng = random.Random(123)
        customers = list(range(1, inst.customer_cnt() + 1))
        rng.shuffle(customers)

        sol = Solution(inst, list(customers))
        split = linear_split(sol)
        result = decoder.decode(split, UPPER_BOUND, False)

        output_customers = [
            nid for nid in result.routes if inst.is_customer(nid)
        ]
        assert output_customers == customers


# ---------------------------------------------------------------------------
# Single CS variants
# ---------------------------------------------------------------------------


class TestSingleCsValidity:
    def test_sequential_customers_valid(self, inst: Instance, decoder: BatteryDecode):
        """Sequential customer order with single_cs should produce a valid solution."""
        customers = list(range(1, inst.customer_cnt() + 1))
        sol = Solution(inst, customers)
        split = linear_split(sol)
        result = decoder.decode(split, UPPER_BOUND, True)

        assert result.is_valid()

    def test_shuffled_customers_valid(self, inst: Instance, decoder: BatteryDecode):
        """10 random permutations with single_cs should all produce valid solutions."""
        rng = random.Random(42)
        customers = list(range(1, inst.customer_cnt() + 1))

        for _ in range(SHUFFLE_ITERATIONS):
            rng.shuffle(customers)
            sol = Solution(inst, list(customers))
            split = linear_split(sol)
            result = decoder.decode(split, UPPER_BOUND, True)

            assert result.is_valid()


class TestSingleCsStructure:
    def test_starts_at_depot(self, inst: Instance, decoder: BatteryDecode):
        """Single_cs decoded result should start with the depot."""
        customers = list(range(1, inst.customer_cnt() + 1))
        sol = Solution(inst, customers)
        split = linear_split(sol)
        result = decoder.decode(split, UPPER_BOUND, True)
        assert result.routes[0] == 0

    def test_ends_at_depot(self, inst: Instance, decoder: BatteryDecode):
        """Single_cs decoded result should end with the depot."""
        customers = list(range(1, inst.customer_cnt() + 1))
        sol = Solution(inst, customers)
        split = linear_split(sol)
        result = decoder.decode(split, UPPER_BOUND, True)
        assert result.routes[-1] == 0

    def test_preserves_all_customers(self, inst: Instance, decoder: BatteryDecode):
        """All customers should appear in single_cs output."""
        customers = list(range(1, inst.customer_cnt() + 1))
        sol = Solution(inst, customers)
        split = linear_split(sol)
        result = decoder.decode(split, UPPER_BOUND, True)

        output_customers = [
            nid for nid in result.routes if inst.is_customer(nid)
        ]
        assert sorted(output_customers) == sorted(customers)

    def test_preserves_customer_order(self, inst: Instance, decoder: BatteryDecode):
        """Customer order should be preserved in single_cs output."""
        rng = random.Random(123)
        customers = list(range(1, inst.customer_cnt() + 1))
        rng.shuffle(customers)

        sol = Solution(inst, list(customers))
        split = linear_split(sol)
        result = decoder.decode(split, UPPER_BOUND, True)

        output_customers = [
            nid for nid in result.routes if inst.is_customer(nid)
        ]
        assert output_customers == customers
