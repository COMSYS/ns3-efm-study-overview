from geopy.geocoders import Nominatim
from geopy.distance import distance as calculate_distance
from argparse import ArgumentParser
import os
from scipy.constants import speed_of_light
import json

simulation_automation_folder = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
meta_configs_folder = os.path.join(simulation_automation_folder, "configs-meta")
sim_configs_folder = os.path.join(simulation_automation_folder, "simulation", "configs-sim")
ns3_parent_folder = os.path.dirname(os.path.dirname(simulation_automation_folder))
overview_folder = os.path.join(ns3_parent_folder, "overview")
topology_folder = os.path.join(simulation_automation_folder, "topologies")


topology_dict = {
    "G2": "Geant-2023.txt",
    "GEANT2": "Geant-2023.txt"
}

latency_lookup_dict = {
    "G2": "Geant-2023-LATENCY.json",
    "GEANT2": "Geant-2023-LATENCY.json"
}

location_blacklist = ["GÉANT2",
                       "NIX.cz",
                       "Public Internet",
                       "SANET",
                       "AMS-IX",
                       "ACONET",
                       "Pionier"]


def determineRealLatencies(topology, default_latency, verbose=False):

    latency_list = []
    try:
        with open(os.path.join(topology_folder, latency_lookup_dict[topology]), "r") as input_file:
            latency_list = json.load(input_file)
    except (FileNotFoundError, json.decoder.JSONDecodeError):
        print(f"Link latencies for {topology} have not been determined before. Do it this time and then save the results to {os.path.join(topology_folder, latency_lookup_dict[topology])}")

        if os.path.exists(os.path.join(topology_folder, latency_lookup_dict[topology])):
            os.remove(os.path.join(topology_folder, latency_lookup_dict[topology]))

        geolocator = Nominatim(user_agent="TopologyEnricher")

        node_list = []
        node_lookup = {}
        link_list = []

        with open(os.path.join(topology_folder, topology_dict[topology]), "r") as input_file:

            for line in input_file.readlines():
                line_split = line.replace("\n", "").split(",")

                if line_split[0] == "node":
                    node_number = int(line_split[1])
                    node_ID = line_split[2]
                    node_list.append((node_number, node_ID))
                    if "Athens" in node_ID:
                        node_ID = "Athens, Greece"
                    if "Sofia" in node_ID:
                        node_ID = "Sofia, Bulgaria"
                    node_lookup[node_number] = node_ID

                elif line_split[0] == "link":
                    link_node1 = int(line_split[1])
                    link_node2 = int(line_split[2])
                    link_ID = line_split[3]

                    ### Get coordinates of nodes
                    location_node1 = geolocator.geocode(node_lookup[link_node1])
                    location_node2 = geolocator.geocode(node_lookup[link_node2])


                    link_latency = None
                    if location_node1 is None or location_node2 is None or node_lookup[link_node1] in location_blacklist or node_lookup[link_node2] in location_blacklist:
                        link_latency = 5
                        if verbose:
                            print(f"Unknown distance between {node_lookup[link_node1]} and {node_lookup[link_node2]}.")
                        latency_list.append({
                                            "link": f"{link_node1}:{link_node2}",
                                            "delayMus": "DEFAULT"
                                            })
                    else:
                        distance = calculate_distance((location_node1.latitude, location_node1.longitude), 
                                                        (location_node2.latitude, location_node2.longitude))
                        latency_ms = ((distance.km * 1000) / speed_of_light) * 1000
                        if verbose:
                            print(f"Distance/latency between {node_lookup[link_node1]} and {node_lookup[link_node2]}: {distance} | {latency_ms} ")
                        latency_list.append({
                                            "link": f"{link_node1}:{link_node2}",
                                            "delayMus": int(latency_ms * 1000)
                                            })
        with open(os.path.join(topology_folder, latency_lookup_dict[topology]), "w") as out_file:
            json.dump(latency_list, out_file, indent=2)

    final_latency_list = []
    for entry in latency_list:
        if entry["delayMus"] == "DEFAULT":
            entry["delayMus"] = default_latency
        final_latency_list.append(entry)

    return final_latency_list



if __name__ == '__main__':

    parser = ArgumentParser(description="ns-3 Topology Latency Enricher")
    parser.add_argument('--input_file', '-i',
                        dest="input_file",
                        action="store",
                        help="Which topology is to be enriched? Options: GEANT, CESNET, DT, G2",
                        required=True)
    args = parser.parse_args()

    determineRealLatencies(args.input_file, default_latency=1000,verbose=True)