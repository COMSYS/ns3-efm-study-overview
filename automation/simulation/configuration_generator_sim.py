import sys
import os
import copy
import json
from argparse import ArgumentParser
import random
from enrich_with_latency import determineRealLatencies
from enum import IntEnum, auto
from copy import deepcopy
import itertools
import math

main_folder = os.path.dirname(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
post_processing_path = os.path.join(main_folder, "raw-result-processing", "src", "post-processing")
sys.path.append(post_processing_path)

import ping_routes_extractor as pre

simulation_folder = os.path.dirname(os.path.realpath(__file__))
sim_configs_dir = os.path.join(simulation_folder, "configs-sim")
topology_folder = os.path.join(os.path.dirname(simulation_folder), "topologies")
topology_lookup_file = os.path.join(topology_folder, "topology-lookup.json")
meta_configs_folder = os.path.join(os.path.dirname(simulation_folder),'configs-meta')

ns3_parent_folder =os.path.dirname(os.path.dirname(simulation_folder))

class PlacementAlgo(IntEnum):
    NAIVE = auto()

def round_half_up(n, decimals=0):
    multiplier = 10**decimals
    return math.floor(n * multiplier + 0.5) / multiplier


def createPingScenarios(use_ping_param, ping_scenarios, topology, topology_links, topology_nodes):

    if not use_ping_param:
        return [{}]

    created_ping_scenarios = []
    global ping_path_cache
    selected_ping_flows = {}

    for ping_scenario in ping_scenarios:

        current_ping_scenario_configuration = {}

        if ping_scenario[0] == "ALL_TO_ALL":
            current_ping_scenario_configuration["pingScenario"] = "ALL_TO_ALL"
            current_ping_scenario_configuration["pingIntervalMs"] = ping_scenario[1]
            current_ping_scenario_configuration["pingPacketCount"] = ping_scenario[2]
            current_ping_scenario_configuration["pingPlacementScheme"] = "AUTOMATIC"
            current_ping_scenario_configuration["cSPingMatching"] = []
            current_ping_scenario_configuration["pingFlowsPerHost"] = len(topology_nodes)
            current_ping_scenario_configuration["pingFlowsOverall"] = len(topology_nodes) *  (len(topology_nodes) - 1)


        elif ping_scenario[0] == "SOME_TO_SOME":

            current_ping_scenario_configuration["pingScenario"] = "SOME_TO_SOME"
            current_ping_scenario_configuration["pingIntervalMs"] = ping_scenario[1]
            current_ping_scenario_configuration["pingPacketCount"] = ping_scenario[2]
            current_ping_scenario_configuration["pingPlacementScheme"] = "MANUAL"
            current_ping_scenario_configuration["pingFlowsPerHost"] = -1
            current_ping_scenario_configuration["cSPingMatching"] = []

            for index in range(3,len(ping_scenario)):
                current_ping_scenario_configuration["cSPingMatching"].append(ping_scenario[index])
            current_ping_scenario_configuration["pingFlowsOverall"] = len(current_ping_scenario_configuration["cSPingMatching"])

        elif ping_scenario[0] == "ALL_TO_ANY":

            current_ping_scenario_configuration["pingIntervalMs"] = ping_scenario[1]
            current_ping_scenario_configuration["pingPacketCount"] = ping_scenario[2]
            current_ping_scenario_configuration["cSPingMatching"] = []

            if ping_scenario[3] == "random":
                current_ping_scenario_configuration["pingScenario"] = "SOME_TO_SOME"
                for source_node in topology_nodes:
                    target_set = topology_nodes.copy()
                    target_set.remove(source_node)
                    for _ in range(ping_scenario[4]):
                        target_node = random.choice(target_set)
                        while f"{source_node}:{target_node}" in current_ping_scenario_configuration["cSPingMatching"]:
                            target_node = random.choice(target_set)
                        current_ping_scenario_configuration["cSPingMatching"].append(f"{source_node}:{target_node}")
                current_ping_scenario_configuration["pingFlowsPerHost"] = ping_scenario[4]
                current_ping_scenario_configuration["pingPlacementScheme"] = "RANDOM"
                current_ping_scenario_configuration["pingFlowsOverall"] = len(current_ping_scenario_configuration["cSPingMatching"])
            else:
                raise NotImplementedError("ping ALL_TO_ANY only supports 'random'")


        else:

            ping_route_test_scenario_map = {
                "GEANT": "sim_pingroutes_config_0.json",
                "GEANT2": "sim_pingroutes_config_1.json",
                "CESNET": "sim_pingroutes_config_2.json",
                "TELEKOM_GERMANY": "sim_pingroutes_config_3.json"
            }

            ping_route_result_name_map = {
                "GEANT": "PINGROUTE_0",
                "GEANT2": "PINGROUTE_1",
                "CESNET": "PINGROUTE_2",
                "TELEKOM_GERMANY": "PINGROUTE_3"
            }

            meta_config_name = "PINGROUTE-Sim.json"
            meta_sim_configuration = None
            try:
                with open (os.path.join(meta_configs_folder,meta_config_name), "r") as input_file:
                    meta_sim_configuration = json.load(input_file)
            except FileNotFoundError:
                print("For this to work, you need to have built a sim config with 'generation-config-sim-PING-ROUTES.json' and executed the corresponding simulation.")
                raise FileNotFoundError

            ping_routes = None
            if topology not in ping_path_cache.keys():
                ping_path_cache[topology] = pre.extract_ping_routes(
                    simulation_results_path=meta_sim_configuration["sim_output_dir"],
                    topology=ping_route_test_scenario_map[topology],
                    pingroute_result_name=ping_route_result_name_map[topology],
                    experiment_identifier=meta_sim_configuration["experiment_identifier"])
            ping_routes = ping_path_cache[topology]


            current_ping_scenario_configuration["pingScenario"] = "SOME_TO_SOME"
            current_ping_scenario_configuration["pingIntervalMs"] = ping_scenario[1]
            current_ping_scenario_configuration["pingPacketCount"] = ping_scenario[2]


            ## Set up cache for topology
            if topology not in selected_ping_flows.keys():
                selected_ping_flows[topology] = {}

            ## We need to compute the ping flow selection
            if ping_scenario[0] not in selected_ping_flows[topology].keys():

                if PlacementAlgo[ping_scenario[0]] == PlacementAlgo.NAIVE:
                    ### For this, we need information on the passed links of a flow going from each point to another
                    covered_links = {entry: False for entry in topology_links}
                    chosen_ping_flows = []
                    for ping_route in ping_routes.keys():
                        
                        ### Iterate over all nodes contained in the ping route
                        for id in range(len(ping_routes[ping_route])-1):

                            current_link_1 = None
                            current_link_2 = None
                            if ping_routes[ping_route][id] < ping_routes[ping_route][id+1]:
                                current_link_1 = (ping_routes[ping_route][id], ping_routes[ping_route][id+1])
                                current_link_2 = (ping_routes[ping_route][id+1], ping_routes[ping_route][id])
                            else:
                                current_link_2 = (ping_routes[ping_route][id], ping_routes[ping_route][id+1])
                                current_link_1 = (ping_routes[ping_route][id+1], ping_routes[ping_route][id])

                            if current_link_1 not in covered_links.keys() and current_link_2 not in covered_links.keys():
                                raise ValueError(f"Keys ({current_link_1}, {current_link_2}) not in link set")

                            ### Only add the direction that is also contained in covered_links
                            if current_link_1 in covered_links.keys():
                                if covered_links[current_link_1] is False:
                                    covered_links[current_link_1] = True
                                    if ping_route not in chosen_ping_flows:
                                        chosen_ping_flows.append(ping_route)
                            elif current_link_2 in covered_links.keys():
                                if covered_links[current_link_2] is False:
                                    covered_links[current_link_2] = True
                                    if ping_route not in chosen_ping_flows:
                                        chosen_ping_flows.append(ping_route)

                    if len(topology_links) != len([val for val in covered_links.keys() if covered_links[val] == True]):
                        raise RuntimeWarning(f"Not all links covered by ping flows: {[val for val in covered_links.keys() if covered_links[val] == False]}")
                
            
                    
                    final_ping_flows = []
                    for chosen_ping_flow in chosen_ping_flows:
                        final_ping_flows.append(f"{chosen_ping_flow.split('/')[0]}:{chosen_ping_flow.split('/')[1]}")

                    selected_ping_flows[topology][ping_scenario[0]] = final_ping_flows
                  
                else:
                    raise NotImplementedError("Currently, we only support naive ping flows.")


            current_ping_scenario_configuration["pingPlacementScheme"] = ping_scenario[0]
            current_ping_scenario_configuration["cSPingMatching"] = selected_ping_flows[topology][ping_scenario[0]]
            current_ping_scenario_configuration["pingFlowsPerHost"] = -1
            current_ping_scenario_configuration["pingFlowsOverall"] = len(current_ping_scenario_configuration["cSPingMatching"])


        created_ping_scenarios.append(current_ping_scenario_configuration)
    return created_ping_scenarios

def createNormalTrafficScenarios(traffic_scenarios, server_max_bytes, different_traffic_combinations_param, topology_nodes):
    created_normal_traffic_scenarios = []
    for _ in range(different_traffic_combinations_param):
        for server_bytes in server_max_bytes:
            for traffic_scenario in traffic_scenarios:

                current_traffic_scenario = {}

                current_traffic_scenario["serverTPFinalPktRcv"] = {"maxBytes": server_bytes}

                if traffic_scenario[0] == "ALL_TO_ALL":
                    current_traffic_scenario["trafficScenario"] = "ALL_TO_ALL"
                    current_traffic_scenario["trafficFlowsPerHost"] = len(topology_nodes)
                    current_traffic_scenario["trafficFlowsOverall"] = len(topology_nodes) * (len(topology_nodes) - 1)

                elif traffic_scenario[0] == "ALL_TO_ANY":
                    current_traffic_scenario["trafficScenario"] = "ALL_TO_ANY"
                    current_traffic_scenario["cSMatching"] = []
                    current_traffic_scenario["trafficScenarioRatio"] = 1

                    if len(traffic_scenario) > 2:
                        if traffic_scenario[1] == "exact":
                            for index in range(2,len(traffic_scenario)):
                                current_traffic_scenario["cSMatching"].append(traffic_scenario[index])
                            current_traffic_scenario["trafficFlowsPerHost"] = -1
                            current_traffic_scenario["trafficFlowsOverall"] = len(current_traffic_scenario["cSMatching"])

                        elif traffic_scenario[1] == "random":
                            current_traffic_scenario["trafficScenario"] = "SOME_TO_SOME"
                            for source_node in topology_nodes:
                                target_set = topology_nodes.copy()
                                target_set.remove(source_node)
                                for _ in range(traffic_scenario[2]):
                                    target_node = random.choice(target_set)
                                    while f"{source_node}:{target_node}" in current_traffic_scenario["cSMatching"]:
                                        target_node = random.choice(target_set)
                                    current_traffic_scenario["cSMatching"].append(f"{source_node}:{target_node}")

                            current_traffic_scenario["trafficScenarioRatio"] = traffic_scenario[2]
                            current_traffic_scenario["trafficFlowsPerHost"] = traffic_scenario[2]
                            current_traffic_scenario["trafficFlowsOverall"] = len(current_traffic_scenario["cSMatching"])

                elif traffic_scenario[0] == "SOME_TO_SOME":
                    current_traffic_scenario["trafficScenario"] = "SOME_TO_SOME"
                    current_traffic_scenario["cSMatching"] = []

                    if len(traffic_scenario) < 2:
                        print("Traffic Scenario SOME_TO_SOME requires some cSMatching mapping")
                        exit(-1)

                    for index in range(1,len(traffic_scenario)):
                        current_traffic_scenario["cSMatching"].append(traffic_scenario[index])
                    current_traffic_scenario["trafficFlowsPerHost"] = -1
                    current_traffic_scenario["trafficFlowsOverall"] = len(current_traffic_scenario["cSMatching"])

                created_normal_traffic_scenarios.append(current_traffic_scenario)
    print(f"Created {len(created_normal_traffic_scenarios)} traffic scenarios")
    return created_normal_traffic_scenarios



def createFailedLinkPlacementScenarios(use_failed_links_param, dynamic_failed_links_param, failed_links_param, 
                                different_combinations_param,
                                number_of_failed_links_param,
                                link_loss_rates_param,
                                use_loss_link_param,
                                delay_ms_param,
                                use_delay_link_param,
                                topology_links):

    ### We do not use any failed link
    if not use_failed_links_param:
        return [[]]

    ### We do not use dynamic failed links, i.e., only the ones configured manually
    if not dynamic_failed_links_param:
        return failed_links_param

    if type(number_of_failed_links_param) != list:
        raise TypeError("Please rewrite your config so that number of failed links is a list")

    if use_loss_link_param and use_delay_link_param:
        raise NotImplementedError("We currently do not support a link to have loss and delay at the same time.")

    if type(link_loss_rates_param) != list or type(delay_ms_param) != list :
        raise NotImplementedError("We do no longer allow to have 'link_loss_rate'/'delayMs' as single values, but instead require them to be lists.")


    created_failed_link_placements = []
    ### Double check
    if dynamic_failed_links_param:

        ### How many links should be broken?
        for number_of_failed_links in number_of_failed_links_param:

            ### How many error scenarios do we want to produce in this setting?
            for _ in range(different_combinations_param):

                used_failed_links = []
                used_failed_links_id = []

                ### Pick the links that are supposed to be broken
                for _ in range(number_of_failed_links):
                    
                    ## Random pick from link set
                    selected_link_index = random.randrange(0,len(topology_links))
                    while f"{topology_links[selected_link_index][0]}:{topology_links[selected_link_index][1]}" in used_failed_links_id:
                        selected_link_index = random.randrange(0,len(topology_links))
                    link_dict = {"link":  f"{topology_links[selected_link_index][0]}:{topology_links[selected_link_index][1]}"}
                    used_failed_links.append(link_dict)
                    used_failed_links_id.append(f"{topology_links[selected_link_index][0]}:{topology_links[selected_link_index][1]}")           

                created_failed_link_placements.append(used_failed_links)
    
        created_failed_link_placements_with_parameterization = []
        if use_loss_link_param:
            for failed_link_placement in created_failed_link_placements:
                for link_loss_rate in link_loss_rates_param:
                    
                    failed_link_placement_with_params = []
                    for link_dict in deepcopy(failed_link_placement):
                        link_dict["lossRate"] = link_loss_rate
                        failed_link_placement_with_params.append(link_dict)
                    created_failed_link_placements_with_parameterization.append(failed_link_placement_with_params)

        elif use_delay_link_param:
            for failed_link_placement in created_failed_link_placements:
                for link_delay in delay_ms_param:
                    
                    failed_link_placement_with_params = []
                    for link_dict in deepcopy(failed_link_placement):
                        link_dict["delayMs"] = link_delay
                        failed_link_placement_with_params.append(link_dict)

                    created_failed_link_placements_with_parameterization.append(failed_link_placement_with_params)
        else:
            raise NotImplementedError("This mode is not supported")

        return created_failed_link_placements_with_parameterization

    raise NotImplementedError("This should not happen")



if __name__ == '__main__':

    parser = ArgumentParser(description="ns-3 Configuration Generator")
    parser.add_argument('--meta_config', '-m',
                        dest="meta_config",
                        action="store",
                        help="Which meta config to use for generating the configs?",
                        default="generation-config-sim-default.json")
    parser.add_argument('--test', '-t',
                        dest="test_only",
                        action="store",
                        help="If this is only a test -> do not load some of the things",
                        default=False)
    args = parser.parse_args()

    print(f"We use the following meta config: {os.path.join(simulation_folder,args.meta_config)}")
    print(simulation_folder)
    jsonfile = open(os.path.join(simulation_folder,args.meta_config))
    meta_config = json.load(jsonfile)
    jsonfile.close()

    try:
        default_config = os.path.join(ns3_parent_folder, "ns-3-dev-fork", "scratch", "EFM-Localization-Stuff", meta_config["default_config_relative_in_scratch"])
        print(f"Use the following ns-3 default config: {default_config}")
        jsonfile = open(default_config)
        default_config = json.load(jsonfile)
        jsonfile.close()
    except FileNotFoundError as e:
        if not args.test_only:
            raise e
        else:
            default_config = {}

    if not meta_config["fix_traffic_scenarios"] and not meta_config["fix_failed_link_scenarios"]:
        raise NotImplementedError("Either 'fix_traffic_scenarios' or 'fix_failed_link_scenarios' must be fixed ...")

    elif meta_config["fix_traffic_scenarios"] and meta_config["fix_failed_link_scenarios"]:
        raise NotImplementedError("Cannot fix both 'fix_traffic_scenarios' AND 'fix_failed_link_scenarios' ...")


    counter = 0
    ping_path_cache = {}

    for topology in meta_config["desired_topologies"]:

        used_config = deepcopy(default_config)
        used_config["topology"] = topology

        """
        DETERMINE LATENCY PROFILE
        """
        if meta_config["bblOverride_useRealDelay"] and topology == "GEANT2":
            print("Insert real delay for GEANT2")
            used_config["bblOverride"] = determineRealLatencies(topology, meta_config["bblOverride_defaultLatency_Mus"])
        elif meta_config["bblOverride_activate"] and len(meta_config["bblOverride"][topology]):
            print(f"Copy bbl override as given into the simulation config for {topology}")
            used_config["bblOverride"] = meta_config["bblOverride"][topology]
        else:
            print(f"Disable bbl Override for {topology}")
            used_config["bblOverride"] = []

        if "bblDelayMus" in meta_config.keys():
            used_config["bblDelayMus"] = meta_config["bblDelayMus"]
        if "ehlDelayMus" in meta_config.keys():
            used_config["ehlDelayMus"] = meta_config["ehlDelayMus"]

        if "bblDataRate" in meta_config.keys():
            used_config["bblDataRate"] = meta_config["bblDataRate"]
        if "ehlDataRate" in meta_config.keys():
            used_config["ehlDataRate"] = meta_config["ehlDataRate"]

        if "bblQueueSize" in meta_config.keys():
            used_config["bblQueueSize"] = meta_config["bblQueueSize"]
        if "ehlQueueSize" in meta_config.keys():
            used_config["ehlQueueSize"] = meta_config["ehlQueueSize"]


        if "simStopMs" in meta_config.keys():
            used_config["simStopMs"] = meta_config["simStopMs"]
        if "ehSStopMs" in meta_config.keys():
            used_config["ehSStopMs"] = meta_config["ehSStopMs"]

        
        ### Extract topology information to enable a dynamic selection of broken link 
        topology_file_mapping = None
        with open(topology_lookup_file, "r") as topology_lookup:
            topology_file_mapping = json.load(topology_lookup)

        topology_file = os.path.join(topology_folder, topology_file_mapping[topology])
        topology_specification = None
        topology_nodes = []
        topology_links = []
        with open(topology_file, "r") as topology_lookup:
            ## Iterate through topology file
            add_nodes = True

            for line in topology_lookup.readlines():
                line_split = line.split(",")
                if line_split[0] == "node":
                    topology_nodes.append(int(line_split[1]))

                elif line_split[0] == "link":
                    topology_links.append((int(line_split[1]), int(line_split[2])))


        used_rng_seeds = []
        ### Iterate through traffic scenarios first and THEN pick different erros
        if meta_config["fix_traffic_scenarios"]:

            for ping_scenario, traffic_scenario in itertools.product(createPingScenarios(meta_config["enablePing"], meta_config["ping_scenarios"], topology, topology_links, topology_nodes), 
                                                                        createNormalTrafficScenarios(meta_config["traffic_scenarios"], meta_config["server_max_bytes"],
                                                                                                        meta_config["different_traffic_combinations"], topology_nodes)):

                for failed_link_scenario in createFailedLinkPlacementScenarios(dynamic_failed_links_param=meta_config["dynamic_failed_links"], 
                                                                    failed_links_param=meta_config["failed_links"], 
                                                                    different_combinations_param=meta_config["different_link_combinations"],
                                                                    use_failed_links_param=meta_config["use_failed_links"],
                                                                    number_of_failed_links_param=meta_config["number_of_failed_links"],
                                                                    link_loss_rates_param=meta_config["link_loss_rate"],
                                                                    delay_ms_param=meta_config["delayMs"],
                                                                    use_delay_link_param=meta_config["use_delay_link"],
                                                                    use_loss_link_param=meta_config["use_loss_link"],
                                                                    topology_links=topology_links):
                    target_config = copy.deepcopy(used_config)

                    target_config["failedLinks"] = failed_link_scenario

                    if meta_config["enablePing"]:
                        target_config["enablePing"] = True
                        for key in ping_scenario.keys():
                            target_config[key] = ping_scenario[key]
                    else:
                        target_config["enablePing"] = False

                    for key in traffic_scenario.keys():
                        target_config[key] = traffic_scenario[key]

                    if meta_config["randomize_rng"]:
                        current_rng_choice = random.randrange(1, pow(10,9))
                        while current_rng_choice in used_rng_seeds:
                            current_rng_choice = random.randrange(1, pow(10,9))
                        target_config["rngSeed"] = current_rng_choice

                    with open(os.path.join(sim_configs_dir, meta_config["output_file_prefix"] + f"_{counter}.json"), "w") as outFile:
                        outFile.write(json.dumps(target_config, indent=2))
                    counter += 1


                    if counter % 100 == 0:
                        print(f"Created {counter} configurations already.")


        ### Iterate through error scenarios first and THEN pick traffic scenarios
        elif meta_config["fix_failed_link_scenarios"]:

            for failed_link_scenario in createFailedLinkPlacementScenarios(dynamic_failed_links_param=meta_config["dynamic_failed_links"], 
                                                                    failed_links_param=meta_config["failed_links"], 
                                                                    different_combinations_param=meta_config["different_link_combinations"],
                                                                    use_failed_links_param=meta_config["use_failed_links"],
                                                                    number_of_failed_links_param=meta_config["number_of_failed_links"],
                                                                    link_loss_rates_param=meta_config["link_loss_rate"],
                                                                    delay_ms_param=meta_config["delayMs"],
                                                                    use_delay_link_param=meta_config["use_delay_link"],
                                                                    use_loss_link_param=meta_config["use_loss_link"],
                                                                    topology_links=topology_links):
                for ping_scenario, traffic_scenario in itertools.product(createPingScenarios(meta_config["enablePing"], meta_config["ping_scenarios"], topology, topology_links, topology_nodes), 
                                                                            createNormalTrafficScenarios(meta_config["traffic_scenarios"], meta_config["server_max_bytes"],
                                                                                                        meta_config["different_traffic_combinations"], topology_nodes)):

                    target_config = copy.deepcopy(used_config)

                    target_config["failedLinks"] = failed_link_scenario

                    if meta_config["enablePing"]:
                        target_config["enablePing"] = True
                        for key in ping_scenario.keys():
                            target_config[key] = ping_scenario[key]
                    else:
                        target_config["enablePing"] = False

                    for key in traffic_scenario.keys():
                        target_config[key] = traffic_scenario[key]

                    if meta_config["randomize_rng"]:
                        current_rng_choice = random.randrange(1, pow(10,9))
                        while current_rng_choice in used_rng_seeds:
                            current_rng_choice = random.randrange(1, pow(10,9))
                        target_config["rngSeed"] = current_rng_choice

                    with open(os.path.join(sim_configs_dir, meta_config["output_file_prefix"] + f"_{counter}.json"), "w") as outFile:
                        outFile.write(json.dumps(target_config, indent=2))
                    counter += 1


                    if counter % 100 == 0:
                        print(f"Created {counter} configurations already.")


    print(f"Created {counter} configurations overall.")
    ### Create a new meta file for the simulation based on the default meta file

    jsonfile_path = os.path.join(meta_configs_folder, meta_config["meta_template_file_name"])
    print(f"We base the simulation meta configuration on the following file: {jsonfile_path}")
    jsonfile = open(jsonfile_path)
    meta_simulation_config = json.load(jsonfile)
    jsonfile.close()


    meta_simulation_config["which_config_files"] = []
    for iterator in range(counter):

        meta_simulation_config["which_config_files"].append([meta_config["output_file_prefix"] + f"_{iterator}.json", meta_config["output_path_prefix"] + f"_{iterator}"])
    meta_simulation_config["experiment_identifier"] = meta_config["experiment_identifier"]

    if "sim_run_count" in meta_config:
        meta_simulation_config["sim_run_count"] = meta_config["sim_run_count"]

    if "num_pool_workers" in meta_config:
        meta_simulation_config["num_pool_workers"] = meta_config["num_pool_workers"]


    if "simStopMs" in meta_config.keys():
        meta_simulation_config["simStopMs"] = meta_config["simStopMs"]
    if "ehSStopMs" in meta_config.keys():
        meta_simulation_config["ehSStopMs"] = meta_config["ehSStopMs"]


    with open(os.path.join(meta_configs_folder, meta_config["meta_outputfile_name"]), "w") as outFile:
        outFile.write(json.dumps(meta_simulation_config, indent=2))

    
    print(f"Metafile for use is written to {os.path.join(meta_configs_folder, meta_config['meta_outputfile_name'])}")
    print(f"Execute simulation using: python3 run-simulations.py --meta_config {meta_config['meta_outputfile_name']}")