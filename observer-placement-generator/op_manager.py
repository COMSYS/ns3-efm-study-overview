import networkx as nx
import os
import json
from pydantic import BaseModel, RootModel
from op_generators import ObserverPlacementAlgorithm, MeasurementType, FlowPathGenerator, PlacementGenerator

# Filepaths
place_gen_folder = os.path.dirname(os.path.realpath(__file__))
output_folder = os.path.join(place_gen_folder, "output")
default_output_file = os.path.join(output_folder, "observer-placements.json")
automation_folder = os.path.join(os.path.dirname(place_gen_folder), "automation")
topology_folder = os.path.join(automation_folder, "topologies")
topology_lookup_file = os.path.join(topology_folder, "topology-lookup.json")


# Data models
class PlacementData(BaseModel):
    measurement_type: set[MeasurementType]
    algorithm: ObserverPlacementAlgorithm
    edge_links: bool
    exists: bool
    placement: set[int]

class ObserverPlacementFile(RootModel):
    root: dict[str, list[PlacementData]]
    
    def __getitem__(self, topology_name: str) -> list[PlacementData]:
        return self.root[topology_name]
    
    def __iter__(self):
        return iter(self.root)
    
    def __contains__(self, topology_name: str) -> bool:
        return topology_name in self.root
    
    def __setitem__(self, topology_name: str, placement_data: list[PlacementData]):
        self.root[topology_name] = placement_data

# Manager
class ObserverPlacementManager:
    def __init__(self, op_file: str | None = None, topology_file_mapping: dict[str, str] | None = None):

        # Load placement data
        if op_file is None:
            op_file = default_output_file
        self.op_file : str = op_file
        self.op_data = ObserverPlacementFile({})
        if os.path.exists(op_file):
            with open(op_file, "r") as file:
                self.op_data = ObserverPlacementFile.model_validate_json(file.read())
        # Load topology file mapping
        if topology_file_mapping is None:
            with open(topology_lookup_file, "r") as topology_lookup:
                self.topology_file_mapping = json.load(topology_lookup)
                for topology_name, topology_file_name in self.topology_file_mapping.items():
                    self.topology_file_mapping[topology_name] = os.path.join(topology_folder, topology_file_name)
        else:
            self.topology_file_mapping = topology_file_mapping
        
        self.topology_mapping : dict[str, nx.Graph] = {}
        
    
    def __import_topology(self, filepath: str) -> nx.Graph:
        G = nx.Graph()
        with open(filepath) as topology_file:
            ## Iterate through topology file
            for line in topology_file.readlines():
                line_split = line.split(",")
                if line_split[0] == "node":
                    G.add_node(int(line_split[1]))

                elif line_split[0] == "link":
                    G.add_edge(int(line_split[1]), int(line_split[2]))
        return G
    
    def __save_data(self, new_placement: tuple[str, PlacementData] | None = None):
        if new_placement is not None:
            if new_placement[0] not in self.op_data:
                self.op_data[new_placement[0]] = []
            self.op_data[new_placement[0]].append(new_placement[1])
        with open(self.op_file, "w") as file:
            file.write(self.op_data.model_dump_json())
    
    def topology_is_valid(self, G: nx.Graph) -> bool:
        return nx.is_connected(G)
    
    def get_topology(self, topology_name: str) -> nx.Graph:
        if topology_name not in self.topology_mapping:
            if topology_name not in self.topology_file_mapping:
                raise ValueError(f"Topology {topology_name} not found in mapping")
            topology_file = self.topology_file_mapping[topology_name]
            if not os.path.exists(topology_file):
                raise FileNotFoundError(f"Topology file {topology_file} does not exist")
            self.topology_mapping[topology_name] = self.__import_topology(topology_file)
            if not self.topology_is_valid(self.topology_mapping[topology_name]):
                raise ValueError(f"Topology {topology_name} is not a valid topology")
        return self.topology_mapping[topology_name] 

    def get_placement(self, topology_name: str, measurement_type: set[MeasurementType], algorithm: ObserverPlacementAlgorithm, edge_links: bool) -> PlacementData | None:
        if topology_name not in self.op_data:
            return None
        for placement in self.op_data[topology_name]:
            if placement.measurement_type == measurement_type and placement.algorithm == algorithm and placement.edge_links == edge_links:
                return placement
        return None
    
    def get_or_generate_placement(self, topology_name: str, measurement_type: set[MeasurementType], algorithm: ObserverPlacementAlgorithm,  edge_links: bool) -> tuple[PlacementData, bool]:
        ex_placement = self.get_placement(topology_name, measurement_type, algorithm, edge_links)
        if ex_placement is not None:
            return ex_placement, True
        
        topology = self.get_topology(topology_name)
        flow_path_gen = FlowPathGenerator(topology)
        placement_gen = PlacementGenerator(topology, measurement_type, flow_path_gen, edge_links)
        placement = placement_gen.generate(algorithm)
        if placement is None:
            exists = False
            placement = set(topology.nodes)
        else:
            exists = True
        placement_data = PlacementData(measurement_type=measurement_type, algorithm=algorithm, edge_links=edge_links, exists=exists, placement=placement)
        self.__save_data((topology_name, placement_data))
        return placement_data, False

    def generate_placements(self, topology_names: set[str], measurement_types: list[set[MeasurementType]], algorithms: set[ObserverPlacementAlgorithm], edge_links: set[bool], print_progress: bool = False):
        for topology_name in topology_names:
            for measurement_type in measurement_types:
                for algorithm in algorithms:
                    for edge_link in edge_links:
                        if print_progress:
                            print(f"Start generating placement for {topology_name}, {set(map(str, measurement_type))}, {algorithm}, {edge_link}", end="")
                        if self.get_or_generate_placement(topology_name, measurement_type, algorithm, edge_link)[1]:
                            if print_progress:
                                print(" - Already exists")
                        else:
                            if print_progress:
                                print(" - Done")
    
    def generate_all_placements(self, topology_names: set[str], print_progress: bool = False):
        sensible_measurement_types = [
            {MeasurementType.EFM_Q},
            {MeasurementType.EFM_L},
            {MeasurementType.EFM_T_SPIN},
            {MeasurementType.EFM_Q, MeasurementType.EFM_L, MeasurementType.EFM_QL},
            {MeasurementType.EFM_Q, MeasurementType.EFM_T_SPIN, MeasurementType.EFM_QT},
            {MeasurementType.EFM_Q, MeasurementType.EFM_R, MeasurementType.EFM_QR},
            {MeasurementType.EFM_L, MeasurementType.EFM_T_SPIN, MeasurementType.EFM_LT}]

        self.generate_placements(topology_names, sensible_measurement_types, set(ObserverPlacementAlgorithm), {True, False}, print_progress)
