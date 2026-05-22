"""Tests that validate decoder comparison data from .output/decoder_comparison_knn1000.json.

The 'combined' decoder (OptimalDecode) should produce optimal costs — no other
decoder should produce a lower cost for the same permutation.
"""

import json
from pathlib import Path

import pytest

DATA_FILE = Path(__file__).resolve().parent.parent / ".output" / "decoder_comparison_knn1000.json"

DECODERS_WITH_SOLVED = {
    "battery": "battery_solved",
    "battery_single": "battery_single_solved",
    "bhga": "bhga_solved",
    "baco": "baco_solved",
    "caco": "caco_solved",
    "frvcpy": "frvcpy_solved",
}


@pytest.fixture(scope="module")
def comparison_data() -> dict:
    with open(DATA_FILE) as f:
        return json.load(f)


class TestCombinedOptimality:
    def test_no_decoder_beats_combined(self, comparison_data: dict):
        """Every solved decoder cost should be >= the combined (optimal) cost."""
        violations = []

        for instance_name, entries in comparison_data.items():
            for i, entry in enumerate(entries):
                combined_cost = entry["combined"]

                for decoder, solved_key in DECODERS_WITH_SOLVED.items():
                    if not entry[solved_key]:
                        continue

                    decoder_cost = entry[decoder]
                    if decoder_cost < combined_cost - 1e-9:
                        violations.append(
                            f"{instance_name}[{i}]: {decoder}={decoder_cost:.6f} "
                            f"< combined={combined_cost:.6f}"
                        )

        assert not violations, (
            f"{len(violations)} violation(s) found:\n" + "\n".join(violations)
        )
