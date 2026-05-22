"""Load and parse .evrp benchmark instance files."""

from pathlib import Path

from evrp_decoder._evrp_core import Instance, Node, NodeType


def load_evrp(filepath: str | Path) -> Instance:
    """Parse an .evrp file and return an :class:`Instance`.

    Parameters
    ----------
    filepath : str | Path
        Path to the .evrp file.

    Returns
    -------
    Instance
        Populated C++ Instance object.
    """
    filepath = Path(filepath)

    # Header metadata
    name = ""
    optimal_value = 0.0
    vehicles = 0
    dimension = 0
    stations = 0
    capacity = 0.0
    energy_capacity = 0.0
    energy_consumption = 0.0

    # Raw parsed data indexed by 1-based node ID from the file
    coords: dict[int, tuple[float, float]] = {}
    demands: dict[int, float] = {}
    station_ids: set[int] = set()
    depot_id = 1

    section = None

    with open(filepath) as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line or line == "EOF":
                continue

            # Detect section headers
            if line.startswith("NODE_COORD_SECTION"):
                section = "coords"
                continue
            elif line.startswith("DEMAND_SECTION"):
                section = "demands"
                continue
            elif line.startswith("STATIONS_COORD_SECTION"):
                section = "stations"
                continue
            elif line.startswith("DEPOT_SECTION"):
                section = "depot"
                continue

            # Parse header key-value pairs
            if section is None:
                if line.startswith("Name:"):
                    name = line.split(":", 1)[1].strip()
                elif line.startswith("OPTIMAL_VALUE:"):
                    optimal_value = float(line.split(":")[1].strip())
                elif line.startswith("VEHICLES:"):
                    vehicles = int(line.split(":")[1].strip())
                elif line.startswith("DIMENSION:"):
                    dimension = int(line.split(":")[1].strip())
                elif line.startswith("STATIONS:"):
                    stations = int(line.split(":")[1].strip())
                elif line.startswith("CAPACITY:"):
                    capacity = float(line.split(":")[1].strip())
                elif line.startswith("ENERGY_CAPACITY:"):
                    energy_capacity = float(line.split(":")[1].strip())
                elif line.startswith("ENERGY_CONSUMPTION:"):
                    energy_consumption = float(line.split(":")[1].strip())
                continue

            # Parse section data
            parts = line.split()

            if section == "coords":
                nid = int(parts[0])
                coords[nid] = (float(parts[1]), float(parts[2]))

            elif section == "demands":
                nid = int(parts[0])
                demands[nid] = float(parts[1])

            elif section == "stations":
                station_ids.add(int(parts[0]))

            elif section == "depot":
                val = int(parts[0])
                if val != -1:
                    depot_id = val

    # Build ordered node list: depot, customers, charging stations
    customer_ids = [
        nid
        for nid in sorted(coords)
        if nid != depot_id and nid not in station_ids
    ]
    sorted_station_ids = sorted(station_ids)

    nodes: list[Node] = []

    # Depot
    def _make_node(nid: int, ntype: NodeType) -> Node:
        n = Node()
        n.type = ntype
        n.x, n.y = coords[nid]
        n.demand = demands.get(nid, 0.0)
        return n

    nodes.append(_make_node(depot_id, NodeType.Depot))

    for nid in customer_ids:
        nodes.append(_make_node(nid, NodeType.Customer))

    for nid in sorted_station_ids:
        nodes.append(_make_node(nid, NodeType.ChargingStation))

    customer_cnt = len(customer_ids)
    charging_station_cnt = len(sorted_station_ids)

    return Instance(
        name=name,
        optimal_value=optimal_value,
        minimum_route_cnt=vehicles,
        cargo_capacity=capacity,
        battery_capacity=energy_capacity,
        energy_consumption=energy_consumption,
        customer_cnt=customer_cnt,
        charging_station_cnt=charging_station_cnt,
        nodes=nodes,
    )


def load_all(dataset_dir: str | Path) -> dict[str, Instance]:
    """Load all .evrp files from a directory.

    Parameters
    ----------
    dataset_dir : str | Path
        Path to the directory containing .evrp files.

    Returns
    -------
    dict[str, Instance]
        Mapping from filename stem to Instance.
    """
    dataset_dir = Path(dataset_dir)
    return {
        p.stem: load_evrp(p) for p in sorted(dataset_dir.glob("*.evrp"))
    }
