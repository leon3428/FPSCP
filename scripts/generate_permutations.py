"""Generate random and KNN permutations for all instances in a dataset directory.

Outputs:
  <output-dir>/permutations_random.json
  <output-dir>/permutations_knn.json

Each file has the structure:
  { "<instance-name>": [[customer_id, ...], ...], ... }
"""

import argparse
import json
import random
from pathlib import Path

from evrp_decoder.loader import load_all
from evrp_decoder.generators import knn_permutation

def parse_args():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dataset-dir', default='dataset')
    p.add_argument('--output-dir', default='.output')
    p.add_argument('--n-permutations', type=int, default=10)
    p.add_argument('--seed', type=int, default=42)
    p.add_argument('--k', type=int, default=3, help='KNN neighbourhood size')
    p.add_argument('--max-attempts-factor', type=int, default=4,
                   help='KNN: max attempts = n_permutations * this factor')
    return p.parse_args()


def generate_random(instances, n_permutations, base_rng):
    result = {}
    for name in sorted(instances):
        inst = instances[name]
        inst_seed = base_rng.randint(0, 2**31 - 1)
        rng = random.Random(inst_seed)
        customers = list(range(1, inst.customer_cnt() + 1))
        perms = []
        for _ in range(n_permutations):
            rng.shuffle(customers)
            perms.append(list(customers))
        result[name] = perms
        print(f'  {name}: {len(perms)} random permutations')
    return result


def generate_knn(instances, n_permutations, base_rng, k, max_attempts_factor):
    result = {}
    for name in sorted(instances):
        inst = instances[name]
        inst_seed = base_rng.randint(0, 2**31 - 1)
        rng = random.Random(inst_seed)

        seen: set[tuple] = set()
        perms: list[list[int]] = []
        max_attempts = n_permutations * max_attempts_factor
        attempts = 0
        while len(perms) < n_permutations and attempts < max_attempts:
            perm = knn_permutation(inst, k, rng)
            key = tuple(perm)
            if key not in seen:
                seen.add(key)
                perms.append(perm)
            attempts += 1

        result[name] = perms
        print(f'  {name}: {len(perms)} KNN permutations ({attempts} attempts)')
    return result


def main():
    args = parse_args()
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    instances = load_all(args.dataset_dir)
    print(f'Loaded {len(instances)} instances')

    print('Generating random permutations...')
    base_rng = random.Random(args.seed)
    random_perms = generate_random(instances, args.n_permutations, base_rng)
    out_path = output_dir / 'permutations_random.json'
    with open(out_path, 'w') as f:
        json.dump(random_perms, f)
    print(f'Saved {out_path}')

    print('Generating KNN permutations...')
    base_rng = random.Random(args.seed)
    knn_perms = generate_knn(instances, args.n_permutations, base_rng, args.k, args.max_attempts_factor)
    out_path = output_dir / 'permutations_knn.json'
    with open(out_path, 'w') as f:
        json.dump(knn_perms, f)
    print(f'Saved {out_path}')


if __name__ == '__main__':
    main()
