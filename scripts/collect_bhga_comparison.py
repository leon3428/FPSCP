"""Run the full BHGA metaheuristic with its own charging decoder (vanilla
focusEnumeration) and with SS-FR-FLA, and record the results as JSON lines.

Each run is one line in the output file, written as soon as it finishes, so an
interrupted collection can be resumed by running the same command again.
"""

import argparse
import json
import math
import os
import re
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

from evrp_decoder.loader import load_evrp
from evrp_decoder._evrp_core import (
    BatteryDecode,
    BhgaRunConfig,
    OptimalDecode,
    Solution,
    run_bhga,
)

DECODERS = ("vanilla", "ss_fr_fla")


def parse_args():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--dataset-dir", default="dataset")
    p.add_argument(
        "--bhga-instances",
        default="extern/bhga/Instances/CVRP",
        help="Directory with the matching .vrp and .evrp files BHGA reads",
    )
    p.add_argument("--output", default="results/bhga_comparison.jsonl")
    # BHGA seeds a std::minstd_rand, which treats seed 0 the same as seed 1,
    # so seeds start at 1.
    p.add_argument("--seeds", type=int, default=16, help="Seeds 1..N per instance and decoder")
    p.add_argument("--nb-iter", type=int, default=30000, help="Iterations without improvement")
    p.add_argument("--mix", default="12116", help="BHGA -mix configuration (12116 = BHGA-SP)")
    p.add_argument("--instances", nargs="*", help="Only run these instances (default: all)")
    p.add_argument("--decoders", nargs="*", default=list(DECODERS), choices=DECODERS)
    p.add_argument("--workers", type=int, default=os.cpu_count())
    return p.parse_args()


def customer_count(name):
    return int(re.search(r"-n(\d+)-", name).group(1))


def redecode(inst, customer_routes):
    """Decode the final routes again with the decoders from the paper, to
    separate a better decoder inside the search from a better final decoding."""
    battery = BatteryDecode(inst)
    out = {}
    for key, single_cs in [("ss_fr_fla", True), ("fr_fla", False)]:
        sol = battery.decode(Solution(inst, customer_routes), math.inf, single_cs)
        out[key] = sol.cost() if sol.is_valid() else None

    giant_tour = [v for v in customer_routes if inst.is_customer(v)]
    sol = OptimalDecode(inst).decode(Solution(inst, giant_tour), math.inf)
    out["fp_fla"] = sol.cost() if sol.is_valid() else None
    return out


def run_task(dataset_dir, bhga_dir, name, seed, decoder, nb_iter, mix):
    inst = load_evrp(Path(dataset_dir) / f"{name}.evrp")

    config = BhgaRunConfig()
    config.vrp_path = str(Path(bhga_dir) / f"{name}.vrp")
    config.evrp_path = str(Path(bhga_dir) / f"{name}.evrp")
    config.decoder = decoder
    config.seed = seed
    config.nb_iter = nb_iter
    config.mix = mix
    res = run_bhga(config, inst)

    routes = list(res.routes)
    customer_routes = list(res.customer_routes)
    return {
        "instance": name,
        "seed": seed,
        "decoder": decoder,
        "nb_iter": nb_iter,
        "mix": mix,
        "cost": res.cost,
        "bhga_cost": res.bhga_cost,
        "valid": Solution(inst, routes).is_valid(),
        "energy_feasible": res.energy_feasible,
        "iterations": res.iterations,
        "elapsed_s": res.elapsed_s,
        "cpu_s": res.cpu_s,
        "charge_calls": res.charge_calls,
        "charge_ns": res.charge_ns,
        "progress": [list(p) for p in res.progress],
        "routes": routes,
        "customer_routes": customer_routes,
        "redecode": redecode(inst, customer_routes),
    }


def load_done(output_path):
    done = set()
    if not output_path.exists():
        return done
    with open(output_path) as f:
        for line in f:
            if line.strip():
                r = json.loads(line)
                done.add((r["instance"], r["seed"], r["decoder"], r["nb_iter"], r["mix"]))
    return done


def main():
    args = parse_args()

    names = sorted(p.stem for p in Path(args.dataset_dir).glob("*.evrp"))
    if args.instances:
        missing = set(args.instances) - set(names)
        if missing:
            raise SystemExit(f"Unknown instances: {sorted(missing)}")
        names = [n for n in names if n in args.instances]

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    done = load_done(output_path)

    # Largest instances first so the longest runs don't end up at the tail.
    tasks = [
        (name, seed, decoder)
        for name in sorted(names, key=customer_count, reverse=True)
        for seed in range(1, args.seeds + 1)
        for decoder in args.decoders
        if (name, seed, decoder, args.nb_iter, args.mix) not in done
    ]
    print(f"{len(tasks)} runs to do ({len(done)} already in {output_path}), {args.workers} workers")

    t0 = time.perf_counter()
    with ProcessPoolExecutor(max_workers=args.workers) as executor, open(output_path, "a") as out:
        futures = {
            executor.submit(
                run_task, args.dataset_dir, args.bhga_instances, name, seed, decoder, args.nb_iter, args.mix
            ): (name, seed, decoder)
            for name, seed, decoder in tasks
        }
        for k, future in enumerate(as_completed(futures), 1):
            name, seed, decoder = futures[future]
            try:
                record = future.result()
            except Exception as e:
                print(f"[ERROR] {name} seed={seed} {decoder}: {e}", flush=True)
                continue
            out.write(json.dumps(record) + "\n")
            out.flush()
            print(
                f"[{k}/{len(tasks)} {time.perf_counter() - t0:.0f}s] {name} seed={seed} {decoder}: "
                f"cost={record['cost']:.2f} valid={record['valid']} "
                f"it={record['iterations']} cpu={record['cpu_s']:.1f}s",
                flush=True,
            )


if __name__ == "__main__":
    main()
