"""Tests for evrp_decoder.BhgaDecode — battery-feasible EVRP decoding with charging.

BhgaDecode only fixes battery constraints, so linear_split must be called
first to fix cargo constraints.

Unlike BatteryDecode (which uses Floyd-Warshall multi-hop paths), BhgaDecode
wraps focusEnumeration which inserts at most one charging station per
customer-to-customer gap. When a gap requires multi-hop coverage, focusEnumeration
cannot fix it and falls back to the unfixed sub-route. The tests reflect this:
structural properties and cargo validity are always guaranteed; full energy+cargo
validity is only guaranteed for simple / well-connected cases.

BhgaDecode takes an .evrp file path and its decode() accepts/returns flat route
lists (not Solution objects).
"""

import random
from pathlib import Path

import pytest

from evrp_decoder._evrp_core import BhgaDecode, Instance, Solution, linear_split
from evrp_decoder.loader import load_evrp

DATASET_DIR = Path(__file__).resolve().parent.parent / "dataset"
SMALL_INSTANCE = DATASET_DIR / "E-n22-k4.evrp"

SHUFFLE_ITERATIONS = 10


@pytest.fixture
def inst() -> Instance:
    return load_evrp(SMALL_INSTANCE)


@pytest.fixture
def decoder() -> BhgaDecode:
    return BhgaDecode(str(SMALL_INSTANCE))


@pytest.fixture
def all_paths() -> list[Path]:
    return sorted(DATASET_DIR.glob("*.evrp"))


# ---------------------------------------------------------------------------
# Core property: BhgaDecode always produces valid solutions
# ---------------------------------------------------------------------------


class TestValidity:
    def test_sequential_customers_valid(self, inst: Instance, decoder: BhgaDecode):
        """Sequential customer order should produce a valid solution."""
        customers = list(range(1, inst.customer_cnt() + 1))
        sol = Solution(inst, customers)
        split = linear_split(sol)
        result = Solution(inst, decoder.decode(split.routes))

        assert result.is_valid()

    def test_shuffled_customers_cargo_valid(self, inst: Instance, decoder: BhgaDecode):
        """10 random permutations should all produce cargo-valid solutions.

        focusEnumeration may not achieve battery feasibility when a gap requires
        multi-hop charging, so only cargo validity (guaranteed by linear_split) is
        asserted here.
        """
        rng = random.Random(42)
        customers = list(range(1, inst.customer_cnt() + 1))

        for _ in range(SHUFFLE_ITERATIONS):
            rng.shuffle(customers)
            sol = Solution(inst, list(customers))
            split = linear_split(sol)
            result = Solution(inst, decoder.decode(split.routes))

            assert result.is_cargo_valid()

    def test_all_instances_cargo_valid(self, all_paths: list[Path]):
        """For every dataset instance, shuffled permutations produce cargo-valid solutions.

        focusEnumeration may not achieve battery feasibility when a gap requires
        multi-hop charging, so only cargo validity (guaranteed by linear_split) is
        asserted here.
        """
        rng = random.Random(42)

        for path in all_paths:
            print(path.stem)
            inst = load_evrp(path)
            decoder = BhgaDecode(str(path))
            customers = list(range(1, inst.customer_cnt() + 1))

            for _ in range(SHUFFLE_ITERATIONS):
                rng.shuffle(customers)
                sol = Solution(inst, list(customers))
                split = linear_split(sol)
                result = Solution(inst, decoder.decode(split.routes))

                assert result.is_cargo_valid(), (
                    f"Cargo validity violation on instance {path.stem}"
                )


# ---------------------------------------------------------------------------
# Structural properties of the decoded result
# ---------------------------------------------------------------------------


class TestStructure:
    def test_starts_at_depot(self, inst: Instance, decoder: BhgaDecode):
        """Decoded result should start with the depot."""
        customers = list(range(1, inst.customer_cnt() + 1))
        sol = Solution(inst, customers)
        split = linear_split(sol)
        result = Solution(inst, decoder.decode(split.routes))
        assert result.routes[0] == 0

    def test_ends_at_depot(self, inst: Instance, decoder: BhgaDecode):
        """Decoded result should end with the depot."""
        customers = list(range(1, inst.customer_cnt() + 1))
        sol = Solution(inst, customers)
        split = linear_split(sol)
        result = Solution(inst, decoder.decode(split.routes))
        assert result.routes[-1] == 0

    def test_preserves_all_customers(self, inst: Instance, decoder: BhgaDecode):
        """All customers from the input should appear in the output."""
        customers = list(range(1, inst.customer_cnt() + 1))
        sol = Solution(inst, customers)
        split = linear_split(sol)
        result = Solution(inst, decoder.decode(split.routes))

        output_customers = [
            nid for nid in result.routes if inst.is_customer(nid)
        ]
        assert sorted(output_customers) == sorted(customers)

    def test_preserves_customer_order(self, inst: Instance, decoder: BhgaDecode):
        """Customer order from the input should be preserved in the output."""
        rng = random.Random(123)
        customers = list(range(1, inst.customer_cnt() + 1))
        rng.shuffle(customers)

        sol = Solution(inst, list(customers))
        split = linear_split(sol)
        result = Solution(inst, decoder.decode(split.routes))

        output_customers = [
            nid for nid in result.routes if inst.is_customer(nid)
        ]
        assert output_customers == customers
