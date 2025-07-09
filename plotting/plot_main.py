import matplotlib.pyplot as plt

import numpy as np
import os
import sys

import json
import matplotlib as mpl
import matplotlib.patches as mpatches
from matplotlib.transforms import Bbox
import matplotlib.gridspec as gridspec

plot_config_folder = os.path.join(os.path.dirname(os.path.realpath(__file__)), "configs")
main_folder = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
post_processing_path = os.path.join(main_folder, "raw-result-processing", "src", "post-processing")
sys.path.append(post_processing_path)

configs_meta_folder = os.path.join(main_folder, "automation", "configs-meta")
sim_config_folder = os.path.join(main_folder, "automation", "simulation", "configs-sim")

import data_manager as dm
import result_extraction as re

import plot_helper as ph
import pandas as pd

def pt2inch(value):
    return 0.01384*value


def x_axis_cut(ax, latency_plot_x_cut=False, abs_or_rel="abs", loss_plot_x_cut=False):
    if latency_plot_x_cut and abs_or_rel == "rel":
        ax.set_xlim(left=-1)
    if loss_plot_x_cut and abs_or_rel == "rel":
        ax.set_xlim(left=-1.5)


def read_plot_config(plot_config_path):
    with open(os.path.join(plot_config_folder, plot_config_path), "r") as input_file: 
        return json.load(input_file)

def read_analysis_config(plot_config):
    with open(os.path.join(configs_meta_folder, plot_config["analysis_config"]), "r") as input_file: 
        return json.load(input_file)

def create_directories(plot_config, analysis_configuration):
    os.makedirs(plot_config["plot_output_folder"], exist_ok=True)
    plot_output_folder = os.path.join(plot_config["plot_output_folder"], analysis_configuration["experiment_identifier"])
    os.makedirs(plot_output_folder,exist_ok=True)

    os.makedirs(plot_config["plot_cache_folder"], exist_ok=True)
    plot_cache_folder = os.path.join(plot_config["plot_cache_folder"], analysis_configuration["experiment_identifier"])
    os.makedirs(plot_cache_folder,exist_ok=True)
    return plot_output_folder, plot_cache_folder

def read_df(plot_config, overall_plot_config, analysis_configuration, plot_cache_folder, plot_cachefile_name_key, df_savefile_name_key, use_configured_delay):

    tmp_filename_configured = os.path.join(plot_cache_folder,plot_config[plot_cachefile_name_key])
    print("Read dataframe: ", tmp_filename_configured)
    df_filtered_results = None
    if not os.path.exists(tmp_filename_configured) or overall_plot_config["force_refresh"]:
        
        df_filtered_results = ph.extract_lsqr_classification(
            analysis_configuration,
            sim_config_folder = sim_config_folder,
            df_savefile_lsqr_name = os.path.join(plot_cache_folder, plot_config[df_savefile_name_key]),
            how_many=None,
            force_refresh=overall_plot_config["force_refresh"],
            use_configured_loss=False,
            use_configured_delay=use_configured_delay)

        with open(tmp_filename_configured, "w") as outfile:
            outfile.write(df_filtered_results.to_json())

    else:
        with open(tmp_filename_configured, "r") as infile:
            df_filtered_results = pd.read_json(infile)
    return df_filtered_results


def plot_latency_values_cdf_core_edge(df_results, ax, chosen_linestyle, bit, abs_or_rel, core_or_edge, color=None):
    if bit not in ["spin", "dart", "pd"] or abs_or_rel not in ["abs", "rel"] or core_or_edge not in ["edge","core"]:
        raise Exception(f"plot_latency_values expects that: bit in ['spin", "dart", "pd'] ; abs_or_rel in ['abs', 'rel'] ; core_or_edge in ['edge','core']. Given: {bit},{abs_or_rel},{core_or_edge}")
    if not color:
        color = ph.bit_plot_color_map[re.bits_reverse_lookup[bit]]
    latency_vals = [iteration_value for iteration_result in list(df_results[[f"{bit}_{abs_or_rel}_{core_or_edge}_raw_for_cdf"]].iterrows()) for iteration_value in iteration_result[1][0]]
    sorted_latency_vals = np.sort(latency_vals)
    cp_prob = 1. * np.arange(len(sorted_latency_vals))/(len(sorted_latency_vals) - 1)
    ax.plot(sorted_latency_vals, cp_prob, markersize=2, markevery=900000, 
                color=color,
                linestyle=chosen_linestyle)

def add_bit_to_handles(bit, legend_handles, legend_labels, plot_colors):
    if ph.bit_plot_name_map[bit] not in legend_labels:
        legend_handles.append(mpatches.Patch(facecolor=ph.bit_plot_color_map[bit], edgecolor="black", label=ph.bit_plot_name_map[bit]))
        legend_labels.append(ph.bit_plot_name_map[bit])
        plot_colors.append(ph.bit_plot_color_map[bit])
    return legend_handles, legend_labels, plot_colors

def add_topology_to_handles(topology, legend_handles, legend_labels, plot_colors):
    if ph.topology_name_map[topology] not in legend_labels:
        legend_handles.append(mpatches.Patch(facecolor=ph.topology_color_map[topology], edgecolor="black", label=ph.topology_name_map[topology]))
        legend_labels.append(ph.topology_name_map[topology])
        plot_colors.append(ph.topology_color_map[topology])
    return legend_handles, legend_labels, plot_colors


"""
######################################
######################################

LATENCY PLOTS

######################################
######################################
"""

def do_baseline_latency_plot(df_filtered_results, plot_output_folder, 
                                traffic_ratio=16,
                                ping_interval=10,
                                ping_packet=300,
                                ping_scenario="ALL_TO_ALL",
                                flow_selection="ALL",
                                abs_or_rel="abs",
                                with_groundtruth=False,
                                localizationMethod="LIN_LSQR_CORE_ONLY",
                                observerSelectionAlgorithm="All",
                                cachefile_base_name=None,
                                ping_cachefile_base_name=None):

    localizationMethod = dm.LocalizationMethod[localizationMethod].value
    
    fig = plt.figure()

    gs0 = gridspec.GridSpec(1, 1, hspace=0, wspace=0)
    ax = fig.add_subplot(gs0[0])

   
    legend_handles = []
    legend_labels = []
    plot_colors = []

    
    plot_cache_name = ""
    if cachefile_base_name:
        plot_cache_name = os.path.join(plot_output_folder, cachefile_base_name)
    else:
        plot_cache_name = os.path.join(plot_output_folder, "do_baseline_latency_plot.pkl")

    ping_cache_name = ""
    if ping_cachefile_base_name:
        ping_cache_name = os.path.join(plot_output_folder, ping_cachefile_base_name)
    else:
        ping_cache_name = os.path.join(plot_output_folder, "do_baseline_latency_plot_ping.pkl")

    one_setting_results = None
    ping_base_results = None
    if df_filtered_results.empty:
        if not os.path.exists(plot_cache_name):
            print("No data to plot do_baseline_latency_plot.")
            return
        else:
            one_setting_results = pd.read_pickle(plot_cache_name)
        if not os.path.exists(ping_cache_name):
            print("No data to plot do_baseline_latency_plot.")
            return
        else:
            ping_base_results = pd.read_pickle(ping_cache_name)
    else:

        ### Only select one traffic ratio/ping interval / ping packet combination
        if traffic_ratio not in df_filtered_results["trafficScenarioRatio"].unique():
            raise Exception(f"Chosen traffic ratio {traffic_ratio} not in data: {df_filtered_results['trafficScenarioRatio'].unique()}.")

        if ping_interval not in df_filtered_results["pingIntervalMs"].unique():
            raise Exception(f"Chosen ping interval {ping_interval} not in data: {df_filtered_results['pingIntervalMs'].unique()}.")

        if ping_packet not in df_filtered_results["pingPacketCount"].unique():
            raise Exception(f"Chosen ping packet count {ping_packet} not in data: {df_filtered_results['pingPacketCount'].unique()}.")
        
        if ping_scenario not in df_filtered_results["pingScenario"].unique():
            raise Exception(f"Chosen ping scenario {ping_scenario} not in data: {df_filtered_results['pingScenario'].unique()}.")
        
        if flow_selection not in df_filtered_results["flowSelectionStrategy"].unique():
            raise Exception(f"Chosen flow selection strategy {flow_selection} not in data: {df_filtered_results['flowSelectionStrategy'].unique()}.")

        if abs_or_rel not in ["abs", "rel"]:
            raise Exception(f"Please choose 'abs' or 'rel' for abs_or_rel and not {abs_or_rel}")

        one_setting_results = df_filtered_results[(df_filtered_results["trafficScenarioRatio"] == traffic_ratio) \
                                                    & (df_filtered_results["pingIntervalMs"] == ping_interval) \
                                                    & (df_filtered_results["pingPacketCount"] == ping_packet)\
                                                    & (df_filtered_results["pingScenario"] == ping_scenario)\
                                                    & (df_filtered_results["flowSelectionStrategy"] == flow_selection)\
                                                    & (df_filtered_results["localizationMethod"] == localizationMethod)\
                                                    & (df_filtered_results["observerSelectionAlgorithm"] == observerSelectionAlgorithm)]

        one_setting_results.to_pickle(plot_cache_name)

        ping_base_results = df_filtered_results[(df_filtered_results["trafficScenarioRatio"] == traffic_ratio) \
            & (df_filtered_results["pingIntervalMs"] == ping_interval) \
            & (df_filtered_results["pingPacketCount"] == ping_packet)\
            & (df_filtered_results["pingScenario"] == ping_scenario)\
            & (df_filtered_results["flowSelectionStrategy"] == flow_selection)\
            & (df_filtered_results["localizationMethod"] == dm.LocalizationMethod.LIN_LSQR_CORE_ONLY.value)\
            & (df_filtered_results["observerSelectionAlgorithm"] == observerSelectionAlgorithm)]

        ping_base_results.to_pickle(ping_cache_name)


    for topology in one_setting_results["topology"].unique():

        topology_results = one_setting_results[(one_setting_results["topology"] == topology)]

        ### Spin bit values
        plot_latency_values_cdf_core_edge(topology_results, ax, ph.topology_line_type_map[topology], "spin", abs_or_rel, "core")
        legend_handles, legend_labels, plot_colors = add_bit_to_handles(dm.EfmBit.SPIN, legend_handles, legend_labels, plot_colors)

        ### Dart values
        plot_latency_values_cdf_core_edge(topology_results, ax, ph.topology_line_type_map[topology], "dart", abs_or_rel, "core")
        legend_handles, legend_labels, plot_colors = add_bit_to_handles(dm.EfmBit.TCPDART, legend_handles, legend_labels, plot_colors)

        ### Ping values
        if not localizationMethod == dm.LocalizationMethod.FLOW_COMBINATION.value:
            plot_latency_values_cdf_core_edge(topology_results, ax, ph.topology_line_type_map[topology], "pd", abs_or_rel, "core")
            legend_handles, legend_labels, plot_colors = add_bit_to_handles(dm.EfmBit.PINGDLY, legend_handles, legend_labels, plot_colors)
        else:

            ping_setting_results = ping_base_results[(ping_base_results["topology"] == topology)]

            plot_latency_values_cdf_core_edge(ping_setting_results, ax, ph.topology_line_type_map[topology], "pd", abs_or_rel, "core")
            legend_handles, legend_labels, plot_colors = add_bit_to_handles(dm.EfmBit.PINGDLY, legend_handles, legend_labels, plot_colors)


    MARKERSIZE_LEGEND=4
    marker_handles = [mpl.lines.Line2D([], [], color='black', linestyle=ph.topology_line_type_map[topology],markersize=MARKERSIZE_LEGEND) for topology in sorted(one_setting_results["topology"].unique())]
    marker_labels = [ph.topology_name_map[topology] for topology in sorted(one_setting_results["topology"].unique())]


    ax_overall = fig.add_subplot(111, frameon=False)
    ax_overall.tick_params(labelcolor='none', which='both', top=False, bottom=False, left=False, right=False)
    ax_overall.set_xlabel("")
    ax_overall.set_ylabel("")
    bbox = Bbox([[-0.13,-0.425],[1.025,1.3]])
    bbox = bbox.transformed(ax_overall.transData).transformed(fig.dpi_scale_trans.inverted())


    ax_overall.legend(handles=legend_handles+marker_handles,
        labels=legend_labels+marker_labels,
        fontsize=7,
        ncol=6,
        bbox_to_anchor = [0.45, 1.05],
        columnspacing=0.7,
        loc="lower center")

    ax.set_ylabel("CDF [\%]")
    ax.set_ylim(0,1)
    ax.set_yticks([0,0.25,0.5,0.75,1])
    ax.set_yticklabels([0,25,50,75,100])
    ax.set_xlim(-1,50)

    ax.set_xlabel("Latency difference [ms]" if abs_or_rel == "abs" else "Relative latency difference", labelpad=0)

    figname= os.path.join(plot_output_folder,f"{plot_config['plot_basename']}_latency_baseline_{traffic_ratio}_{ping_interval}_{ping_packet}_{ping_scenario}_{flow_selection}_{abs_or_rel}{'_with_groundtruth' if with_groundtruth else ''}{localizationMethod}.{'pdf' if overall_plot_config['pdf_mode'] else 'png'}")
    fig.savefig(figname)

    plt.close()


def do_baseline_comparison_latency_plot(df_filtered_results, plot_output_folder, 
                                traffic_ratio=16,
                                ping_interval=10,
                                ping_packet=300,
                                ping_scenario="ALL_TO_ALL",
                                flow_selection="ALL",
                                abs_or_rel="abs",
                                with_groundtruth=False,
                                localizationMethod="LIN_LSQR_CORE_ONLY",
                                observerSelectionAlgorithm="All",
                                cachefile_base_name=None,
                                ping_cachefile_base_name=None):

    localizationMethod = dm.LocalizationMethod[localizationMethod].value
    
    fig = plt.figure()

    gs0 = gridspec.GridSpec(1, 1, hspace=0, wspace=0)
    ax = fig.add_subplot(gs0[0])

    legend_handles = []
    legend_labels = []
    plot_colors = []

    
    plot_cache_name = ""
    if cachefile_base_name:
        plot_cache_name = os.path.join(plot_output_folder, cachefile_base_name)
    else:
        plot_cache_name = os.path.join(plot_output_folder, "do_baseline_latency_plot.pkl")

    ping_cache_name = ""
    if ping_cachefile_base_name:
        ping_cache_name = os.path.join(plot_output_folder, ping_cachefile_base_name)
    else:
        ping_cache_name = os.path.join(plot_output_folder, "do_baseline_latency_plot_ping.pkl")



    one_setting_results = None
    ping_base_results = None
    if df_filtered_results.empty:
        if not os.path.exists(plot_cache_name):
            print("No data to plot do_baseline_latency_plot.")
            return
        else:
            one_setting_results = pd.read_pickle(plot_cache_name)
        if not os.path.exists(ping_cache_name):
            print("No data to plot do_baseline_latency_plot.")
            return
        else:
            ping_base_results = pd.read_pickle(ping_cache_name)
    else:

        if traffic_ratio not in df_filtered_results["trafficScenarioRatio"].unique():
            raise Exception(f"Chosen traffic ratio {traffic_ratio} not in data: {df_filtered_results['trafficScenarioRatio'].unique()}.")

        if ping_interval not in df_filtered_results["pingIntervalMs"].unique():
            raise Exception(f"Chosen ping interval {ping_interval} not in data: {df_filtered_results['pingIntervalMs'].unique()}.")

        if ping_packet not in df_filtered_results["pingPacketCount"].unique():
            raise Exception(f"Chosen ping packet count {ping_packet} not in data: {df_filtered_results['pingPacketCount'].unique()}.")
        
        if ping_scenario not in df_filtered_results["pingScenario"].unique():
            raise Exception(f"Chosen ping scenario {ping_scenario} not in data: {df_filtered_results['pingScenario'].unique()}.")
        
        if flow_selection not in df_filtered_results["flowSelectionStrategy"].unique():
            raise Exception(f"Chosen flow selection strategy {flow_selection} not in data: {df_filtered_results['flowSelectionStrategy'].unique()}.")

        if abs_or_rel not in ["abs", "rel"]:
            raise Exception(f"Please choose 'abs' or 'rel' for abs_or_rel and not {abs_or_rel}")

        one_setting_results = df_filtered_results[(df_filtered_results["trafficScenarioRatio"] == traffic_ratio) \
                                                    & (df_filtered_results["pingIntervalMs"] == ping_interval) \
                                                    & (df_filtered_results["pingPacketCount"] == ping_packet)\
                                                    & (df_filtered_results["pingScenario"] == ping_scenario)\
                                                    & (df_filtered_results["flowSelectionStrategy"] == flow_selection)\
                                                    & (df_filtered_results["localizationMethod"] == localizationMethod)\
                                                    & (df_filtered_results["observerSelectionAlgorithm"] == observerSelectionAlgorithm)]

        one_setting_results.to_pickle(plot_cache_name)

        ping_base_results = df_filtered_results[(df_filtered_results["trafficScenarioRatio"] == traffic_ratio) \
            & (df_filtered_results["pingIntervalMs"] == ping_interval) \
            & (df_filtered_results["pingPacketCount"] == ping_packet)\
            & (df_filtered_results["pingScenario"] == ping_scenario)\
            & (df_filtered_results["flowSelectionStrategy"] == flow_selection)\
            & (df_filtered_results["localizationMethod"] == dm.LocalizationMethod.LIN_LSQR_CORE_ONLY.value)\
            & (df_filtered_results["observerSelectionAlgorithm"] == observerSelectionAlgorithm)]

        ping_base_results.to_pickle(ping_cache_name)

    for topology in one_setting_results["topology"].unique():

        topology_results = one_setting_results[(one_setting_results["topology"] == topology)]

        ### Spin bit values
        plot_latency_values_cdf_core_edge(topology_results, ax, ph.topology_line_type_map[topology], "spin", abs_or_rel, "core")
        legend_handles, legend_labels, plot_colors = add_bit_to_handles(dm.EfmBit.SPIN, legend_handles, legend_labels, plot_colors)

        ### Dart values
        plot_latency_values_cdf_core_edge(topology_results, ax, ph.topology_line_type_map[topology], "dart", abs_or_rel, "core")
        legend_handles, legend_labels, plot_colors = add_bit_to_handles(dm.EfmBit.TCPDART, legend_handles, legend_labels, plot_colors)

        ### Ping values
        if not localizationMethod == dm.LocalizationMethod.FLOW_COMBINATION.value:
            plot_latency_values_cdf_core_edge(topology_results, ax, ph.topology_line_type_map[topology], "pd", abs_or_rel, "core")
            legend_handles, legend_labels, plot_colors = add_bit_to_handles(dm.EfmBit.PINGDLY, legend_handles, legend_labels, plot_colors)
        else:

            ping_setting_results = ping_base_results[(ping_base_results["topology"] == topology)]

            plot_latency_values_cdf_core_edge(ping_setting_results, ax, ph.topology_line_type_map[topology], "pd", abs_or_rel, "core")
            legend_handles, legend_labels, plot_colors = add_bit_to_handles(dm.EfmBit.PINGDLY, legend_handles, legend_labels, plot_colors)


    MARKERSIZE_LEGEND=4
    marker_handles = [mpl.lines.Line2D([], [], color='black', linestyle=ph.topology_line_type_map[topology],markersize=MARKERSIZE_LEGEND) for topology in sorted(one_setting_results["topology"].unique())]
    marker_labels = [ph.topology_name_map[topology] for topology in sorted(one_setting_results["topology"].unique())]


    ax_overall = fig.add_subplot(111, frameon=False)
    ax_overall.tick_params(labelcolor='none', which='both', top=False, bottom=False, left=False, right=False)
    ax_overall.set_xlabel("")
    ax_overall.set_ylabel("")
    bbox = Bbox([[-0.13,-0.425],[1.03,1.3]])
    bbox = bbox.transformed(ax_overall.transData).transformed(fig.dpi_scale_trans.inverted())


    ax_overall.legend(handles=legend_handles+marker_handles,
        labels=legend_labels+marker_labels,
        fontsize=7,
        ncol=6,
        bbox_to_anchor = [0.45, 1.05],
        columnspacing=0.7,
        loc="lower center")

    ax.set_ylabel("CDF [\%]")
    ax.set_ylim(0,1)
    ax.set_yticks([0,0.25,0.5,0.75,1])
    ax.set_yticklabels([0,25,50,75,100])
    ax.set_xlim(-1,1)

    ax.set_xlabel("Latency difference [ms]" if abs_or_rel == "abs" else "Relative latency difference", labelpad=0)

    figname= os.path.join(plot_output_folder,f"{plot_config['plot_basename']}_latency_baseline_{traffic_ratio}_{ping_interval}_{ping_packet}_{ping_scenario}_{flow_selection}_{abs_or_rel}{'_with_groundtruth' if with_groundtruth else ''}{localizationMethod}.{'pdf' if overall_plot_config['pdf_mode'] else 'png'}")
    fig.savefig(figname)

    plt.close()


"""
######################################
######################################

LOSS PLOTS

######################################
######################################
"""

def plot_loss_values_cdf_core_edge(df_results, ax, chosen_linestyle, bit, abs_or_rel, core_or_edge, with_actual=False, without_actual=False, color=None):
    if bit not in ["q", "r", "qr", "l", "ql", "pl", "routescout"] or abs_or_rel not in ["abs", "rel"] or core_or_edge not in ["edge","core"]:
        raise Exception(f"plot_loss_values expects that: bit in ['q', 'r', 'qr', 'l', 'ql', 'pl', 'routescout'] ; abs_or_rel in ['abs', 'rel'] ; core_or_edge in ['edge','core']. Given: {bit},{abs_or_rel},{core_or_edge}")
    
    lookup_key = None
    if with_actual:
        lookup_key = f"{bit}_with_actual_loss_{abs_or_rel}_{core_or_edge}_for_cdf"
        if abs_or_rel == "rel":
            lookup_key = f"{bit}_with_actual_{abs_or_rel}_{core_or_edge}_for_cdf"

    else:
        abs_or_rel = "abs"
        lookup_key = f"{bit}_{abs_or_rel}_{core_or_edge}_raw_for_cdf"

    if without_actual:
        lookup_key = f"{bit}_without_actual_loss_abs_{core_or_edge}_for_cdf"

    if not color:
        color = ph.bit_plot_color_map[re.bits_reverse_lookup[bit]]

    loss_vals = [iteration_value for iteration_result in list(df_results[[lookup_key]].iterrows()) for iteration_value in iteration_result[1].iloc[0]]
    sorted_loss_vals = np.sort(loss_vals)
    cp_prob = 1. * np.arange(len(sorted_loss_vals))/(len(sorted_loss_vals) - 1)
    ax.plot(sorted_loss_vals, cp_prob, markersize=2, markevery=900000, 
                color=color,
                linestyle=chosen_linestyle)


def do_baseline_loss_plot(df_filtered_results, plot_output_folder, 
                            traffic_ratio=16,
                            ping_interval=10,
                            ping_packet=300,
                            ping_scenario="ALL_TO_ALL",
                            flow_selection="ALL",
                            abs_or_rel="abs",
                            with_delay_groundtruth=False,
                            loss_rate=0.01,
                            failed_links=1,
                            with_actual=False,
                            localizationMethod="LIN_LSQR_CORE_ONLY",
                            observerSelectionAlgorithm="All"):
    localizationMethod = dm.LocalizationMethod[localizationMethod].value

    fig = plt.figure()

    ratio_width = 0.49
    gs0 = gridspec.GridSpec(1, 3, width_ratios=[ratio_width,1 - 2*ratio_width, ratio_width], hspace=0, wspace=0.025)
    gs00 = gridspec.GridSpecFromSubplotSpec(1, 1, subplot_spec=gs0[0], hspace=0.025, wspace=0.05)
    ax1 = fig.add_subplot(gs00[0])
    
    gs02 = gridspec.GridSpecFromSubplotSpec(1, 1, subplot_spec=gs0[2], hspace=0.025, wspace=0.05)
    ax2 = fig.add_subplot(gs02[0])

    legend_handles = []
    legend_labels = []
    plot_colors = []

    plot_cache_name = os.path.join(plot_output_folder, "do_baseline_loss_plot.pkl")
    one_setting_results = None
    if df_filtered_results.empty:
        if not os.path.exists(plot_cache_name):
            print("No data to plot do_baseline_loss_plot.")
            return
        else:
            one_setting_results = pd.read_pickle(plot_cache_name)
    else:
        if traffic_ratio not in df_filtered_results["trafficScenarioRatio"].unique():
            raise Exception(f"Chosen traffic ratio {traffic_ratio} not in data: {df_filtered_results['trafficScenarioRatio'].unique()}.")

        if ping_interval not in df_filtered_results["pingIntervalMs"].unique():
            raise Exception(f"Chosen ping interval {ping_interval} not in data: {df_filtered_results['pingIntervalMs'].unique()}.")

        if ping_packet not in df_filtered_results["pingPacketCount"].unique():
            raise Exception(f"Chosen ping packet count {ping_packet} not in data: {df_filtered_results['pingPacketCount'].unique()}.")

        if abs_or_rel not in ["abs", "rel"]:
            raise Exception(f"Please choose 'abs' or 'rel' for abs_or_rel and not {abs_or_rel}")

        if loss_rate not in df_filtered_results["failed_link_loss"].unique():
            raise Exception(f"Chosen loss_rate {loss_rate} not in data: {df_filtered_results['failed_link_loss'].unique()}.")
        
        if flow_selection not in df_filtered_results["flowSelectionStrategy"].unique():
            raise Exception(f"Chosen flow selection strategy {flow_selection} not in data: {df_filtered_results['flowSelectionStrategy'].unique()}.")

        if failed_links not in df_filtered_results["failed_link_count"].unique():
            raise Exception(f"Chosen failed links count {failed_links} not in data: {df_filtered_results['failed_link_count'].unique()}.")
        

        one_setting_results = df_filtered_results[(df_filtered_results["trafficScenarioRatio"] == traffic_ratio)&
                                                (df_filtered_results["pingIntervalMs"] == ping_interval) & 
                                                (df_filtered_results["pingPacketCount"] == ping_packet) & 
                                                (df_filtered_results["failed_link_loss"] == loss_rate) & 
                                                (df_filtered_results["failed_link_count"] == failed_links) & 
                                                (df_filtered_results["pingScenario"] == ping_scenario) & 
                                                (df_filtered_results["flowSelectionStrategy"] == flow_selection) & 
                                                (df_filtered_results["localizationMethod"] == localizationMethod) & 
                                                (df_filtered_results["observerSelectionAlgorithm"] == observerSelectionAlgorithm)]
    
        one_setting_results.to_pickle(plot_cache_name)

    for topology in one_setting_results["topology"].unique():

        topology_results = one_setting_results[(one_setting_results["topology"] == topology)]


        ### Q bit values
        plot_loss_values_cdf_core_edge(topology_results, ax1, ph.topology_line_type_map[topology], "q", abs_or_rel, "core", with_actual=True)
        plot_loss_values_cdf_core_edge(topology_results, ax2, ph.topology_line_type_map[topology], "q", "abs", "core", without_actual=True)
        legend_handles, legend_labels, plot_colors = add_bit_to_handles(dm.EfmBit.Q, legend_handles, legend_labels, plot_colors)

        ### R bit values
        plot_loss_values_cdf_core_edge(topology_results, ax1, ph.topology_line_type_map[topology], "r", abs_or_rel, "core", with_actual=True)
        plot_loss_values_cdf_core_edge(topology_results, ax2, ph.topology_line_type_map[topology], "r", "abs", "core", without_actual=True)
        legend_handles, legend_labels, plot_colors = add_bit_to_handles(dm.EfmBit.R, legend_handles, legend_labels, plot_colors)

        ### L bit values
        plot_loss_values_cdf_core_edge(topology_results, ax1, ph.topology_line_type_map[topology], "l", abs_or_rel, "core", with_actual=True)
        plot_loss_values_cdf_core_edge(topology_results, ax2, ph.topology_line_type_map[topology], "l", "abs", "core", without_actual=True)
        legend_handles, legend_labels, plot_colors = add_bit_to_handles(dm.EfmBit.L, legend_handles, legend_labels, plot_colors)
        
        ### Routescout values
        plot_loss_values_cdf_core_edge(topology_results, ax1, ph.topology_line_type_map[topology], "routescout", abs_or_rel, "core", with_actual=True)
        plot_loss_values_cdf_core_edge(topology_results, ax2, ph.topology_line_type_map[topology], "routescout", "abs", "core", without_actual=True)
        legend_handles, legend_labels, plot_colors = add_bit_to_handles(dm.EfmBit.TCPRO, legend_handles, legend_labels, plot_colors)

        
        ### Ping values
        if not localizationMethod == dm.LocalizationMethod.FLOW_COMBINATION.value:
            plot_loss_values_cdf_core_edge(topology_results, ax1, ph.topology_line_type_map[topology], "pl", abs_or_rel, "core", with_actual=True)
            plot_loss_values_cdf_core_edge(topology_results, ax2, ph.topology_line_type_map[topology], "pl", "abs", "core", without_actual=True)
            legend_handles, legend_labels, plot_colors = add_bit_to_handles(dm.EfmBit.PINGLSS, legend_handles, legend_labels, plot_colors)

        else:

            ping_setting_results = df_filtered_results[(df_filtered_results["trafficScenarioRatio"] == traffic_ratio) &
                                                (df_filtered_results["pingIntervalMs"] == ping_interval) & (df_filtered_results["pingPacketCount"] == ping_packet)&
                                                (df_filtered_results["failed_link_loss"] == loss_rate) & (df_filtered_results["failed_link_count"] == failed_links)\
                                                & (df_filtered_results["pingScenario"] == ping_scenario)\
                                                & (df_filtered_results["flowSelectionStrategy"] == flow_selection)\
                                                & (df_filtered_results["localizationMethod"] == dm.LocalizationMethod.LIN_LSQR_CORE_ONLY.value)\
                                                & (df_filtered_results["observerSelectionAlgorithm"] == observerSelectionAlgorithm)]

            plot_loss_values_cdf_core_edge(ping_setting_results, ax1, ph.topology_line_type_map[topology], "pl", abs_or_rel, "core", with_actual=True)
            plot_loss_values_cdf_core_edge(ping_setting_results, ax2, ph.topology_line_type_map[topology], "pl", "abs", "core", without_actual=True)
            legend_handles, legend_labels, plot_colors = add_bit_to_handles(dm.EfmBit.PINGLSS, legend_handles, legend_labels, plot_colors)




    MARKERSIZE_LEGEND=4
    marker_handles = [mpl.lines.Line2D([], [], color='black', linestyle=ph.topology_line_type_map[topology],markersize=MARKERSIZE_LEGEND) for topology in sorted(one_setting_results["topology"].unique())]
    marker_labels = [ph.topology_name_map[topology] for topology in sorted(one_setting_results["topology"].unique())]


    ax_overall = fig.add_subplot(111, frameon=False)
    ax_overall.tick_params(labelcolor='none', which='both', top=False, bottom=False, left=False, right=False)
    ax_overall.set_xlabel("")
    ax_overall.set_ylabel("")
    bbox = Bbox([[-0.15,-0.675],[1.015,1.4]])
    bbox = bbox.transformed(ax_overall.transData).transformed(fig.dpi_scale_trans.inverted())


    dummyPlot, = plt.plot([0], marker='None',
            linestyle='None', label='')

    new_legend_handles = [dummyPlot, legend_handles[0],
                            marker_handles[0], legend_handles[1],
                            marker_handles[1], legend_handles[2],
                            marker_handles[2], legend_handles[3],
                            dummyPlot, legend_handles[4]]
    
    new_legend_labels = ["", legend_labels[0],
                            marker_labels[0], legend_labels[1],
                            marker_labels[1], legend_labels[2],
                            marker_labels[2], legend_labels[3],
                            "", legend_labels[4]]

    ax_overall.legend(handles=new_legend_handles,
        labels=new_legend_labels,
        fontsize=7,
        ncol=5,
        bbox_to_anchor = [0.5, 0.99],
        columnspacing=1,
        loc="lower center")


    ax1.set_ylabel("CDF [\%]")
    ax1.set_ylim(-0.01,1.01)
    ax1.set_yticks([0,0.25,0.5,0.75,1])
    ax1.set_yticklabels([0,25,50,75,100])
    ax2.set_ylim(-0.01,1.01)
    ax2.set_yticks([0,0.25,0.5,0.75,1])
    ax2.set_yticklabels([])
    x_axis_cut(ax1, abs_or_rel=abs_or_rel, loss_plot_x_cut=True)

    labelpad=0
    ax1.set_xlabel("Abs. loss difference [\%]\n Links with loss" if abs_or_rel == "abs" else "Rel. loss difference\n Links with loss", labelpad=labelpad)
    ax2.set_xlabel("Abs. loss difference [\%]\n Links without loss", labelpad=labelpad)
    ax_overall.set_xlabel("", labelpad=labelpad)

    figname= os.path.join(plot_output_folder,f"{plot_config['plot_basename']}_packetloss_baseline_{traffic_ratio}_{loss_rate}_{ping_interval}_{ping_packet}_{flow_selection}_{ping_scenario}_{abs_or_rel}_{'with_actual' if with_actual else 'all_links'}{localizationMethod}.{'pdf' if overall_plot_config['pdf_mode'] else 'png'}")
    fig.savefig(figname)

    plt.close()

def do_qbit_loss_combined_multi_link_packet_loss_ratio_plot(df_filtered_results, 
                                                            df_filtered_results_multilink,
                                                            plot_output_folder, 
                            traffic_ratio=16,
                            ping_interval=10,
                            ping_packet=300,
                            ping_scenario="ALL_TO_ALL",
                            flow_selection="ALL",
                            abs_or_rel="abs",
                            with_delay_groundtruth=False,
                            loss_rate=0.01,
                            with_actual=False,
                            localizationMethod="FLOW_COMBINATION",
                            observerSelectionAlgorithm="All"):
    localizationMethod = dm.LocalizationMethod[localizationMethod].value

    fig = plt.figure()

    ratio_width = 0.49
    gs0 = gridspec.GridSpec(1, 3, width_ratios=[ratio_width,1 - 2*ratio_width, ratio_width], hspace=0, wspace=0.025)
    gs00 = gridspec.GridSpecFromSubplotSpec(1, 1, subplot_spec=gs0[0], hspace=0.025, wspace=0.05)
    ax1 = fig.add_subplot(gs00[0])
    
    gs02 = gridspec.GridSpecFromSubplotSpec(1, 1, subplot_spec=gs0[2], hspace=0.025, wspace=0.05)
    ax2 = fig.add_subplot(gs02[0])

    legend_handles = []
    legend_labels = []
    plot_colors = []


    plot_cache_name = os.path.join(plot_output_folder, "do_qbit_loss_combined_multi_link_packet_loss_ratio_plot_one.pkl")
    one_setting_results = None
    if df_filtered_results_multilink.empty:
        if not os.path.exists(plot_cache_name):
            print("No data to plot do_qbit_loss_combined_multi_link_packet_loss_ratio_plot.")
            return
        else:
            one_setting_results = pd.read_pickle(plot_cache_name)
    else:

        if traffic_ratio not in df_filtered_results_multilink["trafficScenarioRatio"].unique():
            raise Exception(f"Chosen traffic ratio {traffic_ratio} not in data: {df_filtered_results_multilink['trafficScenarioRatio'].unique()}.")

        if ping_interval not in df_filtered_results_multilink["pingIntervalMs"].unique():
            raise Exception(f"Chosen ping interval {ping_interval} not in data: {df_filtered_results_multilink['pingIntervalMs'].unique()}.")

        if ping_packet not in df_filtered_results_multilink["pingPacketCount"].unique():
            raise Exception(f"Chosen ping packet count {ping_packet} not in data: {df_filtered_results_multilink['pingPacketCount'].unique()}.")

        if abs_or_rel not in ["abs", "rel"]:
            raise Exception(f"Please choose 'abs' or 'rel' for abs_or_rel and not {abs_or_rel}")

        if loss_rate not in df_filtered_results_multilink["failed_link_loss"].unique():
            raise Exception(f"Chosen loss_rate {loss_rate} not in data: {df_filtered_results_multilink['failed_link_loss'].unique()}.")
        
        if flow_selection not in df_filtered_results_multilink["flowSelectionStrategy"].unique():
            raise Exception(f"Chosen flow selection strategy {flow_selection} not in data: {df_filtered_results_multilink['flowSelectionStrategy'].unique()}.")



        one_setting_results = df_filtered_results_multilink[(df_filtered_results_multilink["trafficScenarioRatio"] == traffic_ratio) 
                                                  & (df_filtered_results_multilink["pingIntervalMs"] == ping_interval) 
                                                  & (df_filtered_results_multilink["pingPacketCount"] == ping_packet)
                                                    & (df_filtered_results_multilink["pingScenario"] == ping_scenario)
                                                    & (df_filtered_results_multilink["flowSelectionStrategy"] == flow_selection)
                                                    & (df_filtered_results_multilink["localizationMethod"] == localizationMethod)
                                                    & (df_filtered_results_multilink["observerSelectionAlgorithm"] == observerSelectionAlgorithm)
                                                        & (df_filtered_results_multilink["failed_link_loss"] == loss_rate)]

        one_setting_results.to_pickle(plot_cache_name)

    plot_cache_name = os.path.join(plot_output_folder, "do_qbit_loss_combined_multi_link_packet_loss_ratio_plot_two.pkl")
    two_setting_results = None
    if df_filtered_results.empty:
        if not os.path.exists(plot_cache_name):
            print("No data to plot do_qbit_loss_combined_multi_link_packet_loss_ratio_plot.")
            return
        else:
            two_setting_results = pd.read_pickle(plot_cache_name)
    else:

        if traffic_ratio not in df_filtered_results["trafficScenarioRatio"].unique():
            raise Exception(f"Chosen traffic ratio {traffic_ratio} not in data: {df_filtered_results['trafficScenarioRatio'].unique()}.")

        if ping_interval not in df_filtered_results["pingIntervalMs"].unique():
            raise Exception(f"Chosen ping interval {ping_interval} not in data: {df_filtered_results['pingIntervalMs'].unique()}.")

        if ping_packet not in df_filtered_results["pingPacketCount"].unique():
            raise Exception(f"Chosen ping packet count {ping_packet} not in data: {df_filtered_results['pingPacketCount'].unique()}.")

        if abs_or_rel not in ["abs", "rel"]:
            raise Exception(f"Please choose 'abs' or 'rel' for abs_or_rel and not {abs_or_rel}")

        if loss_rate not in df_filtered_results["failed_link_loss"].unique():
            raise Exception(f"Chosen loss_rate {loss_rate} not in data: {df_filtered_results['failed_link_loss'].unique()}.")
        
        if flow_selection not in df_filtered_results["flowSelectionStrategy"].unique():
            raise Exception(f"Chosen flow selection strategy {flow_selection} not in data: {df_filtered_results['flowSelectionStrategy'].unique()}.")



        two_setting_results = df_filtered_results[(df_filtered_results["trafficScenarioRatio"] == traffic_ratio) 
                                                  & (df_filtered_results["pingIntervalMs"] == ping_interval) 
                                                  & (df_filtered_results["pingPacketCount"] == ping_packet) 
                                                  & (df_filtered_results["failed_link_count"] == 1)
                                                  & (df_filtered_results["pingScenario"] == ping_scenario)
                                                  & (df_filtered_results["flowSelectionStrategy"] == flow_selection)
                                                  & (df_filtered_results["localizationMethod"] == localizationMethod)
                                                  & (df_filtered_results["observerSelectionAlgorithm"] == observerSelectionAlgorithm)]

        two_setting_results.to_pickle(plot_cache_name)


    multi_link_settings = [ph.COLORS[0], ph.COLORS[1], ph.COLORS[2], ph.COLORS[3], ph.COLORS[4]]
    multi_link_zipped_settings = [(setting,value) for setting, value in zip(multi_link_settings,sorted(one_setting_results["failed_link_count"].unique()))]

    link_error_settings = [ph.COLORS[5], ph.COLORS[6], ph.COLORS[7], ph.COLORS[8], ph.COLORS[9]]
    link_error_zipped_settings = [(setting,value) for setting, value in zip(link_error_settings,sorted(two_setting_results["failed_link_loss"].unique()))]

    for topology in one_setting_results["topology"].unique():
     
        topology_results = one_setting_results[(one_setting_results["topology"] == topology)]

        for settings, failed_link_count in multi_link_zipped_settings:

            setting_results = topology_results[(topology_results["failed_link_count"] == failed_link_count)]

            ### Q bit values
            plot_loss_values_cdf_core_edge(setting_results, ax2, ph.topology_line_type_map[topology], "q", abs_or_rel, "core", with_actual=with_actual, color=settings)


        topology_results = two_setting_results[(two_setting_results["topology"] == topology)]

        for settings, failed_link_loss in link_error_zipped_settings:

            setting_results = topology_results[(topology_results["failed_link_loss"] == failed_link_loss)]

            ### Q bit values
            plot_loss_values_cdf_core_edge(setting_results, ax1, ph.topology_line_type_map[topology], "q", abs_or_rel, "core", with_actual=with_actual, color=settings)


    ax_overall = fig.add_subplot(111, frameon=False)
    ax_overall.tick_params(labelcolor='none', which='both', top=False, bottom=False, left=False, right=False)
    ax_overall.set_xlabel("")
    ax_overall.set_ylabel("")
    bbox = Bbox([[-0.15,-0.675],[1.015,1.25]])
    bbox = bbox.transformed(ax_overall.transData).transformed(fig.dpi_scale_trans.inverted())

    dummyPlot, = plt.plot([0], marker='None',
            linestyle='None', label='')


    MARKERSIZE_LEGEND=4
    marker_handles = []
    marker_labels = []
    
    for topology in sorted(one_setting_results["topology"].unique()):
        marker_handles.append(mpl.lines.Line2D([], [], color='black', linestyle=ph.topology_line_type_map[topology],markersize=MARKERSIZE_LEGEND))
        marker_labels.append(ph.topology_name_map[topology])

    legend1 = ax_overall.legend(handles=marker_handles,
        labels=marker_labels,
        fontsize=7,
        ncol=5,
        bbox_to_anchor = [0.5, 0.99], 
        columnspacing=1,
        loc="lower center")


    marker_handles = []
    marker_labels = []
    for setting in multi_link_zipped_settings:
        marker_handles.append(mpl.lines.Line2D([], [], color=setting[0], linestyle="solid",markersize=MARKERSIZE_LEGEND))
        marker_labels.append(setting[1])

    legend2 = ax_overall.legend(handles=marker_handles,
        labels=marker_labels,
        fontsize=7,
        ncol=1,
        bbox_to_anchor = [0.59, 0.125], 
        columnspacing=1,
        loc="lower center")
    
    marker_handles = []
    marker_labels = []
    for setting in link_error_zipped_settings:
        marker_handles.append(mpl.lines.Line2D([], [], color=setting[0], linestyle="solid",markersize=MARKERSIZE_LEGEND))
        marker_labels.append(f"{setting[1] * 100}\%")

    legend3 = ax_overall.legend(handles=marker_handles,
        labels=marker_labels,
        fontsize=7,
        ncol=1,
        bbox_to_anchor = [0.1, 0.125],
        columnspacing=1,
        loc="lower center")


    plt.gca().add_artist(legend1)
    plt.gca().add_artist(legend2)


    ax1.set_ylabel("CDF [\%]")
    ax1.set_ylim(-0.01,1.01)
    ax1.set_yticks([0,0.25,0.5,0.75,1])
    ax1.set_yticklabels([0,25,50,75,100])
    ax2.set_ylim(-0.01,1.01)
    ax2.set_yticks([0,0.25,0.5,0.75,1])
    ax2.set_yticklabels([])

    labelpad=0
    ax1.set_xlabel("One link, different loss.")
    ax2.set_xlabel("Multiple links, 1\% loss.")
    ax_overall.set_xlabel("Packet loss difference [\%]" if abs_or_rel == "abs" else "Relative packet loss difference", labelpad=10)


    figname= os.path.join(plot_output_folder,f"{plot_config['plot_basename']}_packetloss_q_COMBINED_{traffic_ratio}_{loss_rate}_{ping_interval}_{ping_packet}_{flow_selection}_{ping_scenario}_{abs_or_rel}_{'with_actual' if with_actual else 'all_links'}{localizationMethod}.{'pdf' if overall_plot_config['pdf_mode'] else 'png'}")
    fig.savefig(figname)

    plt.close()


if __name__ == "__main__":

    overall_plot_config = read_plot_config("plot-config-ANRW-Complete.json")


    available_latency_plots = ["do_baseline_latency_plot", "do_baseline_comparison_latency_plot"]
    if not (overall_plot_config["latency_plot_MAIN_SWITCH"] and any([overall_plot_config[plot] for plot in available_latency_plots])):
        print("Skip latency plots.")
    else:
        print("Do latency plots.")

        plot_config = read_plot_config(plot_config_path=overall_plot_config["latency_plot_config"])
        analysis_configuration = read_analysis_config(plot_config=plot_config)
        plot_output_folder, plot_cache_folder = create_directories(plot_config=overall_plot_config, analysis_configuration=analysis_configuration)

        df_filtered_results = pd.DataFrame()
        load_full_df_filtered_results = False
    
        if not overall_plot_config["use_plot_cache"]:
            load_full_df_filtered_results = True
        else:
            if overall_plot_config["do_baseline_latency_plot"]:
                if not os.path.exists(os.path.join(plot_output_folder, "do_baseline_latency_plot.pkl")):
                    load_full_df_filtered_results = True
            if overall_plot_config["do_baseline_comparison_latency_plot"]:
                if not os.path.exists(os.path.join(plot_output_folder, "do_baseline_comparison_latency_plot.pkl")):
                    load_full_df_filtered_results = True

        if load_full_df_filtered_results:
            if overall_plot_config["USE_GROUNDTRUTH_DELAY"]:
                df_filtered_results = read_df(plot_config, 
                                                overall_plot_config,
                                                analysis_configuration, 
                                                plot_cache_folder, 
                                                plot_cachefile_name_key="plot_cachefile_as_experienced",
                                                df_savefile_name_key="df_savefile_as_experienced_name", 
                                                use_configured_delay=False)
            else:
                df_filtered_results = read_df(plot_config, 
                                                overall_plot_config,
                                                analysis_configuration, 
                                                plot_cache_folder, 
                                                plot_cachefile_name_key="plot_cachefile_as_configured",
                                                df_savefile_name_key="df_savefile_as_configured_name", 
                                                use_configured_delay=True)

        if overall_plot_config["do_baseline_latency_plot"]:
            do_baseline_latency_plot(df_filtered_results, plot_output_folder, 
                                     traffic_ratio=overall_plot_config["baseline_latency_traffic_ratio"], ping_interval=overall_plot_config["baseline_latency_ping_interval"], ping_packet=overall_plot_config["baseline_latency_ping_packet"], ping_scenario=overall_plot_config["baseline_ping_scenario"], flow_selection=overall_plot_config["baseline_flow_selection_strategy"],
                                     abs_or_rel="rel", with_groundtruth=overall_plot_config["USE_GROUNDTRUTH_DELAY"],
                                     localizationMethod=overall_plot_config["baseline_locMethod"])
            
        if overall_plot_config["do_baseline_comparison_latency_plot"]:
            do_baseline_comparison_latency_plot(df_filtered_results, plot_output_folder, 
                                     traffic_ratio=overall_plot_config["baseline_latency_traffic_ratio"], ping_interval=overall_plot_config["baseline_latency_ping_interval"], ping_packet=overall_plot_config["baseline_latency_ping_packet"], ping_scenario=overall_plot_config["baseline_ping_scenario"], flow_selection=overall_plot_config["baseline_flow_selection_strategy"],
                                     abs_or_rel="rel", with_groundtruth=overall_plot_config["USE_GROUNDTRUTH_DELAY"],
                                     localizationMethod=overall_plot_config["baseline_comparison_latency_plot_locMethod"],
                                     cachefile_base_name="do_baseline_comparison_latency_plot.pkl")          


    available_loss_plots = ["do_baseline_loss_plot", "do_qbit_loss_combined_multi_link_packet_loss_ratio_plot"]
    if not (overall_plot_config["loss_plot_MAIN_SWITCH"] and any([overall_plot_config[plot] for plot in available_loss_plots])):
        print("Skip loss plots.")
    else:
        print("Do loss plots.")

        plot_config = read_plot_config(plot_config_path=overall_plot_config["loss_plot_config"])
        analysis_configuration = read_analysis_config(plot_config=plot_config)
        plot_output_folder, plot_cache_folder = create_directories(plot_config=overall_plot_config, analysis_configuration=analysis_configuration)
        
        plot_config_multilink = read_plot_config(plot_config_path=overall_plot_config["loss_plot_config_multilink"])
        analysis_configuration_multilink = read_analysis_config(plot_config=plot_config_multilink)
        plot_output_folder_multilink, plot_cache_folder_multilink = create_directories(plot_config=overall_plot_config, analysis_configuration=analysis_configuration_multilink)


        df_filtered_results = pd.DataFrame()
        df_filtered_results_multilink = pd.DataFrame()
        load_full_df_filtered_results = False
        load_full_df_filtered_results_multilink = False
    
        if not overall_plot_config["use_plot_cache"]:
            load_full_df_filtered_results = True
        else:
            if overall_plot_config["do_baseline_loss_plot"]:
                if not os.path.exists(os.path.join(plot_output_folder, "do_baseline_loss_plot.pkl")):
                    load_full_df_filtered_results = True
            if overall_plot_config["do_qbit_loss_combined_multi_link_packet_loss_ratio_plot"]:
                if not os.path.exists(os.path.join(plot_output_folder, "do_qbit_loss_combined_multi_link_packet_loss_ratio_plot_one.pkl")):
                    load_full_df_filtered_results_multilink = True
                if not os.path.exists(os.path.join(plot_output_folder, "do_qbit_loss_combined_multi_link_packet_loss_ratio_plot_two.pkl")):
                    load_full_df_filtered_results = True

        if load_full_df_filtered_results:
            if overall_plot_config["USE_GROUNDTRUTH_DELAY"]:

                df_filtered_results = read_df(plot_config, 
                                            overall_plot_config, 
                                            analysis_configuration, 
                                            plot_cache_folder, 
                                            plot_cachefile_name_key="plot_cachefile_as_configured",
                                            df_savefile_name_key="df_savefile_as_configured_name", 
                                            use_configured_delay=False)
            else:

                df_filtered_results = read_df(plot_config,
                                            overall_plot_config, 
                                            analysis_configuration,
                                            plot_cache_folder, 
                                            plot_cachefile_name_key="plot_cachefile_as_experienced",
                                            df_savefile_name_key="df_savefile_as_experienced_name", 
                                            use_configured_delay=True)

        if load_full_df_filtered_results_multilink:
            if overall_plot_config["USE_GROUNDTRUTH_DELAY"]:

                df_filtered_results_multilink = read_df(plot_config_multilink, 
                                            overall_plot_config, 
                                            analysis_configuration_multilink, 
                                            plot_cache_folder_multilink, 
                                            plot_cachefile_name_key="plot_cachefile_as_configured",
                                            df_savefile_name_key="df_savefile_as_configured_name", 
                                            use_configured_delay=False)
            else:

                df_filtered_results_multilink = read_df(plot_config_multilink,
                                            overall_plot_config, 
                                            analysis_configuration_multilink,
                                            plot_cache_folder_multilink, 
                                            plot_cachefile_name_key="plot_cachefile_as_experienced",
                                            df_savefile_name_key="df_savefile_as_experienced_name", 
                                            use_configured_delay=True)

        if overall_plot_config["do_baseline_loss_plot"]:

            do_baseline_loss_plot(df_filtered_results, plot_output_folder, 
                                  traffic_ratio=overall_plot_config["baseline_loss_traffic_ratio"], ping_interval=overall_plot_config["baseline_loss_ping_interval"], ping_packet=overall_plot_config["baseline_loss_ping_packet"], loss_rate=overall_plot_config["baseline_loss_loss_rate"], failed_links=overall_plot_config["baseline_loss_failed_links"],
                                  abs_or_rel="rel", with_actual=True,
                                  localizationMethod=overall_plot_config["baseline_loss_locMethod"])

        if overall_plot_config["do_qbit_loss_combined_multi_link_packet_loss_ratio_plot"]:

            do_qbit_loss_combined_multi_link_packet_loss_ratio_plot(df_filtered_results, 
                                                                    df_filtered_results_multilink,
                                                                    plot_output_folder, 
                                  traffic_ratio=overall_plot_config["baseline_loss_traffic_ratio"], ping_interval=overall_plot_config["baseline_loss_ping_interval"], ping_packet=overall_plot_config["baseline_loss_ping_packet"], loss_rate=overall_plot_config["baseline_loss_loss_rate"],
                                  abs_or_rel="rel", with_actual=True)