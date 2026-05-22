"""Permutation and instance generators for EVRP instances."""

import random

from evrp_decoder._evrp_core import Instance, Node, NodeType


def generate_instance(
    customer_cnt: int,
    charging_station_cnt: int,
    battery_capacity: float,
    cargo_capacity: float,
    energy_consumption: float = 1.0,
    max_demand: float = 100.0,
    rng: random.Random | None = None,
) -> Instance:
    """Generate a random EVRP instance with uniform node positions in [0, 1].

    The depot, customers, and charging stations are each placed uniformly at
    random in the unit square [0, 1]².  Customer demands are drawn uniformly
    from [1, cargo_capacity / 2].

    Parameters
    ----------
    customer_cnt : int
        Number of customer nodes.
    charging_station_cnt : int
        Number of charging-station nodes.
    battery_capacity : float
        Maximum battery energy a vehicle can carry.
    cargo_capacity : float
        Maximum cargo capacity of a vehicle.
    energy_consumption : float, optional
        Energy consumed per unit distance (default ``1.0``).
    minimum_route_cnt : int, optional
        Minimum number of vehicle routes (default ``1``).
    rng : random.Random | None
        Random number generator.  If ``None``, a default one is created.

    Returns
    -------
    Instance
        A fully constructed EVRP instance ready for decoding.
    """
    if rng is None:
        rng = random.Random()

    nodes: list[Node] = []

    # Depot
    depot = Node()
    depot.type = NodeType.Depot
    depot.x = rng.random()
    depot.y = rng.random()
    depot.demand = 0.0
    nodes.append(depot)

    # Customers
    for _ in range(customer_cnt):
        n = Node()
        n.type = NodeType.Customer
        n.x = rng.random()
        n.y = rng.random()
        n.demand = rng.uniform(1.0, max_demand)
        nodes.append(n)

    # Charging stations
    for _ in range(charging_station_cnt):
        n = Node()
        n.type = NodeType.ChargingStation
        n.x = rng.random()
        n.y = rng.random()
        n.demand = 0.0
        nodes.append(n)

    return Instance(
        name="random",
        optimal_value=0.0,
        minimum_route_cnt=0,
        cargo_capacity=cargo_capacity,
        battery_capacity=battery_capacity,
        energy_consumption=energy_consumption,
        customer_cnt=customer_cnt,
        charging_station_cnt=charging_station_cnt,
        nodes=nodes,
    )

def knn_permutation(inst, k, rng=None):
    """Generate a customer permutation using a k-nearest-neighbour heuristic.

    Starting from the depot, the next customer is chosen uniformly at random
    from the *k* closest unvisited customers.  When fewer than *k* unvisited
    customers remain, the choice is made from all of them.

    Parameters
    ----------
    inst : Instance
        EVRP instance — must support ``customer_cnt()``, ``depot_id()``,
        and ``distance(node1_id, node2_id)``.
    k : int
        Number of nearest neighbours to consider at each step.
    rng : random.Random | None
        Random number generator.  If ``None``, a default one is used.

    Returns
    -------
    list[int]
        A permutation of customer node IDs (1-indexed).
    """
    if rng is None:
        rng = random.Random()

    n = inst.customer_cnt()
    depot = 0

    unvisited = set(range(1, n + 1))
    perm = []
    current = depot

    while unvisited:
        # Sort unvisited customers by distance from current node
        neighbours = sorted(unvisited, key=lambda c: inst.distance(current, c))

        # Pick uniformly from the k closest (or all remaining if fewer)
        pool = neighbours[:min(k, len(neighbours))]
        chosen = rng.choice(pool)

        perm.append(chosen)
        unvisited.remove(chosen)
        current = chosen

    return perm
