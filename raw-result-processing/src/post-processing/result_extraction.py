from textwrap import wrap
import copy

from data_manager import (
    SimRunResultSet,
    LocalizationMethod,
    EfmBit,
    ClassificationMode,
)
import data_manager as dm
import run_processing as rp

import pandas as pd
from enum import IntEnum

def is_loss_bit(bit):
    return bit in [EfmBit.L, 
                    EfmBit.Q, 
                    EfmBit.R, 
                    EfmBit.QR, 
                    EfmBit.QL, 
                    EfmBit.TCPRO,
                    EfmBit.SEQ,
                    EfmBit.PINGLSS]

bits_lookup = {
    EfmBit.SPIN : "spin",
    EfmBit.L : "l",
    EfmBit.Q: "q",
    EfmBit.R: "r",
    EfmBit.QR : 'qr',
    EfmBit.QL: "ql",
    EfmBit.TCPDART: "dart",
    EfmBit.TCPRO: "routescout",
    EfmBit.SEQ: "seq",
    EfmBit.PINGLSS: "pl",
    EfmBit.PINGDLY: "pd"}
bits_reverse_lookup = {
    "l": EfmBit.L,
    "q": EfmBit.Q,
    "r": EfmBit.R,
    "qr": EfmBit.QR,
    "ql": EfmBit.QL,
    "spin" : EfmBit.SPIN,
    "dart": EfmBit.TCPDART,
    "routescout": EfmBit.TCPRO,
    "seq": EfmBit.SEQ,
    "pl": EfmBit.PINGLSS,
    "pd": EfmBit.PINGDLY
}

bits_lookup_short = {
    EfmBit.SPIN : "s",
    EfmBit.L : "l",
    EfmBit.Q: "q",
    EfmBit.R: "r",
    EfmBit.QR : 'qr',
    EfmBit.QL: "ql",
    EfmBit.TCPDART: "d",
    EfmBit.TCPRO: "rs",
    EfmBit.SEQ: "seq",
    EfmBit.PINGLSS: "pl",
    EfmBit.PINGDLY: "pd"}
bits_short_reverse_lookup = {
    "l": EfmBit.L,
    "q": EfmBit.Q,
    "r": EfmBit.R,
    "qr": EfmBit.QR,
    "ql": EfmBit.QL,
    "s" : EfmBit.SPIN,
    "d": EfmBit.TCPDART,
    "rs": EfmBit.TCPRO,
    "se": EfmBit.SEQ,
    "pl": EfmBit.PINGLSS,
    "pd": EfmBit.PINGDLY
}

algos_lookup = {
    LocalizationMethod.POSSIBLE : "poss",
    LocalizationMethod.PROBABLE : "prob",
    LocalizationMethod.WEIGHT_DIR: "wdir",
    LocalizationMethod.WEIGHT_DIR_LVL: "wdir_lvl",
    LocalizationMethod.WEIGHT_BAD: "wbad",
    LocalizationMethod.WEIGHT_BAD_LVL: "wbad_lvl",
    LocalizationMethod.WEIGHT_ITER: "witer",
    LocalizationMethod.DLC: "DLC",
    LocalizationMethod.DETECTION: "DETECTION",
    LocalizationMethod.LIN_LSQR: "LIN_LSQR",
    LocalizationMethod.LIN_LSQR_FIXED_FLOWS: "LIN_LSQR_FIXED_FLOWS",
    LocalizationMethod.LIN_LSQR_CORE_ONLY: "LIN_LSQR_CORE_ONLY",
    LocalizationMethod.LIN_LSQR_CORE_ONLY_FIXED_FLOWS: "LIN_LSQR_CORE_ONLY_FIXED_FLOWS",
    LocalizationMethod.FLOW_COMBINATION: "FLOW_COMBINATION",
    LocalizationMethod.FLOW_COMBINATION_FIXED_FLOWS: "FLOW_COMBINATION_FIXED_FLOWS"
}
algos_reverse_lookup = {
    "poss" : LocalizationMethod.POSSIBLE,
    "prob" : LocalizationMethod.PROBABLE,
    "wdir" : LocalizationMethod.WEIGHT_DIR,
    "wdir_lvl" : LocalizationMethod.WEIGHT_DIR_LVL,
    "wbad" : LocalizationMethod.WEIGHT_BAD,
    "wbad_lvl" : LocalizationMethod.WEIGHT_BAD_LVL,
    "witer" : LocalizationMethod.WEIGHT_ITER,
    "DLC" : LocalizationMethod.DLC,
    "DETECTION" : LocalizationMethod.DETECTION,
    "LIN_LSQR" : LocalizationMethod.LIN_LSQR,
    "FLOW_COMBINATION": LocalizationMethod.FLOW_COMBINATION
}

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


measurement_type_lookup = {
    MeasurementType.EFM_T_SPIN: EfmBit.SPIN,
    MeasurementType.EFM_L: EfmBit.L,
    MeasurementType.EFM_Q: EfmBit.Q,
    MeasurementType.EFM_R: EfmBit.R,
    MeasurementType.EFM_QR: EfmBit.QR,
    MeasurementType.EFM_QL: EfmBit.QL

}

classification_mode_lookup = {
    ClassificationMode.STATIC : "static",
    ClassificationMode.PERFECT : "perfect",
}    
classification_mode_reverse_lookup = {
    "static" : ClassificationMode.STATIC,
    "perfect" : ClassificationMode.PERFECT
}


def getRunSummaryDataFrame_For_LSQR(
    sim_result_sets: list[SimRunResultSet],
    use_configured_loss: bool = False,
    use_configured_delay: bool = False
) -> pd.DataFrame:
    """
    @param sim_result_sets: a list of SimRunResultSets. One SimRunResultSet is the outcome of running execute_simulation_pipeline ONCE.
    """

    print("Start creating dataframe")
    df_collected_results = pd.DataFrame()
    for sim_result_set in sim_result_sets:

        for run_iteration in sim_result_set.sim_run_results:

            number_of_failed_links = len(run_iteration.config["failedLinks"])
            failed_links_delay = failed_links_loss = 0
            failed_links_numbers = []
            if number_of_failed_links != 0:
                failed_links_delay = run_iteration.config["failedLinks"][0]["delayMs"]
                failed_links_loss = run_iteration.config["failedLinks"][0]["lossRate"]
                failed_links_numbers = [failed_link["link"] for failed_link in run_iteration.config["failedLinks"]]

            classified_lc_filtered_results = [
                (sim_loc_configuration.classification_config.classification_mode, sim_loc_configuration.classification_config.observer_set_metadata, sim_loc_configuration.flow_selection, rp.CategorizeLocResults(run_iteration, use_configured_loss, use_configured_delay)[sim_loc_configuration])
                for sim_loc_configuration in run_iteration.localization_res.keys()]
            
            index_values_orig = ["sim_id", "run_id", "run_nr", "topology", "trafficScenario", "trafficScenarioRatio", "trafficFlowsPerHost", "trafficFlowsOverall", "failed_link_count", "failed_link_delay", "failed_link_loss", "failed_links_identifiers"]
            values_values_orig = ["_".join(run_iteration.sim_id.split("_")[0:2]), run_iteration.sim_id.split("_")[3], run_iteration.sim_id.split("_")[2], run_iteration.config["topology"], run_iteration.config["trafficScenario"], run_iteration.config["trafficScenarioRatio"], run_iteration.config["trafficFlowsPerHost"], run_iteration.config["trafficFlowsOverall"], number_of_failed_links, failed_links_delay, failed_links_loss, failed_links_numbers]
            key_value_map_orig = {"sim_id": "_".join(run_iteration.sim_id.split("_")[0:2]), 
                             "run_id": run_iteration.sim_id.split("_")[3], 
                             "run_nr": run_iteration.sim_id.split("_")[2],
                             "topology": run_iteration.config["topology"], 
                             "trafficScenario": run_iteration.config["trafficScenario"],
                             "trafficScenarioRatio": run_iteration.config["trafficScenarioRatio"],
                             "trafficFlowsPerHost": run_iteration.config["trafficFlowsPerHost"],
                             "trafficFlowsOverall": run_iteration.config["trafficFlowsOverall"],
                             "failed_link_count": number_of_failed_links, 
                             "failed_link_delay": failed_links_delay, 
                             "failed_link_loss": failed_links_loss, 
                             "failed_links_identifiers": failed_links_numbers}
     
            if run_iteration.config["enablePing"]:
                    
                index_values_orig.append("pingIntervalMs")                    
                index_values_orig.append("pingPacketCount")                    
                index_values_orig.append("pingScenario")                    
                index_values_orig.append("pingScenarioRatio")                
                index_values_orig.append("pingPlacementScheme")                
                index_values_orig.append("pingFlowsPerHost")                
                index_values_orig.append("pingFlowsOverall")

                values_values_orig.append(run_iteration.config["pingIntervalMs"])
                values_values_orig.append(run_iteration.config["pingPacketCount"])
                values_values_orig.append(run_iteration.config["pingScenario"])
                values_values_orig.append(run_iteration.config["pingScenarioRatio"])
                values_values_orig.append(run_iteration.config["pingPlacementScheme"])
                values_values_orig.append(run_iteration.config["pingFlowsPerHost"])
                values_values_orig.append(run_iteration.config["pingFlowsOverall"])

                key_value_map_orig["pingIntervalMs"] = run_iteration.config["pingIntervalMs"]
                key_value_map_orig["pingPacketCount"] = run_iteration.config["pingPacketCount"]
                key_value_map_orig["pingScenario"] = run_iteration.config["pingScenario"]
                key_value_map_orig["pingScenarioRatio"] = run_iteration.config["pingScenarioRatio"]
                key_value_map_orig["pingPlacementScheme"] = run_iteration.config["pingPlacementScheme"]
                key_value_map_orig["pingFlowsPerHost"] = run_iteration.config["pingFlowsPerHost"]
                key_value_map_orig["pingFlowsOverall"] = run_iteration.config["pingFlowsOverall"]

            row_dicts = {}
            bit_counter_with = {}
            bit_counter_without = {}

            for bit in sorted(rp.GetUsedEFMBits(run_iteration)):

                for loc_alg_with_param in sorted(rp.GetUsedLocAlgosWithParam(run_iteration)):
                    loc_alg = loc_alg_with_param[0]
                    loc_params = loc_alg_with_param[1]

                    classified_fully_filtered_results = [
                        (classification_mode, observer_metadata, flow_selection, rp.FilterCategorizedLocResults(
                            res, method=loc_alg, efm_bits=dm.extendEfmBit(bit), params=loc_params
                        ))
                        for (classification_mode, observer_metadata, flow_selection, res) in classified_lc_filtered_results
                    ]
                    for class_mode, observer_metadata, flow_selection, res in classified_fully_filtered_results:

                        if len(res)==0:
                            continue

                        if len(res) != 1:
                            raise Exception("This list should have length 1.")

                        if class_mode == ClassificationMode.PERFECT or (res[0].localization_result.method != LocalizationMethod.LIN_LSQR and res[0].localization_result.method != LocalizationMethod.LIN_LSQR_CORE_ONLY and res[0].localization_result.method != LocalizationMethod.FLOW_COMBINATION and res[0].localization_result.method != LocalizationMethod.LIN_LSQR_FIXED_FLOWS and res[0].localization_result.method != LocalizationMethod.LIN_LSQR_CORE_ONLY_FIXED_FLOWS and res[0].localization_result.method != LocalizationMethod.FLOW_COMBINATION_FIXED_FLOWS):
                            continue

                        index_values = copy.deepcopy(index_values_orig)
                        values_values = copy.deepcopy(values_values_orig)
                        key_value_map = copy.deepcopy(key_value_map_orig) 

                        index_values.append("localizationMethod")
                        index_values.append("flowSelectionStrategy")
                        index_values.append("flowCount")
                        index_values.append("observerSelectionEfmBits")
                        index_values.append("observerSelectionAlgorithm")
                        values_values.append(res[0].localization_result.method)
                        values_values.append(flow_selection.selectionStrategy)
                        values_values.append(flow_selection.flow_count)
                        values_values.append([measurement_type_lookup[MeasurementType[current_measurement_type]] for current_measurement_type in observer_metadata['measurement_types'] ] if observer_metadata else "None")
                        values_values.append(ObserverPlacementAlgorithm(observer_metadata['algorithm']).name if observer_metadata else "All")
                        key_value_map["localizationMethod"] = res[0].localization_result.method
                        key_value_map["flowSelectionStrategy"] = flow_selection.selectionStrategy
                        key_value_map["flowCount"] = flow_selection.flow_count
                        key_value_map["observerSelectionEfmBits"] = [measurement_type_lookup[MeasurementType[current_measurement_type]] for current_measurement_type in observer_metadata['measurement_types'] ] if observer_metadata else "None"
                        key_value_map["observerSelectionAlgorithm"] = ObserverPlacementAlgorithm(observer_metadata['algorithm']).name if observer_metadata else "All"

                        row_id = "+".join([str(item) for item in values_values])

                        if row_id in row_dicts.keys():
                            index_values = row_dicts[row_id]["index_values"]
                            values_values = row_dicts[row_id]["values_values"]
                            key_value_map = row_dicts[row_id]["key_value_map"]

                            if bit not in bit_counter_with.keys():
                                bit_counter_with[bit] = 0
                            bit_counter_with[bit] += 1
                        else:
                            row_dicts[row_id] = {}
                            if bit not in bit_counter_without.keys():
                                bit_counter_without[bit] = 0
                            bit_counter_without[bit] += 1


                        """
                        Extract values for CDF independent of the actually used EFM bit
                        """

                        index_values.append(f"{bits_lookup[bit]}_abs_raw_for_cdf")
                        val = [val for _, val in res[0].link_rating_errors_abs.items()]
                        values_values.append(val)
                        key_value_map[f"{bits_lookup[bit]}_abs_raw_for_cdf"] = val

                        index_values.append(f"{bits_lookup[bit]}_abs_core_raw_for_cdf")
                        val = [val for key, val in res[0].link_rating_errors_abs.items() if key in run_iteration.core_links]
                        values_values.append(val)
                        key_value_map[f"{bits_lookup[bit]}_abs_core_raw_for_cdf"] = val

                        index_values.append(f"{bits_lookup[bit]}_abs_edge_raw_for_cdf")
                        val = [val for key, val in res[0].link_rating_errors_abs.items() if key in run_iteration.edge_links]
                        values_values.append(val)
                        key_value_map[f"{bits_lookup[bit]}_abs_edge_raw_for_cdf"] = val

                        index_values.append(f"{bits_lookup[bit]}_rel_raw_for_cdf")
                        val = [val for _, val in res[0].link_rating_errors_rel.items()]
                        values_values.append(val)
                        key_value_map[f"{bits_lookup[bit]}_rel_raw_for_cdf"] = val

                        index_values.append(f"{bits_lookup[bit]}_rel_core_raw_for_cdf")
                        val = [val for key, val in res[0].link_rating_errors_rel.items() if key in run_iteration.core_links]
                        values_values.append(val)
                        key_value_map[f"{bits_lookup[bit]}_rel_core_raw_for_cdf"] = val

                        index_values.append(f"{bits_lookup[bit]}_rel_edge_raw_for_cdf")
                        val = [val for key, val in res[0].link_rating_errors_rel.items() if key in run_iteration.edge_links]
                        values_values.append(val)
                        key_value_map[f"{bits_lookup[bit]}_rel_edge_raw_for_cdf"] = val

                        if is_loss_bit(bit):

                            links_with_actual_loss = [link for link in run_iteration.link_ground_truth.keys() if run_iteration.link_ground_truth[link].lost_packets > 0]
                            core_links_with_actual_loss = [link for link in run_iteration.link_ground_truth.keys() if run_iteration.link_ground_truth[link].lost_packets > 0 and link in run_iteration.core_links]
                            edge_links_with_actual_loss = [link for link in run_iteration.link_ground_truth.keys() if run_iteration.link_ground_truth[link].lost_packets > 0 and link in run_iteration.edge_links]

                            links_without_actual_loss = [link for link in run_iteration.link_ground_truth.keys() if run_iteration.link_ground_truth[link].lost_packets == 0]
                            core_links_without_actual_loss = [link for link in run_iteration.link_ground_truth.keys() if run_iteration.link_ground_truth[link].lost_packets == 0 and link in run_iteration.core_links]
                            edge_links_without_actual_loss = [link for link in run_iteration.link_ground_truth.keys() if run_iteration.link_ground_truth[link].lost_packets == 0 and link in run_iteration.edge_links]

                            if "links_with_actual_loss" not in key_value_map.keys():
                                index_values.append("links_with_actual_loss")
                                values_values.append(len(links_with_actual_loss))
                                key_value_map["links_with_actual_loss"] = len(links_with_actual_loss)

                                index_values.append("core_links_with_actual_loss")
                                values_values.append(len(core_links_with_actual_loss))
                                key_value_map["core_links_with_actual_loss"] = len(core_links_with_actual_loss)

                                index_values.append("edge_links_with_actual_loss")
                                values_values.append(len(edge_links_with_actual_loss))
                                key_value_map["edge_links_with_actual_loss"] = len(edge_links_with_actual_loss)


                                index_values.append("links_without_actual_loss")
                                values_values.append(len(links_without_actual_loss))
                                key_value_map["links_without_actual_loss"] = len(links_without_actual_loss)

                                index_values.append("core_links_without_actual_loss")
                                values_values.append(len(core_links_without_actual_loss))
                                key_value_map["core_links_without_actual_loss"] = len(core_links_without_actual_loss)

                                index_values.append("edge_links_without_actual_loss")
                                values_values.append(len(edge_links_without_actual_loss))
                                key_value_map["edge_links_without_actual_loss"] = len(edge_links_without_actual_loss)


                            index_values.append(f"{bits_lookup[bit]}_with_actual_loss_abs_for_cdf")
                            val = [res[0].link_rating_errors_abs[key] for key in links_with_actual_loss]
                            values_values.append(val)
                            key_value_map[f"{bits_lookup[bit]}_with_actual_loss_abs_for_cdf"] = val

                            index_values.append(f"{bits_lookup[bit]}_with_actual_loss_abs_core_for_cdf")
                            val = [res[0].link_rating_errors_abs[key] for key in core_links_with_actual_loss]
                            values_values.append(val)
                            key_value_map[f"{bits_lookup[bit]}_with_actual_loss_abs_core_for_cdf"] = val

                            index_values.append(f"{bits_lookup[bit]}_with_actual_loss_abs_edge_for_cdf")
                            val = [res[0].link_rating_errors_abs[key] for key in edge_links_with_actual_loss]
                            values_values.append(val)
                            key_value_map[f"{bits_lookup[bit]}_with_actual_loss_abs_edge_for_cdf"] = val


                            index_values.append(f"{bits_lookup[bit]}_with_actual_rel_for_cdf")
                            val = [res[0].link_rating_errors_rel[key] for key in links_with_actual_loss]
                            values_values.append(val)
                            key_value_map[f"{bits_lookup[bit]}_with_actual_rel_for_cdf"] = val

                            index_values.append(f"{bits_lookup[bit]}_with_actual_rel_core_for_cdf")
                            val = [res[0].link_rating_errors_rel[key] for key in core_links_with_actual_loss]
                            values_values.append(val)
                            key_value_map[f"{bits_lookup[bit]}_with_actual_rel_core_for_cdf"] = val

                            index_values.append(f"{bits_lookup[bit]}_with_actual_rel_edge_for_cdf")
                            val = [res[0].link_rating_errors_rel[key] for key in edge_links_with_actual_loss]
                            values_values.append(val)
                            key_value_map[f"{bits_lookup[bit]}_with_actual_rel_edge_for_cdf"] = val


                            index_values.append(f"{bits_lookup[bit]}_without_actual_loss_abs_for_cdf")
                            val = [res[0].link_rating_errors_abs[key] for key in links_without_actual_loss if key in res[0].link_rating_errors_abs.keys()]
                            values_values.append(val)
                            key_value_map[f"{bits_lookup[bit]}_without_actual_loss_abs_for_cdf"] = val

                            index_values.append(f"{bits_lookup[bit]}_without_actual_loss_abs_core_for_cdf")
                            val = [res[0].link_rating_errors_abs[key] for key in core_links_without_actual_loss]
                            values_values.append(val)
                            key_value_map[f"{bits_lookup[bit]}_without_actual_loss_abs_core_for_cdf"] = val

                            index_values.append(f"{bits_lookup[bit]}_without_actual_loss_abs_edge_for_cdf")
                            if res[0].localization_result.method != LocalizationMethod.LIN_LSQR_CORE_ONLY and res[0].localization_result.method != LocalizationMethod.FLOW_COMBINATION and res[0].localization_result.method != LocalizationMethod.LIN_LSQR_CORE_ONLY_FIXED_FLOWS and res[0].localization_result.method != LocalizationMethod.FLOW_COMBINATION_FIXED_FLOWS:
                                val = [res[0].link_rating_errors_abs[key] for key in edge_links_without_actual_loss]
                            else:
                                val = []
                            values_values.append(val)
                            key_value_map[f"{bits_lookup[bit]}_without_actual_loss_abs_edge_for_cdf"] = val

                        row_dicts[row_id]["index_values"] = index_values
                        row_dicts[row_id]["values_values"] = values_values
                        row_dicts[row_id]["key_value_map"] = key_value_map

            longest_row_id = -1
            longest_row_length = 0
            if df_collected_results.empty:
                for row_id in row_dicts:
                    if (len(row_dicts[row_id]["values_values"])) > longest_row_length:
                        longest_row_id = row_id
                        longest_row_length = len(row_dicts[row_id]["values_values"])
                df_collected_results = pd.DataFrame([row_dicts[longest_row_id]["key_value_map"]])


            for row_id in row_dicts:
                
                if row_id != longest_row_id:
                    
                    try:
                        df_collected_results.loc[len(df_collected_results)] = row_dicts[row_id]["values_values"]
                    except ValueError as e:
                        if str(e) == "cannot set a row with mismatched columns":

                            if LocalizationMethod.FLOW_COMBINATION in row_dicts[row_id]["values_values"] or LocalizationMethod.FLOW_COMBINATION_FIXED_FLOWS in row_dicts[row_id]["values_values"]:

                                new_value_list = []
                                for key in list(df_collected_results.columns):
                                    
                                    if key in row_dicts[row_id]["index_values"]:
                                        new_value_list.append(row_dicts[row_id]["values_values"][row_dicts[row_id]["index_values"].index(key)])
                                    else:
                                        if "for_cdf" in key:
                                            new_value_list.append([-42])
                                        elif "with_actual_loss" in key:
                                            new_value_list.append([-42])
                                        elif "without_actual_loss" in key:
                                            new_value_list.append([-42])
                                        else:
                                            raise Exception("Reconstructing LSQR dataframe fails.")

                                df_collected_results.loc[len(df_collected_results)] = new_value_list

                            else:
                                raise Exception("Mismatched columns when building LSQR dataframe.")

    print("Finished creating dataframe")
    return df_collected_results