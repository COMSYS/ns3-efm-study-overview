from __future__ import annotations
from collections import defaultdict
import networkx as nx
from enum import IntEnum
import itertools
import gurobipy
from multiprocessing import Pool
import random
import math

def round_half_up(n, decimals=0):
    multiplier = 10**decimals
    return math.floor(n * multiplier + 0.5) / multiplier

class ObserverPlacementAlgorithm(IntEnum):
    GREEDY = 0
    DETECTIP = 1
    ONEIDIP = 2
    RANDOM_FIVE = 3
    RANDOM_TEN = 4
    RAND_PERC_25 = 5
    RAND_PERC_50 = 6
    RAND_PERC_75 = 7

class MeasurementType(IntEnum):
    EFM_Q = 0
    EFM_L = 1
    EFM_R = 2
    EFM_T_SPIN = 3
    EFM_QR = 5
    EFM_QL = 6
    EFM_QT = 7
    EFM_LT = 8

    def __str__(self) -> str:
        return self.name

class Path(tuple[int, ...]):
    def get_up_to(self, node: int) -> Path:
        if node not in self:
            raise ValueError("Node not in path")
        return Path(self[:self.index(node)+1])
    
    def get_from(self, node: int) -> Path:
        if node not in self:
            raise ValueError("Node not in path")
        return Path(self[self.index(node):])
    
    def get_links(self) -> set[tuple[int,int]]:
        return set((self[i], self[i+1]) for i in range(len(self)-1))
    
    def get_reverse(self) -> Path:
        return Path(reversed(self))
    
    def prepend(self, node: int) -> Path:
        return Path((node,) + self)
    
    def append(self, node: int) -> Path:
        return Path(super().__add__((node,)))
    
    def __add__(self, other: Path) -> Path:
        return Path(super().__add__(other))

def path_to_measurement_paths(path: Path, observer: int, measurement_type: MeasurementType, bidirectional: bool) -> set[Path]:
    measurement_paths = set()
    reverse_path = path.get_reverse()
    if measurement_type == MeasurementType.EFM_Q:
        measurement_paths.add(path.get_up_to(observer))
    elif measurement_type == MeasurementType.EFM_L:
        measurement_paths.add(path)
    elif measurement_type == MeasurementType.EFM_R:
        measurement_paths.add(reverse_path + path.get_up_to(observer))
    elif measurement_type == MeasurementType.EFM_T_SPIN:
        measurement_paths.add(path + reverse_path)
        if bidirectional:
            measurement_paths.add(path.get_from(observer) + reverse_path.get_up_to(observer))
    elif measurement_type == MeasurementType.EFM_QR:
        measurement_paths.add(reverse_path)
        if bidirectional:
            measurement_paths.add(path.get_from(observer))
            measurement_paths.add(path.get_from(observer) + reverse_path.get_up_to(observer))
        pass
    elif measurement_type == MeasurementType.EFM_QL:
        measurement_paths.add(path.get_from(observer))
    elif measurement_type == MeasurementType.EFM_QT:
        measurement_paths.add(path.get_from(observer) + reverse_path)
        if bidirectional:
            measurement_paths.add(path.get_from(observer))
            measurement_paths.add(path)
    elif measurement_type == MeasurementType.EFM_LT:
        measurement_paths.add(reverse_path)

    return measurement_paths

class FlowPathGenerator:
    def __init__(self, topology: nx.Graph) -> None:
        self.topology = topology
        self.paths = None

    def generate(self) -> set[Path]:
        if self.paths is not None:
            return self.paths

        paths = set()
        for node1, node2 in itertools.combinations(self.topology.nodes,2):
            sp = nx.shortest_path(self.topology, node1, node2)
            paths.add(Path(sp))
            paths.add(Path(reversed(sp))) # add reverse path
        self.paths = paths
        return paths



class PlacementGenerator:
    ENDHOST = -1

    def __init__(self, topology: nx.Graph, measurement_types: set[MeasurementType], flow_path_gen: FlowPathGenerator, network_edge_links: bool) -> None:
        self.topology = topology
        self.measurement_types = measurement_types
        self.obsv_links, self.obsv_paths = self.__generate_obsv_links(flow_path_gen, network_edge_links) 
        self.all_links = frozenset(self.__get_all_links(network_edge_links))
        pass

    def __generate_obsv_links(self, flow_path_gen: FlowPathGenerator, network_edge_links: bool) -> tuple[dict[int, set[tuple[int,int]]], dict[int, list[set[tuple[int,int]]]]]:
        flow_paths = flow_path_gen.generate()
        obsv_links = {obsv : set() for obsv in self.topology.nodes}
        obsv_paths = {obsv : list() for obsv in self.topology.nodes}
        for flow_path in flow_paths:
            if network_edge_links:
                extended_flow_path = flow_path.prepend(self.ENDHOST).append(self.ENDHOST)
            else:
                extended_flow_path = flow_path
            for node in flow_path:
                mps = set()
                for measurement_type in self.measurement_types:
                    mps.update(path_to_measurement_paths(extended_flow_path, node, measurement_type, True))
                for mp in mps:
                    if len(mp) > 1:
                        link_set = mp.get_links()
                        obsv_links[node].update(link_set)
                        obsv_paths[node].append(link_set)
        return obsv_links, obsv_paths

    def __get_all_links(self, network_edge_links: bool) -> set[tuple[int,int]]:
        links = set()
        assert self.topology.is_directed() == False
        for edge in self.topology.edges:
            links.add(edge)
            links.add(edge[::-1])
        if network_edge_links:
            for node in self.topology.nodes:
                links.add((node, self.ENDHOST))
                links.add((self.ENDHOST, node))
        return links

    def generate(self, algorithm: ObserverPlacementAlgorithm, none_is_all: bool = False) -> set[int] | None:
        result = set()
        if algorithm == ObserverPlacementAlgorithm.GREEDY:
            result = self.generate_greedy()
        elif algorithm == ObserverPlacementAlgorithm.DETECTIP:
            result = self.generate_detect_ip()
        elif algorithm == ObserverPlacementAlgorithm.ONEIDIP:
            result = self.generate_oneid_ip()
        elif algorithm == ObserverPlacementAlgorithm.RANDOM_FIVE:
            result = self.generate_random(5)
        elif algorithm == ObserverPlacementAlgorithm.RANDOM_TEN:
            result = self.generate_random(10)
        elif algorithm == ObserverPlacementAlgorithm.RAND_PERC_25:
            result = self.generate_random(int(round_half_up(len(self.topology.nodes) * 0.25,0)))
        elif algorithm == ObserverPlacementAlgorithm.RAND_PERC_50:
            result = self.generate_random(int(round_half_up(len(self.topology.nodes) * 0.5,0)))
        elif algorithm == ObserverPlacementAlgorithm.RAND_PERC_75:
            result = self.generate_random(int(round_half_up(len(self.topology.nodes) * 0.75,0)))
        else:
            raise NotImplementedError()
        if result is None and none_is_all:
            return set(self.topology.nodes)
        else:
            return result

    def generate_greedy(self) -> set[int] | None:
        placement = set()
        observers = set(self.topology.nodes)
        uncovered_links = set(self.all_links)
        while uncovered_links:
            if not observers:
                return None
            best_observer = None
            best_observer_links = -1
            for observer in observers:
                observer_links = len(uncovered_links.intersection(self.obsv_links[observer]))
                if observer_links > best_observer_links:
                    best_observer = observer
                    best_observer_links = observer_links
            assert best_observer is not None
            placement.add(best_observer)
            observers.remove(best_observer)
            uncovered_links.difference_update(self.obsv_links[best_observer])
        return placement

    def generate_random(self, number_of_observers) -> set[int] | None:
        placement = set()
        observers = set(self.topology.nodes)
        
        for _ in range(number_of_observers):
            if not observers:
                return None
            available_observers = list(observers)
            observer = random.choice(available_observers)
            assert observer is not None
            placement.add(observer)
            observers.remove(observer)
        return placement

    def _compute_observer_cover_matrix(self) -> dict[int, dict[tuple[int,int], int]]:
        ob_coverage = dict()
        for observer, link_set in self.obsv_links.items():
            ob_coverage[observer] = defaultdict(int)
            for link in link_set:
                ob_coverage[observer][link] = 1
        return ob_coverage
    
    def _compute_observer_diff_matrix_p(self) -> dict[int, dict[tuple[tuple[int,int],tuple[int,int]], int]]:
        ob_diff_matrix = dict()
        observers = list(self.obsv_links.keys())
        cores = 6
        with Pool(cores) as p:
            args = [(self, ob) for ob in observers]
            results = p.map(_compute_observer_diff_matrix_p_helper, args, len(args)//cores)
            for i in range(len(observers)):
                ob_diff_matrix[observers[i]] = results[i]

        return ob_diff_matrix

    def generate_detect_ip(self) -> set[int] | None:
        ob_coverage = self._compute_observer_cover_matrix()

        with gurobipy.Model() as model:
            model.ModelSense = gurobipy.GRB.MINIMIZE

            observer_vars = dict()
            for observer in self.obsv_links:
                observer_vars[observer] = model.addVar(vtype=gurobipy.GRB.BINARY, obj=1, name=f"ob_{observer}")

            model.update()

            for link in self.all_links:
                model.addConstr(gurobipy.quicksum(observer_vars[observer] * ob_coverage[observer][link] for observer in self.obsv_links) >= 1, name=f"link_{link}")
            
            model.optimize()
            placement = set()
            if model.Status == gurobipy.GRB.OPTIMAL:
                for ob, var in observer_vars.items():
                    if var.x > 0.5:
                        placement.add(ob)
                return placement
            else:
                return None
            
    def generate_oneid_ip(self) -> set[int] | None:
        ob_coverage = self._compute_observer_cover_matrix()
        ob_diff_matrix = self._compute_observer_diff_matrix_p()

        with gurobipy.Model() as model:
            model.ModelSense = gurobipy.GRB.MINIMIZE
            observer_vars = dict()
            for observer in self.obsv_links:
                observer_vars[observer] = model.addVar(vtype=gurobipy.GRB.BINARY, obj=1, name=f"ob_{observer}")
            
            model.update()

            link_list = list(self.all_links)

            for link in link_list:
                model.addConstr(gurobipy.quicksum(observer_vars[observer] * ob_coverage[observer][link] for observer in self.obsv_links) >= 1, name=f"link_{link}")
            
            for i in range(len(link_list)):
                for j in range(i+1, len(link_list)):
                    model.addConstr(gurobipy.quicksum(observer_vars[observer] * ob_diff_matrix[observer][(link_list[i], link_list[j])] for observer in self.obsv_links) >= 1, name=f"diff_{link_list[i]}_{link_list[j]}")
            
            model.optimize()

            placement = set()
            if model.Status == gurobipy.GRB.OPTIMAL:
                for ob, var in observer_vars.items():
                    if var.x > 0.5:
                        placement.add(ob)
                return placement
            else:
                return None

def _compute_observer_diff_matrix_p_helper(args: tuple[PlacementGenerator, int]):
    placement_gen, target_observer = args
    link_list = list(placement_gen.all_links)
    link_dict = defaultdict(int)
    remaining = {(l1,l2) for l1 in link_list for l2 in link_list if l1 != l2}
    for measurement_path in placement_gen.obsv_paths[target_observer]:
        if len(measurement_path) == 1:
            try:
                link_list.remove(next(iter(measurement_path)))
            except ValueError:
                pass
        for l1 in link_list:
            if l1 not in measurement_path:
                for l2 in measurement_path:
                    if (l1,l2) in remaining:
                        link_dict[(l1,l2)] = 1
                        link_dict[(l2,l1)] = 1
                        remaining.discard((l1,l2))
                        remaining.discard((l2,l1))
        if not remaining:
            break
    return link_dict