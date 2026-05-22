"""frvcpy adapter for EVRP decoding.

Wraps the frvcpy labeling-algorithm solver as a drop-in two-phase decoder
compatible with the notebook's _process_chunk interface.

All charging stations are modelled as instant-recharge-to-100% via a
near-vertical charging function breakpoint pair.
"""

import sys
import time
from pathlib import Path

# Ensure the frvcpy submodule is importable without installation
_FRVCPY_DIR = Path(__file__).parent.parent / "extern" / "frvcpy"
if str(_FRVCPY_DIR) not in sys.path:
    sys.path.insert(0, str(_FRVCPY_DIR))

from frvcpy import algorithm  # noqa: E402
from frvcpy.solver import Solver  # noqa: E402

# ---------------------------------------------------------------------------
# Monkey-patch frvcpy's _build_label_list to add floating-point tolerance
# to a strict > pruning check that can discard optimal labels when
# soc + energy_consumed lands exactly at battery capacity.
# ---------------------------------------------------------------------------
_FP_TOL = 1e-9
_original_build_label_list = algorithm.FrvcpAlgo._build_label_list
_original_relax_arc = algorithm.FrvcpAlgo._relax_arc


def _patched_build_label_list(self, curr_label):
    """Wrapper that temporarily inflates max_energy_at_departure by a tiny
    tolerance so that the boundary check ``soc + consumed > max_e_dep``
    does not misfire due to ≈1e-14 floating-point rounding."""
    if curr_label.last_visited_cs is not None:
        idx = curr_label.last_visited_cs
        orig = self.max_energy_at_departure[idx]
        self.max_energy_at_departure[idx] = orig + _FP_TOL
        try:
            return _original_build_label_list(self, curr_label)
        finally:
            self.max_energy_at_departure[idx] = orig
    return _original_build_label_list(self, curr_label)


def _patched_relax_arc(self, curr_label, next_node_local_id,
                        energy_arc, time_arc):
    """Wrapper that temporarily inflates instance.max_q by a tiny tolerance
    so that the ``soc + charge > max_q`` check in _relax_arc does not
    misfire due to floating-point rounding."""
    orig = self.instance.max_q
    self.instance.max_q = orig + _FP_TOL
    try:
        return _original_relax_arc(self, curr_label, next_node_local_id,
                                   energy_arc, time_arc)
    finally:
        self.instance.max_q = orig


algorithm.FrvcpAlgo._build_label_list = _patched_build_label_list
algorithm.FrvcpAlgo._relax_arc = _patched_relax_arc


class FrvcpyDecode:
    """Two-phase EVRP decoder backed by the frvcpy FRVCP labeling algorithm.

    Usage mirrors BhgaDecode / BacoDecode / CacoDecode:

        dec = FrvcpyDecode(inst)
        result = Solution(inst, dec.decode(split_result.routes))
        elapsed = dec.last_elapsed_ns()
    """

    def __init__(self, inst):
        """Build the frvcpy instance dict from a C++ Instance object.

        Parameters
        ----------
        inst : evrp_decoder._evrp_core.Instance
        """
        N = inst.node_cnt()
        q_max = inst.battery_capacity()
        ec = inst.energy_consumption()
        nc = inst.customer_cnt()
        ns = inst.charging_station_cnt()

        # Add a virtual depot-CS node (index N) co-located with the depot so
        # frvcpy can plan depot-return recharging stops like BatteryDecode.
        # frvcpy crashes if the actual depot (node 0) is listed as a CS because
        # its path-tracking assumes the route origin is not a CS.
        M = N + 1  # extended node count
        energy_matrix = [[0.0] * M for _ in range(M)]
        time_matrix = [[0.0] * M for _ in range(M)]
        for i in range(N):
            for j in range(N):
                d = inst.distance(i, j)
                energy_matrix[i][j] = d * ec
                time_matrix[i][j] = d
            # virtual depot-CS (index N) has same distances as depot (index 0)
            energy_matrix[i][N] = inst.distance(i, 0) * ec
            energy_matrix[N][i] = inst.distance(0, i) * ec
            time_matrix[i][N] = inst.distance(i, 0)
            time_matrix[N][i] = inst.distance(0, i)

        # Instant recharge: slope = max_q / 1e-9 ≈ ∞, so charging time ≈ 0
        css = [
            {"node_id": nc + 1 + k, "cs_type": 0} for k in range(ns)
        ] + [{"node_id": N, "cs_type": 0}]
        breakpoints = [{"cs_type": 0, "time": [0.0, 1e-9], "charge": [0.0, q_max]}]

        self._instance_dict = {
            "max_q": q_max,
            "energy_matrix": energy_matrix,
            "time_matrix": time_matrix,
            "process_times": [0.0] * M,
            "css": css,
            "breakpoints_by_type": breakpoints,
        }
        self._depot_cs_id = N
        self._q_max = q_max
        self._last_elapsed_ns = 0

    def decode(self, routes: list) -> list:
        """Insert charging stations optimally into split routes.

        Parameters
        ----------
        routes : list[int]
            Flat depot-separated route list from linear_split, e.g.
            [0, c1, c2, 0, c3, c4, c5, 0].

        Returns
        -------
        list[int]
            Same format with charging stations inserted.  If a sub-route is
            infeasible, it is returned unchanged so that Solution.is_valid()
            will return False.
        """
        t0 = time.perf_counter_ns()

        # Split flat list into per-vehicle customer sequences
        segments = []
        current = []
        for node in routes:
            if node == 0:
                if current:
                    segments.append(current)
                    current = []
            else:
                current.append(node)
        if current:
            segments.append(current)

        # Start with the leading depot; each segment appends its inner nodes + trailing depot
        result = [0]
        for seg in segments:
            frvcp_route = [0] + seg + [0]
            slv = Solver(self._instance_dict, frvcp_route, self._q_max)
            duration, feas_route = slv.solve()

            if duration == float("inf") or feas_route is None:
                # Infeasible: pass through without stations
                result.extend(seg)
            else:
                # feas_route is [(node_id, charge_or_None), ...] depot-to-depot
                # skip first (depot) and last (depot); next segment's leading 0 is our trailing 0
                # Map virtual depot-CS back to real depot (node 0)
                for node_id, _ in feas_route[1:-1]:
                    result.append(0 if node_id == self._depot_cs_id else node_id)
            result.append(0)

        # Remove consecutive depots that arise from depot-CS visits at
        # segment boundaries (the virtual depot-CS maps back to node 0).
        cleaned = [result[0]]
        for node in result[1:]:
            if not (node == 0 and cleaned[-1] == 0):
                cleaned.append(node)

        self._last_elapsed_ns = time.perf_counter_ns() - t0
        return cleaned

    def last_elapsed_ns(self) -> int:
        """Elapsed nanoseconds for the most recent decode() call."""
        return self._last_elapsed_ns
