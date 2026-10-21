"""Tests for evrp_decoder.MontoyaDecode — split-then-repair decoding, checked against OptimalDecode.

MontoyaDecode is exact for this problem (no route duration limit), so for every permutation it must
return a valid solution with the same cost as OptimalDecode.
"""

import random
from pathlib import Path

import pytest

from evrp_decoder._evrp_core import Instance, MontoyaDecode, OptimalDecode, Solution
from evrp_decoder.generators import generate_instance, knn_permutation
from evrp_decoder.loader import load_all, load_evrp

DATASET_DIR = Path(__file__).resolve().parent.parent / "dataset"
SMALL_INSTANCE = DATASET_DIR / "E-n22-k4.evrp"

SHUFFLE_ITERATIONS = 10
REL_TOL = 1e-9


@pytest.fixture
def inst() -> Instance:
    return load_evrp(SMALL_INSTANCE)


@pytest.fixture(scope="module")
def all_instances() -> dict[str, Instance]:
    return load_all(DATASET_DIR)


def customers_in_order(inst: Instance, sol: Solution) -> list[int]:
    return [nid for nid in sol.routes if inst.is_customer(nid)]


def assert_matches_optimal(inst: Instance, perm: list[int], label: str = ""):
    expected = OptimalDecode(inst).decode(Solution(inst, list(perm)))
    actual = MontoyaDecode(inst).decode(Solution(inst, list(perm)))

    if not expected.is_valid():
        # OptimalDecode found no feasible decoding and returned the input; MontoyaDecode must too.
        assert list(actual.routes) == list(perm), f"{label}: MontoyaDecode decoded an infeasible permutation"
        return

    assert actual.is_valid(), f"{label}: MontoyaDecode returned an invalid solution"
    assert customers_in_order(inst, actual) == list(perm), f"{label}: customer order not preserved"
    assert actual.cost() == pytest.approx(expected.cost(), rel=REL_TOL), (
        f"{label}: montoya={actual.cost():.9f} optimal={expected.cost():.9f}"
    )


# ---------------------------------------------------------------------------
# Cost equality with OptimalDecode
# ---------------------------------------------------------------------------


class TestMatchesOptimal:
    def test_sequential_customers(self, inst: Instance):
        assert_matches_optimal(inst, list(range(1, inst.customer_cnt() + 1)))

    def test_all_instances_shuffled(self, all_instances: dict[str, Instance]):
        rng = random.Random(42)
        for name, inst in all_instances.items():
            customers = list(range(1, inst.customer_cnt() + 1))
            for it in range(SHUFFLE_ITERATIONS):
                rng.shuffle(customers)
                assert_matches_optimal(inst, customers, label=f"{name}[shuffle {it}]")

    def test_all_instances_knn(self, all_instances: dict[str, Instance]):
        """KNN permutations give long, tight routes where charging-station placement matters more."""
        rng = random.Random(7)
        for name, inst in all_instances.items():
            for it in range(SHUFFLE_ITERATIONS):
                perm = knn_permutation(inst, 3, rng)
                assert_matches_optimal(inst, perm, label=f"{name}[knn {it}]")

    @pytest.mark.parametrize("battery_capacity", [0.6, 0.8, 1.0, 1.5, 2.5])
    @pytest.mark.parametrize("charging_station_cnt", [1, 3, 8])
    def test_generated_instances(self, battery_capacity: float, charging_station_cnt: int):
        """Random instances across battery sizes, from barely feasible (long station chains) to uncharged."""
        rng = random.Random(1000 * charging_station_cnt + int(10 * battery_capacity))
        for inst_it in range(5):
            inst = generate_instance(
                customer_cnt=30,
                charging_station_cnt=charging_station_cnt,
                battery_capacity=battery_capacity,
                cargo_capacity=300.0,
                rng=rng,
            )
            customers = list(range(1, inst.customer_cnt() + 1))
            for it in range(SHUFFLE_ITERATIONS):
                rng.shuffle(customers)
                assert_matches_optimal(
                    inst, customers, label=f"B={battery_capacity} cs={charging_station_cnt} inst={inst_it} it={it}"
                )


# ---------------------------------------------------------------------------
# Bookkeeping exposed through the bindings
# ---------------------------------------------------------------------------


class TestStats:
    def test_counters(self, inst: Instance):
        decoder = MontoyaDecode(inst)
        decoder.decode(Solution(inst, list(range(1, inst.customer_cnt() + 1))))
        assert decoder.last_routes_evaluated() > 0
        assert 0 <= decoder.last_routes_repaired() <= decoder.last_routes_evaluated()
        assert decoder.last_elapsed_ns() > 0
