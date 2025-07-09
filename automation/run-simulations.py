import sys
import os
from pathlib import Path
import json
from argparse import ArgumentParser
from multiprocessing import Pool
from queue import Queue
import datetime

simulation_automation_folder = os.path.dirname(os.path.realpath(__file__))
meta_configs_folder = os.path.join(simulation_automation_folder, "configs-meta")
sim_configs_folder = os.path.join(simulation_automation_folder, "simulation", "configs-sim")
topology_folder = os.path.join(simulation_automation_folder, "topologies")

ns3_parent_folder = os.path.join(os.path.dirname(simulation_automation_folder))

sys.path.append(simulation_automation_folder)

from execute_simulation_pipeline import main as start_simulation

if __name__ == "__main__":


    parser = ArgumentParser(description="ns-3 Simulation Automation")
    parser.add_argument('--meta_config', '-m',
                        dest="meta_config",
                        action="store",
                        help="Which meta config to use for executing the simulations?",
                        default="config-meta-sim-default.json")
    args = parser.parse_args()


    meta_config_path = os.path.join(meta_configs_folder, args.meta_config)

    print(f"Use the following meta config: {meta_config_path}")
    jsonfile = open(meta_config_path)
    meta_config = json.load(jsonfile)
    jsonfile.close()


    all_config_files = meta_config["which_config_files"]

    ## Check if all config files exist
    for config_file_name_prefix in all_config_files:
        config_file = os.path.join(sim_configs_folder, config_file_name_prefix[0])

        if not os.path.exists(config_file):
            print(f"Config {config_file} does not exist. We do not continue.")
            exit(-1)

    completed_counter = 0
    failed_counter = 0
    skipped_counter = 0
    failed_experiment_args = []
    process_count = 0
    def simulation_complete_callback(return_value):
        global process_count, completed_counter, failed_counter, failed_experiment_args, skipped_counter
        process_count -= 1        
        SIM_ID = return_value[-1][-1]
        RETURN_VALUE = return_value[0][0]
        CURRENT_STATUS = "Current status: {} completed simulation configurations ({} failed, {} skipped.)"
        if RETURN_VALUE == -1:
            failed_counter += 1
            failed_experiment_args.append(return_value[1])
            print(f"Simulation did not complete ({SIM_ID}). " + CURRENT_STATUS.format(completed_counter, failed_counter, skipped_counter))
        elif RETURN_VALUE == -2:
            failed_counter += 1
            failed_experiment_args.append(return_value[1])
            print(f"Simulation did not complete ({SIM_ID}). " + CURRENT_STATUS.format(completed_counter, failed_counter, skipped_counter))
        elif RETURN_VALUE == -3:
            skipped_counter += 1
            print(f"Skipped simulation ({SIM_ID}). " + CURRENT_STATUS.format(completed_counter, failed_counter, skipped_counter))
        else:
            completed_counter += 1
            print(f"Simulation completed ({SIM_ID}). " + CURRENT_STATUS.format(completed_counter, failed_counter, skipped_counter))
        
    
    def start_simulation_process_pool(args):
        print("Start simulation")
        return (start_simulation(args[0],
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
    simulation_queue = Queue()
    for config_file_name_prefix in all_config_files:

        config_file = os.path.join(sim_configs_folder, config_file_name_prefix[0])

        simulation_queue.put((Path(ns3_parent_folder),
                                False,
                                meta_config["build"],
                                meta_config["compress_results"],
                                True, ## Decompress
                                True, ## Remove after compress
                                meta_config["confirm_overwrite"], 
                                meta_config["debug"],
                                meta_config["create_dirs"],
                                meta_config["sim_only"],
                                config_file,
                                topology_folder,
                                os.path.join(meta_config["sim_output_dir"], meta_config["experiment_identifier"]),
                                meta_config["sim_run_count"],
                                meta_config["sim_run_offset"],
                                meta_config["sim_traces_per_file"],
                                meta_config["sim_trace_events_per_file"],
                                meta_config["sim_disable_obsv_traces"] if "sim_disable_obsv_traces" in meta_config.keys() else None,
                                meta_config["sim_disable_host_traces"],
                                meta_config["sim_enable_on_the_fly_qlog"],
                                None,None,None, None, ## analysis-related stuff
                                config_file_name_prefix[1]))


    print(f"We have {simulation_queue.qsize()} simulation configs in the queue.")
    overall_todo = simulation_queue.qsize()
    try:
        with Pool(meta_config["num_pool_workers"]) as process_pool:
            while not simulation_queue.empty():
                
                if process_count < meta_config["num_pool_workers"]:
                    simulation_args = simulation_queue.get()
                    print(f"Start simulation with config: {simulation_args}")
                    process_count += 1
                    process_pool.apply_async(start_simulation_process_pool, args=(simulation_args,), callback=simulation_complete_callback)
                    print(f"We have {simulation_queue.qsize()} entries remaining in the queue.")

                    print(30*"XXX")
                    print(30*"XXX")
                    print(f"Completed {completed_counter}/{overall_todo} simulation configurations ({failed_counter} failed, {skipped_counter} skipped).")
                    print(30*"XXX")
                    print(30*"XXX")

            print("Started all")
                    
            ### Wait until everything complete
            while process_count != 0:
                continue

    except Exception as e:
        print("Capital error in the experiments. Store the simulation configs that have not yet been executed.")

        while not simulation_queue.empty():
            simulation_args = simulation_queue.get()
            failed_experiment_args.append(simulation_args)
            print("These are not finished")
            
        with open(os.path.join(overview_folder, meta_config["experiment_identifier"] + "_emergency_dump.json"), "r") as emergency_file:
            json.dump(failed_experiment_args, emergency_file)

    print(f"Overall loop complete. {completed_counter}/{overall_todo} were complete, {failed_counter} failed, {skipped_counter} skipped.")
    duration = datetime.datetime.now()-start_time
    print(f"All {overall_todo} simulations took {duration}.")

                            
                            
                            
                            
                            







    
