import argparse
import json
import os
import time
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

from evrp_decoder.loader import load_all
from evrp_decoder._evrp_core import (
    BatteryDecode,
    BhgaDecode,
    BacoDecode,
    CacoDecode,
    MontoyaDecode,
    OptimalDecode,
    Solution,
    linear_split,
)
from evrp_decoder.frvcpy_decode import FrvcpyDecode


def parse_args():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--dataset-dir", default="dataset")
    p.add_argument(
        "--permutations",
        required=True,
        help="Path to permutations JSON (from generate_permutations.py)",
    )
    p.add_argument("--output", required=True, help="Output path for results JSON")
    return p.parse_args()


def process_chunk(inst, path, perms):
    optimal_dec = OptimalDecode(inst)
    montoya_dec = MontoyaDecode(inst)
    battery_dec = BatteryDecode(inst)
    bhga_dec = BhgaDecode(str(path))
    baco_dec = BacoDecode(str(path))
    caco_dec = CacoDecode(str(path))
    frvcpy_dec = FrvcpyDecode(inst)

    ret = []
    for perm in perms:
        try:
            res_opt = optimal_dec.decode(Solution(inst, perm))
            t_optimal = optimal_dec.last_elapsed_ns()

            res_montoya = montoya_dec.decode(Solution(inst, perm))
            t_montoya = montoya_dec.last_elapsed_ns()

            t0 = time.perf_counter_ns()
            split_result = linear_split(Solution(inst, perm))
            t_split = time.perf_counter_ns() - t0
            routes = split_result.routes

            res_bat = battery_dec.decode(split_result, False)
            t_battery = battery_dec.last_elapsed_ns()

            res_bat_single = battery_dec.decode(split_result, True)
            t_battery_single = battery_dec.last_elapsed_ns()

            res_bhga = Solution(inst, bhga_dec.decode(routes))
            t_bhga = bhga_dec.last_elapsed_ns()

            res_baco = Solution(inst, baco_dec.decode(routes))
            t_baco = baco_dec.last_elapsed_ns()

            res_caco = Solution(inst, caco_dec.decode(routes))
            t_caco = caco_dec.last_elapsed_ns()

            res_frvcpy = Solution(inst, frvcpy_dec.decode(routes))
            t_frvcpy = frvcpy_dec.last_elapsed_ns()

            ret.append(
                {
                    "combined": res_opt.cost(),
                    "montoya": res_montoya.cost(),
                    "montoya_solved": res_montoya.is_valid(),
                    "battery": res_bat.cost(),
                    "battery_solved": res_bat.is_valid(),
                    "battery_single": res_bat_single.cost(),
                    "battery_single_solved": res_bat_single.is_valid(),
                    "bhga": res_bhga.cost(),
                    "bhga_solved": res_bhga.is_valid(),
                    "baco": res_baco.cost(),
                    "baco_solved": res_baco.is_valid(),
                    "caco": res_caco.cost(),
                    "caco_solved": res_caco.is_valid(),
                    "frvcpy": res_frvcpy.cost(),
                    "frvcpy_solved": res_frvcpy.is_valid(),
                    "t_optimal": t_optimal,
                    "t_montoya": t_montoya,
                    "t_split": t_split,
                    "t_battery": t_battery,
                    "t_battery_single": t_battery_single,
                    "t_bhga": t_bhga,
                    "t_baco": t_baco,
                    "t_caco": t_caco,
                    "t_frvcpy": t_frvcpy,
                }
            )
        except Exception as e:
            import traceback
            print(f"[ERROR] perm={perm}: {e}", flush=True)
            traceback.print_exc()
    return ret


def main():
    args = parse_args()

    instances = load_all(args.dataset_dir)
    instance_paths = {p.stem: p for p in sorted(Path(args.dataset_dir).glob("*.evrp"))}
    print(f"Loaded {len(instances)} instances")

    with open(args.permutations) as f:
        all_perms: dict[str, list[list[int]]] = json.load(f)

    worker_cnt = os.cpu_count()

    tasks = []
    for name in sorted(all_perms):
        inst = instances[name]
        path = instance_paths[name]
        perms = all_perms[name]
        n_chunks = max(1, min(worker_cnt, len(perms)))
        chunk_size = (len(perms) + n_chunks - 1) // n_chunks
        for i in range(0, len(perms), chunk_size):
            tasks.append((name, inst, path, perms[i : i + chunk_size]))

    print(f"Submitting {len(tasks)} tasks across {worker_cnt} threads")

    result_parts: dict[str, list] = defaultdict(list)
    done_count: dict[str, int] = defaultdict(int)
    total_chunks: dict[str, int] = defaultdict(int)
    for name, _inst, _path, _chunk in tasks:
        total_chunks[name] += 1

    with ThreadPoolExecutor(max_workers=worker_cnt) as executor:
        futures = {
            executor.submit(process_chunk, inst, path, chunk): name
            for name, inst, path, chunk in tasks
        }
        for future in as_completed(futures):
            name = futures[future]
            try:
                result_parts[name].append(future.result())
            except Exception as e:
                import traceback
                print(f"[ERROR] chunk for {name} failed: {e}", flush=True)
                traceback.print_exc()
            done_count[name] += 1
            if done_count[name] == total_chunks[name]:
                print(f"  {name}: done")

    results = {}
    for name in sorted(result_parts):
        merged = []
        for part in result_parts[name]:
            merged.extend(part)
        results[name] = merged

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "w") as f:
        json.dump(results, f)
    print(f"Saved {output_path}")


if __name__ == "__main__":
    main()
