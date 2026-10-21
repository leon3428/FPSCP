# FPSCP

Decoders and comparison tools for the Fixed Permutation Splitting and Charging Problem(FPSCP) and Fixed Route Vehicle Charging Problem(FRVCP).

## Features

- C++ implementations for EVRP instances, solutions, validation, route costs,
  linear cargo splitting, battery decoding, and combined optimal decoding.
- Python bindings packaged as `evrp_decoder._evrp_core`.
- Loader utilities for `.evrp` benchmark files.
- Adapters for external/reference decoders:
  - BHGA focus enumeration
  - BACO and CACO from CEVRP
  - `frvcpy` fixed-route charging station insertion
- Scripts for generating customer permutations and collecting decoder
  comparison data.
- Notebooks for data exploration, front-size analysis, and decoder comparison.

## Repository Layout

```text
.
|-- evrp_decoder/        Python package, loaders, generators, frvcpy adapter
|-- src/evrp/            C++ EVRP core and decoders
|-- src/bindings/        pybind11 module definition
|-- scripts/             Experiment data-generation scripts
|-- notebooks/           Analysis notebooks
|-- tests/               pytest suite
|-- extern/              Git submodules used by comparison decoders
|-- CMakeLists.txt       CMake/scikit-build configuration
`-- pyproject.toml       Python package metadata and tooling
```

## Requirements

- Python 3.10+
- CMake 3.20+
- A C++ compiler with C++23 support
- `uv` or another Python environment manager
- Git submodules initialized under `extern/`

The project is built through `scikit-build-core`, so installing the Python
package also compiles the C++ extension.

## Setup

Clone with submodules, or initialize them after cloning:

```bash
git submodule update --init --recursive
```

Create the environment and install the package.  This compiles the C++
extension through scikit-build:

```bash
uv sync --dev
```

## Dataset

Tests and scripts expect EVRP benchmark files in a top-level `dataset/`
directory:

```text
dataset/
|-- E-n22-k4.evrp
|-- ...
`-- X-n1001-k43.evrp
```

The dataset directory is not created by the setup commands; add the benchmark
files before running the full test suite or experiment scripts.

The loader accepts any directory containing `.evrp` files:

```python
from evrp_decoder.loader import load_all, load_evrp

inst = load_evrp("dataset/E-n22-k4.evrp")
instances = load_all("dataset")
```

## Quick Example

```python
from evrp_decoder._evrp_core import OptimalDecode, Solution, linear_split
from evrp_decoder.loader import load_evrp

inst = load_evrp("dataset/E-n22-k4.evrp")

# Customer IDs are 1..inst.customer_cnt(); node 0 is the depot.
customers = list(range(1, inst.customer_cnt() + 1))

# Combined decoder inserts depots and charging stations while preserving
# customer order.
decoder = OptimalDecode(inst)
decoded = decoder.decode(Solution(inst, customers))

print(decoded.routes)
print(decoded.cost())
print(decoded.is_valid())
```

## Decoder Overview

`Solution` routes are flat node-ID lists.  Depot visits are represented by
`0`, customers by `1..customer_cnt`, and charging stations by
`customer_cnt + 1..node_cnt - 1`.

| Decoder | Notes |
| --- | --- |
| `linear_split`  | Adds depot separators to satisfy cargo capacity. |
| `OptimalDecode` | Combined cargo and battery decoder. |
| `BatteryDecode` | Inserts charging stations for fixed split routes. |
| `BhgaDecode`    | Wraps the BHGA focus-enumeration implementation. |
| `BacoDecode`    | Wraps the BACO implementation from CEVRP. |
| `CacoDecode`    | Wraps the CACO implementation from CEVRP. |
| `FrvcpyDecode`  | Wraps `frvcpy`. |

For two-phase decoders, call `linear_split` first and pass `split.routes` to
the charging-station decoder.

## Running Tests

```bash
uv run task test
```

or:

```bash
uv run pytest tests/ -v
```

The full suite requires:

- initialized submodules in `extern/`
- the compiled Python extension
- a populated `dataset/` directory

## Experiment Scripts

Generate random and k-nearest-neighbour customer permutations:

```bash
uv run python scripts/generate_permutations.py \
  --dataset-dir dataset \
  --output-dir .output \
  --n-permutations 100 \
  --seed 42 \
  --k 3
```

Collect decoder comparison data:

```bash
uv run python scripts/collect_data.py \
  --dataset-dir dataset \
  --permutations .output/permutations_random.json \
  --output .output/decoder_results.json
```

## Notebooks

The `notebooks/` directory includes exploratory and analysis notebooks:

- `data_exploration.ipynb`
- `front_size_analysis.ipynb`
- `decoder_comparison.ipynb`

Start Jupyter with:

```bash
uv run jupyter lab
```

## Development Notes

- CMake builds `cevrp_lib` as a shared library with hidden symbols because the
  CEVRP and BHGA submodules both define incompatible global `Case` classes.
- `evrp_decoder.frvcpy_decode` imports `extern/frvcpy` directly, so the
  `frvcpy` submodule must be present even if it is not installed separately.
