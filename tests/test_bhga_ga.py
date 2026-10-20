"""Tests for evrp_decoder.run_bhga — the full BHGA metaheuristic with a
swappable charging decoder (vanilla focusEnumeration or SS-FR-FLA).
"""

import math
import random
from pathlib import Path

import pytest

from evrp_decoder._evrp_core import (
    BatteryDecode,
    BhgaDecode,
    BhgaRunConfig,
    Instance,
    Node,
    NodeType,
    Solution,
    linear_split,
    run_bhga,
)
from evrp_decoder.loader import load_evrp

ROOT = Path(__file__).resolve().parent.parent
DATASET_DIR = ROOT / "dataset"
BHGA_DIR = ROOT / "extern" / "bhga" / "Instances" / "CVRP"
SMALL_INSTANCE = DATASET_DIR / "E-n22-k4.evrp"
DECODERS = ["vanilla", "ss_fr_fla"]
NB_ITER = 200
SHUFFLE_ITERATIONS = 10


@pytest.fixture(scope="module")
def inst() -> Instance:
    return load_evrp(SMALL_INSTANCE)


def make_config(decoder: str, seed: int = 1, name: str = "E-n22-k4", mix: str = "12116") -> BhgaRunConfig:
    config = BhgaRunConfig()
    config.vrp_path = str(BHGA_DIR / f"{name}.vrp")
    config.evrp_path = str(BHGA_DIR / f"{name}.evrp")
    config.decoder = decoder
    config.seed = seed
    config.nb_iter = NB_ITER
    config.mix = mix
    return config


def uncapped(inst: Instance) -> Instance:
    """Copy of the instance without a cargo limit, as used inside BHGA."""
    nodes = []
    for i in range(inst.node_cnt()):
        src = inst.node(i)
        node = Node()
        node.type, node.x, node.y, node.demand = src.type, src.x, src.y, src.demand
        nodes.append(node)
    return Instance(
        inst.name, 0.0, 0, math.inf, inst.battery_capacity(), inst.energy_consumption(),
        inst.customer_cnt(), inst.charging_station_cnt(), nodes,
    )


def split_routes(flat: list[int]) -> list[list[int]]:
    routes, current = [], [0]
    for v in flat[1:]:
        current.append(v)
        if v == 0:
            if len(current) > 2:
                routes.append(current)
            current = [0]
    return routes


@pytest.fixture(scope="module", params=DECODERS)
def result(request, inst):
    return run_bhga(make_config(request.param), inst)


class TestRunResult:
    def test_valid(self, inst, result):
        assert result.energy_feasible
        assert Solution(inst, list(result.routes)).is_valid()

    def test_cost_matches_routes(self, inst, result):
        assert result.cost == pytest.approx(Solution(inst, list(result.routes)).cost())
        assert result.cost == pytest.approx(result.bhga_cost)

    def test_customer_routes(self, inst, result):
        customer_routes = list(result.customer_routes)
        assert Solution(inst, customer_routes).is_cargo_valid()
        customers = [v for v in customer_routes if inst.is_customer(v)]
        assert sorted(customers) == list(range(1, inst.customer_cnt() + 1))
        assert customers == [v for v in result.routes if inst.is_customer(v)]

    def test_statistics(self, result):
        assert result.iterations >= NB_ITER
        assert result.charge_calls > 0
        assert result.charge_ns > 0
        assert len(result.progress) >= 1
        costs = [cost for _, cost, _ in result.progress]
        assert costs == sorted(costs, reverse=True)


class TestConfig:
    @pytest.mark.parametrize("decoder", DECODERS)
    def test_deterministic(self, inst, decoder):
        a = run_bhga(make_config(decoder, seed=7), inst)
        b = run_bhga(make_config(decoder, seed=7), inst)
        assert list(a.routes) == list(b.routes)
        assert a.iterations == b.iterations

    def test_unknown_decoder(self, inst):
        with pytest.raises(ValueError):
            run_bhga(make_config("nope"), inst)

    @pytest.mark.parametrize("mix", ["12016", "12216", "1211", "12a16"])
    def test_unsupported_mix(self, inst, mix):
        with pytest.raises(ValueError):
            run_bhga(make_config("vanilla", mix=mix), inst)

    def test_missing_instance(self, inst):
        with pytest.raises(RuntimeError):
            run_bhga(make_config("vanilla", name="does-not-exist"), inst)


class TestDecoderDominance:
    """SS-FR-FLA is exact for at most one station per edge, which includes
    every insertion focusEnumeration can produce, so per route it is never
    worse and never fails where focusEnumeration succeeds."""

    def test_ss_never_worse_per_route(self, inst):
        free = uncapped(inst)
        battery = BatteryDecode(free)
        bhga = BhgaDecode(str(SMALL_INSTANCE))
        rng = random.Random(42)
        perm = list(range(1, inst.customer_cnt() + 1))
        for _ in range(SHUFFLE_ITERATIONS):
            rng.shuffle(perm)
            for route in split_routes(list(linear_split(Solution(inst, perm)).routes)):
                vanilla = Solution(inst, list(bhga.decode(route)))
                ss = battery.decode(Solution(free, route), math.inf, True)
                if vanilla.is_energy_and_cargo_valid():
                    assert ss.is_energy_and_cargo_valid()
                    assert ss.cost() <= vanilla.cost() + 1e-9
