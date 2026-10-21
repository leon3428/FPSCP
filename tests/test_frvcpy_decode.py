"""Tests for evrp_decoder.FrvcpyDecode — frvcpy-based charging station insertion.

FrvcpyDecode is a two-phase decoder: linear_split must be called first, then
decode() is passed the flat split-route list.  Since frvcpy is an exact solver,
its costs should be <= BatteryDecode costs on the same fixed routes.
"""

import random
from pathlib import Path

import pytest

from evrp_decoder._evrp_core import BatteryDecode, Instance, Solution, linear_split
from evrp_decoder.frvcpy_decode import FrvcpyDecode
from evrp_decoder.loader import load_all, load_evrp

DATASET_DIR = Path(__file__).resolve().parent.parent / "dataset"
SMALL_INSTANCE = DATASET_DIR / "E-n22-k4.evrp"

SHUFFLE_ITERATIONS = 1000


@pytest.fixture
def inst() -> Instance:
    return load_evrp(SMALL_INSTANCE)


@pytest.fixture
def decoder(inst: Instance) -> FrvcpyDecode:
    return FrvcpyDecode(inst)


@pytest.fixture
def all_instances() -> dict[str, Instance]:
    return load_all(DATASET_DIR)


# ---------------------------------------------------------------------------
# Validity
# ---------------------------------------------------------------------------


class TestValidity:
    def test_sequential_customers_valid(self, inst: Instance, decoder: FrvcpyDecode):
        """Sequential customer order should produce a valid solution."""
        customers = list(range(1, inst.customer_cnt() + 1))
        split = linear_split(Solution(inst, customers))
        result = Solution(inst, decoder.decode(split.routes))
        assert result.is_valid()

    def test_shuffled_customers_valid(self, inst: Instance, decoder: FrvcpyDecode):
        """Random permutations should all produce valid solutions."""
        rng = random.Random(42)
        customers = list(range(1, inst.customer_cnt() + 1))

        for _ in range(SHUFFLE_ITERATIONS):
            rng.shuffle(customers)
            split = linear_split(Solution(inst, list(customers)))
            result = Solution(inst, decoder.decode(split.routes))
            assert result.is_valid()


# ---------------------------------------------------------------------------
# Structural properties
# ---------------------------------------------------------------------------


class TestStructure:
    def test_starts_at_depot(self, inst: Instance, decoder: FrvcpyDecode):
        customers = list(range(1, inst.customer_cnt() + 1))
        split = linear_split(Solution(inst, customers))
        out = decoder.decode(split.routes)
        assert out[0] == 0

    def test_ends_at_depot(self, inst: Instance, decoder: FrvcpyDecode):
        customers = list(range(1, inst.customer_cnt() + 1))
        split = linear_split(Solution(inst, customers))
        out = decoder.decode(split.routes)
        assert out[-1] == 0

    def test_no_consecutive_depots(self, inst: Instance, decoder: FrvcpyDecode):
        """There should be no empty routes (consecutive depot markers)."""
        rng = random.Random(7)
        customers = list(range(1, inst.customer_cnt() + 1))

        for _ in range(SHUFFLE_ITERATIONS):
            rng.shuffle(customers)
            split = linear_split(Solution(inst, list(customers)))
            out = decoder.decode(split.routes)

            for a, b in zip(out, out[1:]):
                assert not (a == 0 and b == 0), (
                    f"Consecutive depots found in output: {out}"
                )

    def test_preserves_all_customers(self, inst: Instance, decoder: FrvcpyDecode):
        customers = list(range(1, inst.customer_cnt() + 1))
        split = linear_split(Solution(inst, customers))
        out = decoder.decode(split.routes)

        out_customers = [nid for nid in out if inst.is_customer(nid)]
        assert sorted(out_customers) == sorted(customers)

    def test_preserves_customer_order(self, inst: Instance, decoder: FrvcpyDecode):
        rng = random.Random(123)
        customers = list(range(1, inst.customer_cnt() + 1))
        rng.shuffle(customers)

        split = linear_split(Solution(inst, list(customers)))
        out = decoder.decode(split.routes)

        out_customers = [nid for nid in out if inst.is_customer(nid)]
        assert out_customers == customers

    def test_timing(self, inst: Instance, decoder: FrvcpyDecode):
        customers = list(range(1, inst.customer_cnt() + 1))
        split = linear_split(Solution(inst, customers))
        decoder.decode(split.routes)
        assert decoder.last_elapsed_ns() > 0


# ---------------------------------------------------------------------------
# Cost comparison with BatteryDecode (both are optimal for a fixed route)
# ---------------------------------------------------------------------------


class TestOptimality:
    def test_cost_equals_battery_decode_small(self, inst: Instance, decoder: FrvcpyDecode):
        """On the small instance, frvcpy cost must equal BatteryDecode cost.

        Both are optimal exact solvers for a fixed route, so they must agree.
        Prints the first diverging case to help debug if the assertion fails.
        """
        battery_dec = BatteryDecode(inst)
        rng = random.Random(42)
        customers = list(range(1, inst.customer_cnt() + 1))

        for i in range(SHUFFLE_ITERATIONS):
            rng.shuffle(customers)
            split = linear_split(Solution(inst, list(customers)))
            routes = split.routes

            bat = battery_dec.decode(split, False)
            frv = Solution(inst, decoder.decode(routes))

            diff = frv.cost() - bat.cost()
            assert abs(diff) < 1e-6, (
                f"Iteration {i}: frvcpy cost {frv.cost():.4f} != "
                f"battery cost {bat.cost():.4f} (diff={diff:.6f})\n"
                f"  routes in : {routes}\n"
                f"  battery out: {bat.routes}\n"
                f"  frvcpy  out: {frv.routes}\n"
                f"  frvcpy valid: {frv.is_valid()}"
                f"  battery valid: {bat.is_valid()}"
            )
