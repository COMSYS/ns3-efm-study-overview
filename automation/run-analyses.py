import sys
import os
from pathlib import Path
import json
from argparse import ArgumentParser
from multiprocessing import Pool
from queue import Queue
import datetime
import natsort

simulation_automation_folder = os.path.dirname(os.path.realpath(__file__))
meta_configs_folder = os.path.join(simulation_automation_folder, "configs-meta")
analysis_configs_folder = os.path.join(simulation_automation_folder, "analysis", "configs-analysis")

ns3_parent_folder = os.path.join(os.path.dirname(simulation_automation_folder))

sys.path.append(simulation_automation_folder)

from execute_simulation_pipeline import main as start_analysis

if __name__ == "__main__":


    parser = ArgumentParser(description="ns-3 Analysis Automation")
    parser.add_argument('--meta_config', '-m',
                        dest="meta_config",
                        action="store",
                        help="Which meta config to use for executing the analysis?",
                        default="config-meta-analysis-default.json")
    args = parser.parse_args()


    meta_config_path = os.path.join(meta_configs_folder, args.meta_config)

    print(f"Use the following meta config: {meta_config_path}")
    jsonfile = open(meta_config_path)
    meta_config = json.load(jsonfile)
    jsonfile.close()


    all_config_files = meta_config["which_config_files"]

    ## Check if all config files exist
    for config_file_name_prefix in all_config_files:
        config_file = os.path.join(analysis_configs_folder, config_file_name_prefix[0])

        if not os.path.exists(config_file):
            print(f"Config {config_file} does not exist. We do not continue.")
            exit(-1)

    completed_counter = 0
    failed_counter = 0
    skipped_counter = 0
    failed_experiment_args = []
    process_count = 0
    def analysis_complete_callback(return_value):
        global process_count, completed_counter, failed_counter, skipped_counter, failed_experiment_args
        process_count -= 1

        SIM_ID = return_value[-1][-1]
        if not return_value[0][0]:
            print(f"Analysis completed : {SIM_ID}")
            completed_counter += 1
        elif return_value[0][0] == -1 or return_value[0][0] == -42:
            print(f"Analysis did not complete : {SIM_ID}")
            failed_counter+= 1
            failed_experiment_args.append(return_value)
        elif return_value[0][0] == -2:
            print(f"Analysis config was invalid : {SIM_ID}")
            failed_counter+= 1
            failed_experiment_args.append(return_value)
        elif return_value[0][0] == -3:
            print(f"Skipped the evaluation due to overwrite : {SIM_ID}")
            skipped_counter+= 1
        elif return_value[0][0] == "No-Compressed-Results" or return_value[0][0] == "No-Uncompressed-Results":
            print(f"No usable results available : {SIM_ID}")
            failed_counter += 1
            failed_experiment_args.append(return_value)
        else:
            print(f"Analysis completed : {SIM_ID}")
            completed_counter += 1
    
    def start_analysis_process_pool(args):
        print("Start analysis")
        return (start_analysis(args[0],
                            from_cmd_line=args[1],
                            build=args[2],
                            compress=args[3],
                            decompress=args[4],
                            remove_after_compress=args[5],
                            confirm_overwrite=args[6], 
                            debug=args[7],
                            create_dirs=args[8],
                            sim_only=args[9],
                            sim_config_file=args[10],
                            sim_topology_dir=args[11],
                            sim_output_dir=args[12],
                            sim_run_count=args[13],
                            sim_run_offset=args[14],
                            sim_traces_per_file=args[15],
                            sim_trace_events_per_file=args[16],
                            sim_disable_obsv_traces=args[17],
                            sim_disable_host_traces=args[18],
                            sim_enable_on_the_fly_qlog=args[19],
                            analysis_only=args[20],
                            analysis_config_file=args[21],
                            analysis_input_dir=args[22], 
                            analysis_output_dir=args[23],
                            output_path_prefix=args[24]), args)


    start_time = datetime.datetime.now()
    analysis_queue = Queue()    
    for config_file_name_prefix in natsort.humansorted(all_config_files, key=lambda a: a[3]):

        config_file = os.path.join(analysis_configs_folder, config_file_name_prefix[0])

        analysis_queue.put((Path(ns3_parent_folder),
                                False,
                                None,
                                meta_config["compress_results"],
                                True, ##Decompress
                                True, ## Remove after compress
                                meta_config["confirm_overwrite"], 
                                meta_config["debug"],
                                meta_config["create_dirs"],
                                None,
                                None,
                                None,
                                None,
                                meta_config["sim_run_count"],
                                meta_config["sim_run_offset"],
                                None,
                                None,
                                None,
                                None,
                                None,
                                meta_config["analysis_only"],
                                config_file,
                                os.path.join(meta_config["analysis_input_dir"],meta_config["experiment_identifier"]), 
                                os.path.join(meta_config["analysis_output_dir"],config_file_name_prefix[2]),
                                config_file_name_prefix[3]))


    print(f"We have {analysis_queue.qsize()} simulation configs in the queue.")
    overall_todo = analysis_queue.qsize()
    try:
        with Pool(meta_config["num_pool_workers"]) as process_pool:
            while not analysis_queue.empty():
                
                if process_count < meta_config["num_pool_workers"]:
                    analysis_args = analysis_queue.get()
                    print(f"Start analysis with config: {analysis_args}")
                    process_count += 1
                    process_pool.apply_async(start_analysis_process_pool, args=(analysis_args,), callback=analysis_complete_callback)

                    print(30*"XXX")
                    print(f"Completed {completed_counter}/{overall_todo} analysis configurations ({failed_counter} failed, {skipped_counter} skipped). {analysis_queue.qsize()} entries remaining in the queue")
                    print(30*"XXX")


            print("Started all")
                    
            while process_count != 0:
                continue

    except Exception as e:
        print("Capital error in the experiments. Store the analysis configs that have not yet been executed.")

        while not analysis_queue.empty():
            analysis_args = analysis_queue.get()
            print("These are not finished")
            

    print(f"Overall loop complete. {completed_counter}/{overall_todo} were complete, {failed_counter} failed, {skipped_counter} were skipped.")
    duration = datetime.datetime.now()-start_time
    print(f"All {overall_todo} analysis steps took {duration}.")

    if len(failed_experiment_args):
        print("Information on the failed experiments.")

        for failed_experiment in failed_experiment_args:
            print(failed_experiment)