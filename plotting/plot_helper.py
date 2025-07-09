import matplotlib as mpl
import scipy
import numpy as np
import math
import os
import json
import pandas as pd

mpl.use('agg')
COLORS = [u'#006ba5', u'#f26c64', u'#ffcc99', u'#88d279', u'#984ea3', u'#8c564b', u'#e377c2', u'#7f7f7f', u'#bcbd22', u'#17becf', u'#ffff99']

import data_manager as dm
import result_extraction as re

bit_plot_name_map = {
    dm.EfmBit.Q: "Q",
    dm.EfmBit.L: "L",
    dm.EfmBit.QR: "QR",
    dm.EfmBit.QL: "QL",
    dm.EfmBit.TCPRO: "RouteScout",
    dm.EfmBit.PINGLSS: "Ping",
    dm.EfmBit.SEQ: "SEQ",
    dm.EfmBit.R: "R",
    dm.EfmBit.SPIN: "Spin",
    dm.EfmBit.TCPDART: "Dart",
    dm.EfmBit.PINGDLY: "Ping"
}

bit_plot_color_map = {
    dm.EfmBit.Q: COLORS[1],
    dm.EfmBit.L: COLORS[2],
    dm.EfmBit.R: COLORS[3],
    dm.EfmBit.SPIN: COLORS[1],
    dm.EfmBit.TCPRO: COLORS[0],
    dm.EfmBit.TCPDART: COLORS[0],
    dm.EfmBit.PINGDLY: COLORS[5],
    dm.EfmBit.PINGLSS: COLORS[5],
    dm.EfmBit.QR: COLORS[6],
    dm.EfmBit.QL: COLORS[7],
    dm.EfmBit.SEQ: COLORS[8]
}

topology_line_type_map = {
    "TELEKOM_GERMANY": "solid",
    "GEANT2": "dashdot",
    "CESNET": "dotted"
}

topology_name_map = {
    "TELEKOM_GERMANY": "Telekom",
    "GEANT2": "GÉANT",
    "CESNET": "CESNET"
}

topology_color_map = {
    "TELEKOM_GERMANY":COLORS[0],
    "GEANT2": COLORS[1],
    "CESNET": COLORS[2]
}


def ci_df(df, confidence=0.99):
    return df.apply(ci, axis=0)


def ci(data, confidence=0.99):
    a = 1.0 * np.array(data)
    n = len(a)
    m, se = np.mean(a), scipy.stats.sem(a)
    h = se * scipy.stats.t.ppf((1 + confidence) / 2., n-1)
    if math.isnan(h):
        return 0
    return h


def mean_df(df):
    return df.apply(np.mean)

def filter_results_df(df_results, classfication_modes=None, loc_algos=None):

    df_filtered_results = df_results
    if classfication_modes:
        df_filtered_results = df_filtered_results[df_filtered_results["ClassificationMode"].apply(lambda x: re.classification_mode_reverse_lookup[x] in classfication_modes)]

    if loc_algos: 
        df_filtered_results = df_filtered_results[df_filtered_results["LocAlgo"].apply(lambda x: re.algos_reverse_lookup[x] in loc_algos)]

    return df_filtered_results


def get_used_bits(df_results):
    used_bit_map = []
    for column_name in df_results.columns:

        if "_FP_abs" in column_name and column_name.replace("_FP_abs","") in re.bits_reverse_lookup.keys() and re.bits_reverse_lookup[column_name.replace("_FP_abs","")] not in used_bit_map:
            used_bit_map.append(re.bits_reverse_lookup[column_name.replace("_FP_abs","")])

        if "_abs_less_than_5perc" in column_name and column_name.replace("_abs_less_than_5perc","") in re.bits_reverse_lookup.keys() and re.bits_reverse_lookup[column_name.replace("_abs_less_than_5perc","")] not in used_bit_map:
            used_bit_map.append(re.bits_reverse_lookup[column_name.replace("_abs_less_than_5perc","")])

        if "_abs_less_than_5ms" in column_name and column_name.replace("_abs_less_than_5ms","") in re.bits_reverse_lookup.keys() and re.bits_reverse_lookup[column_name.replace("_abs_less_than_5ms","")] not in used_bit_map:
            used_bit_map.append(re.bits_reverse_lookup[column_name.replace("_abs_less_than_5ms","")])

        if "_abs_raw_for_cdf" in column_name and column_name.replace("_abs_raw_for_cdf","") in re.bits_reverse_lookup.keys() and re.bits_reverse_lookup[column_name.replace("_abs_raw_for_cdf","")] not in used_bit_map:
            used_bit_map.append(re.bits_reverse_lookup[column_name.replace("_abs_raw_for_cdf","")])
    
    return used_bit_map


def import_sim_results_as_lsqr_df(analysis_configuration, sim_config_folder, how_many=None,
                                use_configured_loss: bool = False,
                                use_configured_delay: bool = False,
                                iterative=True):

    if not how_many:
        how_many = len(analysis_configuration["which_config_files"])

    simResultSets = []
    import_counter = 1

    df_collected_results = pd.DataFrame()
    for config_file_tuple in analysis_configuration["which_config_files"][:how_many]:
        
        try:

            analysis_config_name = config_file_tuple[0]
            sim_config_name = config_file_tuple[1]
            analysis_result_subfolder = config_file_tuple[2]
            analysis_result_prefix = config_file_tuple[3]

            sim_config_file = os.path.join(sim_config_folder, sim_config_name)
            print(f"Load sim config: {sim_config_file} ({(import_counter/how_many) * 100} %)")
            sim_configuration = None
            with open(sim_config_file, "r") as input_file: 
                sim_configuration = json.load(input_file)

            simResultsPath = os.path.join(analysis_configuration["analysis_output_dir"], analysis_result_subfolder)
            simResults = dm.ImportSimRunResultSet(simResultsPath, "analysis-" + analysis_result_prefix)

            if not iterative:
                simResultSets.append(simResults)
            else:
                run_dataframe = re.getRunSummaryDataFrame_For_LSQR([simResults], use_configured_loss, use_configured_delay)

                if df_collected_results.empty:
                    df_collected_results = run_dataframe
                else:
                    df_collected_results = pd.concat([df_collected_results, run_dataframe], ignore_index=True)

        except FileNotFoundError as e:
            print(e)
            print("Folder does not exist")
            continue
        except ValueError as e:
            print("Not sim results")
            print(e)
            continue
        except Exception as e:
            print("Other error")
            print(e)

        import_counter += 1


    if iterative:
        return df_collected_results
    else:
        return re.getRunSummaryDataFrame_For_LSQR(simResultSets, use_configured_loss, use_configured_delay)


def extract_lsqr_classification(analysis_configuration, sim_config_folder, df_savefile_lsqr_name, how_many=None, force_refresh=True,
                                use_configured_loss: bool = False,
                                use_configured_delay: bool = False):

    def import_from_scratch():
        df_collected_results = import_sim_results_as_lsqr_df(analysis_configuration, sim_config_folder, how_many, use_configured_loss, use_configured_delay)
        print(f"Store filtered results to file ({df_savefile_lsqr_name})")
        with open(df_savefile_lsqr_name, "w") as df_savefile:
            df_savefile.write(df_collected_results.to_json())
        return df_collected_results
    
    df_collected_results = None
    if force_refresh:
        print("Enforce reloading results from scratch")
        df_collected_results = import_from_scratch()

    else:
        try:
            with open(df_savefile_lsqr_name, "r") as df_savefile:
                df_collected_results = pd.read_json(df_savefile)

        except FileNotFoundError:
            print("Import simulation results into dataframe.")        
            df_collected_results = import_from_scratch()
        else:
            print(f"Use stored dataframe ({df_savefile_lsqr_name})")

    return df_collected_results