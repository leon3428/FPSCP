"""Tests for evrp_decoder.linear_split — optimal cargo-feasible route splitting."""

import random
from pathlib import Path

import pytest

from evrp_decoder._evrp_core import Instance, Solution, linear_split
from evrp_decoder.loader import load_all, load_evrp

DATASET_DIR = Path(__file__).resolve().parent.parent / "dataset"
SMALL_INSTANCE = DATASET_DIR / "E-n22-k4.evrp"

SHUFFLE_ITERATIONS = 100


@pytest.fixture
def inst() -> Instance:
    return load_evrp(SMALL_INSTANCE)


@pytest.fixture
def all_instances() -> dict[str, Instance]:
    return load_all(DATASET_DIR)


# ---------------------------------------------------------------------------
# Core property: linear_split always produces cargo-valid solutions
# ---------------------------------------------------------------------------


class TestCargoFeasibility:
    def test_sequential_customers_cargo_valid(self, inst: Instance):
        """Sequential customer order should produce a cargo-valid split."""
        customers = list(range(1, inst.customer_cnt() + 1))
        sol = Solution(inst, customers)
        result = linear_split(sol)

        assert result.is_cargo_valid()

    def test_shuffled_customers_cargo_valid(self, inst: Instance):
        """100 random permutations should all produce cargo-valid splits."""
        rng = random.Random(42)
        customers = list(range(1, inst.customer_cnt() + 1))

        for _ in range(SHUFFLE_ITERATIONS):
            rng.shuffle(customers)
            sol = Solution(inst, list(customers))
            result = linear_split(sol)
            assert result.is_cargo_valid()

    def test_all_instances_cargo_valid(self, all_instances: dict[str, Instance]):
        """For every dataset instance, shuffled permutations produce cargo-valid splits."""
        rng = random.Random(42)

        for name, inst in all_instances.items():
            customers = list(range(1, inst.customer_cnt() + 1))

            for _ in range(SHUFFLE_ITERATIONS):
                rng.shuffle(customers)
                sol = Solution(inst, list(customers))
                result = linear_split(sol)
                assert result.is_cargo_valid(), (
                    f"Cargo violation on instance {name}"
                )


# ---------------------------------------------------------------------------
# Structural properties of the split result
# ---------------------------------------------------------------------------


class TestStructure:
    def test_starts_at_depot(self, inst: Instance):
        """Split result should start with the depot."""
        customers = list(range(1, inst.customer_cnt() + 1))
        sol = Solution(inst, customers)
        result = linear_split(sol)
        assert result.routes[0] == 0

    def test_ends_at_depot(self, inst: Instance):
        """Split result should end with the depot."""
        customers = list(range(1, inst.customer_cnt() + 1))
        sol = Solution(inst, customers)
        result = linear_split(sol)
        assert result.routes[-1] == 0

    def test_preserves_all_customers(self, inst: Instance):
        """All customers from the input should appear in the output."""
        customers = list(range(1, inst.customer_cnt() + 1))
        sol = Solution(inst, customers)
        result = linear_split(sol)

        output_customers = [
            nid for nid in result.routes if inst.is_customer(nid)
        ]
        assert sorted(output_customers) == sorted(customers)

    def test_preserves_customer_order(self, inst: Instance):
        """Customer order from the input should be preserved in the output."""
        rng = random.Random(123)
        customers = list(range(1, inst.customer_cnt() + 1))
        rng.shuffle(customers)

        sol = Solution(inst, list(customers))
        result = linear_split(sol)

        output_customers = [
            nid for nid in result.routes if inst.is_customer(nid)
        ]
        assert output_customers == customers


