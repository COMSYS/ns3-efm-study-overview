import sys
import os
import copy
import json
from argparse import ArgumentParser
from collections import defaultdict

analysis_folder = os.path.dirname(os.path.realpath(__file__))
automation_folder = os.path.dirname(analysis_folder)
analysis_configs_dir = os.path.join(analysis_folder, "configs-analysis")
meta_configs_folder = os.path.join(automation_folder,'configs-meta')
simulation_configs_dir = os.path.join(automation_folder, 'simulation', 'configs-sim')
observer_placement_generator_dir = os.path.join(os.path.dirname(automation_folder), 'observer-placement-generator')

ns3_parent_folder = os.path.dirname(os.path.dirname(analysis_folder))

sys.path.append(observer_placement_generator_dir)
from op_manager import ObserverPlacementManager
from op_generators import ObserverPlacementAlgorithm, MeasurementType

def get_topology_simoutputprefix_mapping(meta_simulation_config: dict) -> dict:
    top_config = defaultdict(list)
    for mapping in meta_simulation_config["which_config_files"]:
        sim_config_file = os.path.join(simulation_configs_dir, mapping[0])
        if not os.path.exists(sim_config_file):
            print(f"Simulation config {sim_config_file} specified in mapping of sim-meta-config file does not exist. Abort.")
            exit(-1)
        with open(sim_config_file) as jsonfile:
            sim_config = json.load(jsonfile)
        top_config[sim_config["topology"]].append(mapping[1])
    return top_config

def translate_efm_bit_set_for_plc_gen(efm_bit_set: list) -> set:
    measurement_types = set()
    for efm_bit in efm_bit_set:
        if efm_bit == "Q":
            measurement_types.add(MeasurementType.EFM_Q)
        elif efm_bit == "L":
            measurement_types.add(MeasurementType.EFM_L)
        elif efm_bit == "R":
            measurement_types.add(MeasurementType.EFM_R)
        elif efm_bit == "T":
            measurement_types.add(MeasurementType.EFM_T_SPIN)
        elif efm_bit == "SPIN":
            measurement_types.add(MeasurementType.EFM_T_SPIN)
        elif efm_bit == "QR":
            measurement_types.add(MeasurementType.EFM_QR)
        elif efm_bit == "QL":
            measurement_types.add(MeasurementType.EFM_QL)
        elif efm_bit == "QT":
            measurement_types.add(MeasurementType.EFM_QT)
        elif efm_bit == "LT":
            measurement_types.add(MeasurementType.EFM_LT)
        elif efm_bit == "SEQ":
            measurement_types.add(MeasurementType.EFM_Q)
        elif efm_bit == "TCPRO":
            measurement_types.add(MeasurementType.EFM_L)
        elif efm_bit == "TCPDART":
            measurement_types.add(MeasurementType.EFM_T_SPIN)
        else:
            raise ValueError(f"Unknown EFM bit {efm_bit}")
    return measurement_types

def handle_meta_observer_set(observer_set, topology, efm_bit_sets, observer_plc_manager: ObserverPlacementManager):
    if observer_set[0] == "explicit":
        return observer_set[1]
    elif observer_set[0] == "generated":
        expected_config_keys = ["algorithm", "matchEfmBits", "edgeLinks"]
        generated_sets = []
        for generation_config in observer_set[1]:
            print(generation_config)
            for key in expected_config_keys:
                if key not in generation_config:
                    raise ValueError(f"Key {key} not found in observer set generation config {generation_config}")
            
            if generation_config["matchEfmBits"]:
                measurement_type_sets = [translate_efm_bit_set_for_plc_gen(efm_bit_set) for efm_bit_set in efm_bit_sets]
            else:
                if "efmBitSets" not in generation_config:
                    raise ValueError(f"Key efmBitSets not found in observer set generation config {generation_config} and matchEfmBits is false")
                measurement_type_sets = [translate_efm_bit_set_for_plc_gen(efm_bit_set) for efm_bit_set in generation_config["efmBitSets"]]
            
            for measurement_types in measurement_type_sets:
                placement_data, _ = observer_plc_manager.get_or_generate_placement(topology, measurement_types, ObserverPlacementAlgorithm[generation_config["algorithm"]], generation_config["edgeLinks"])
                generated_set = dict()
                generated_set["observers"] = list(placement_data.placement)
                generated_set["metadata"] = {"algorithm": placement_data.algorithm, "edge_links": placement_data.edge_links, "exists":  placement_data.exists, "measurement_types": list(map(str, placement_data.measurement_type))}
                generated_sets.append(generated_set)
        return generated_sets
    else:
        raise ValueError(f"Unknown observer set type {observer_set[0]}")

def generate_analysis_config_files(meta_config, base_config, topology, observer_plc_manager, file_index_start=0):
    counter = 0
    analysis_configs = []
    for lossRateTh in meta_config["lossRateTh"]:
        for delayThMs in meta_config["delayThMs"]:
            for classificationModes in meta_config["classificationModes"]:
                for simFilter in meta_config["simFilter"]:
                    for localizationMethods in meta_config["localizationMethods"]:
                        for flowSelectionStrategies in meta_config["flowSelectionStrategies"]:
                            for observerSets in meta_config["observerSets"]:
                                for efmBitSets in meta_config["efmBitSets"]:

                                    actual_config = copy.deepcopy(base_config)
                                    actual_config["storeMeasurements"] =  meta_config["storeMeasurements"]
                                    actual_config["performLocalization"] = meta_config["performLocalization"]
                                    actual_config["output_raw_values"] = meta_config["output_raw_values"]
                                    actual_config["classification_base_id"] = meta_config["classification_base_id"] + f"_{str(counter)}"
                                    actual_config["lossRateTh"] = lossRateTh
                                    actual_config["delayThMs"] = delayThMs
                                    actual_config["classificationModes"] = classificationModes
                                    actual_config["simFilter"] = simFilter
                                    actual_config["localizationMethods"] = localizationMethods
                                    actual_config["flowSelectionStrategies"] = flowSelectionStrategies
                                    actual_config["observerSets"] = handle_meta_observer_set(observerSets, topology, efmBitSets, observer_plc_manager)
                                    actual_config["efmBitSets"] = efmBitSets

                                    analysis_configs.append(actual_config)
                                    counter += 1

    analysis_config_file_name = meta_config["output_file_prefix"] + f"_{file_index_start}.json"
    with open(os.path.join(analysis_configs_dir, analysis_config_file_name), "w") as outFile:
        outFile.write(json.dumps(analysis_configs, indent=2))

    return [analysis_config_file_name]



def main():
    parser = ArgumentParser(description="ns-3 Configuration Generator for Analysis")
    parser.add_argument('--meta_config', '-m',
                        dest="meta_config",
                        action="store",
                        help="Which meta config to use for generating the configs?",
                        default="generation-config-analysis-default.json")
    args = parser.parse_args()

    print(f"Use the following meta config: {os.path.join(analysis_folder,args.meta_config)}")
    jsonfile = open(os.path.join(analysis_folder,args.meta_config))
    meta_config = json.load(jsonfile)
    jsonfile.close()

    default_config = os.path.join(ns3_parent_folder, "raw-result-processing", "data", meta_config["default_config_relative_result_processing_data"])
    print(f"Use default configuration parameters as defined in: {default_config}")
    jsonfile = open(default_config)
    used_config = json.load(jsonfile)
    jsonfile.close()

    jsonfile_path = os.path.join(meta_configs_folder, meta_config["simulation_meta_file_name"])
    print(f"This analysis complements the following simulation meta file: {jsonfile_path}")
    jsonfile = open(jsonfile_path)
    meta_simulation_config = json.load(jsonfile)
    jsonfile.close()

    topology_simoutputprefix_mapping = get_topology_simoutputprefix_mapping(meta_simulation_config)

    observer_plc_manager = ObserverPlacementManager()

    analysis_config_counter = 0
    analysis_config_file_names_per_top = dict()

    for topology in topology_simoutputprefix_mapping:
        base_config = used_config[0]
        base_config["time_filter_ms"] = meta_simulation_config["ehSStopMs"]
        configs = generate_analysis_config_files(meta_config, used_config[0], topology, observer_plc_manager, analysis_config_counter)
        analysis_config_counter += len(configs)
        analysis_config_file_names_per_top[topology] = configs


    default_meta_file_name = 'config-meta-analysis-default.json'
    if "default_meta_config_file" in meta_config.keys():
        default_meta_file_name = meta_config["default_meta_config_file"]

    jsonfile_path = os.path.join(meta_configs_folder, default_meta_file_name)
    print(f"Base the analysis meta config on the following file: {jsonfile_path}")
    jsonfile = open(jsonfile_path)
    meta_analysis_config = json.load(jsonfile)
    jsonfile.close()



    meta_analysis_config["which_config_files"] = []
    analysis_config_index = 0
    for topology in topology_simoutputprefix_mapping:
        for analysis_config_file_name in analysis_config_file_names_per_top[topology]:
            for sim_output_prefix in topology_simoutputprefix_mapping[topology]:
                for sim_config_name, prefix in meta_simulation_config["which_config_files"]:        
                    if sim_output_prefix == prefix:
                        meta_analysis_config["which_config_files"].append([analysis_config_file_name, sim_config_name, meta_config["analysis_folder_prefix"] + f"_{analysis_config_index}", sim_output_prefix])
            analysis_config_index += 1
    assert analysis_config_counter == analysis_config_index

    meta_analysis_config["sim_run_count"] = meta_simulation_config["sim_run_count"]
    meta_analysis_config["sim_run_offset"] = meta_simulation_config["sim_run_offset"]
    meta_analysis_config["experiment_identifier"] = meta_config["experiment_identifier"]
    meta_analysis_config["confirm_overwrite"] = meta_config["confirm_overwrite"]

    if "num_pool_workers" in meta_config.keys():
        meta_analysis_config["num_pool_workers"] =  meta_config["num_pool_workers"]

    with open(os.path.join(meta_configs_folder, meta_config["analysis_meta_outputfile_name"]), "w") as outFile:
        outFile.write(json.dumps(meta_analysis_config, indent=2))

    print(f"Execute analysis using: python3 run-analyses.py --meta_config {meta_config['analysis_meta_outputfile_name']}")

if __name__ == '__main__':
    main()
