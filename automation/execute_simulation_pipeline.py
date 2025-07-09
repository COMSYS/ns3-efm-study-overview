import subprocess
import sys
import os
from pathlib import Path
import argparse
import time

print_prefix = "[PIPELINE]"
RETURNCODE_OVERWRITE = -3

def performSimulation(sim_path: Path, config_path: Path, output_path: Path, from_cmd_line: bool, args):
    
    config_val_path = sim_path.joinpath("scratch/EFM-Localization-Stuff/")

    try:
        if args.debug:
            print(print_prefix, "Validating simulation config file with command:", "'python3 config-validator.py", str(config_path) + "'", "in directory", config_val_path)
        subprocess.run(["python3", "config-validator.py", str(config_path)], cwd=config_val_path, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        print(print_prefix, "Error: Simulation config file is invalid. Show error? (y/n)")
        if not from_cmd_line: 
            print(f"Config file: {str(config_path)}")
            print("============\n", e.stdout.decode("utf-8"), "\n============")
            return -2
        if input().lower() == "y":
            print("============\n", e.stdout.decode("utf-8"), "\n============")
        sys.exit(1)

    print(print_prefix, "Starting simulation...")
    cmd = ["./ns3", "run", "efm-localization-test", "--"]
    cmd.append("--qlogFilePrefix=" + args.outputPathPrefix)
    cmd.append("--configFile=" + str(config_path))
    cmd.append("--outputDir=" + str(output_path))

    if args.sim_topology_dir is not None:
        cmd.append("--topologyDir=" + args.sim_topology_dir)
    if args.sim_run_count is not None:
        cmd.append("--numberOfRuns=" + str(args.sim_run_count))
    if args.sim_run_offset is not None:
        cmd.append("--runNumberOffset=" + str(args.sim_run_offset))
    if args.sim_traces_per_file is not None:
        cmd.append("--tracesPerFile=" + str(args.sim_traces_per_file))
    if args.sim_trace_events_per_file is not None:
        cmd.append("--traceEventsPerFile=" + str(args.sim_trace_events_per_file))
    if args.sim_disable_obsv_traces:
        cmd.append("--disableObsvTraces")
    if args.sim_disable_host_traces:
        cmd.append("--disableHostTraces")
    if args.sim_enable_onthefly_qlog:
        cmd.append("--enableOnTheFlyQLog")

    if args.debug:
        print(print_prefix, "Running simulation with command:", "'"+ " ".join(cmd) + "'", "in directory", sim_path)
    start = time.time()
    subprocess.run(cmd, cwd=sim_path, check=True)
    end = time.time()
    print(print_prefix, f"Simulation finished in {(end - start):.2f} seconds.")

def performAnalysis(analysis_path: Path, config_path: Path, sim_input_path: Path, output_path: Path, from_cmd_line: bool, args):
    config_val_path = analysis_path.joinpath("src/analysis")
    if not config_val_path.exists():
        print(print_prefix, "Error: Analysis config validator not found.")
        sys.exit(1)

    try:
        if args.debug:
            print(print_prefix, "Validating analysis config file with command:", "'python3 config-validator.py", str(config_path) + "'", "in directory", config_val_path)
        subprocess.run(["python3", "config-validator.py", str(config_path)], cwd=config_val_path, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        print(print_prefix, "Error: Analysis config file is invalid. Show error? (y/n)")
        if not from_cmd_line: 
            print(f"Config file: {str(config_path)}")
            print("============\n", e.stdout.decode("utf-8"), "\n============")
            return -2
        if input().lower() == "y":
            print("============\n", e.stdout.decode("utf-8"), "\n============")
        sys.exit(1)

    build_path = analysis_path.joinpath("build/Release")
    if args.build:
        print(print_prefix, "Building result processor...")
        if not build_path.exists():
            if args.debug:
                print(print_prefix, "Creating build directory:", str(build_path))
            build_path.mkdir(parents=True)
            if args.debug:
                print(print_prefix, "Running cmake with command:", "'cmake -DCMAKE_BUILD_TYPE=Release ../..' in directory", build_path)
            subprocess.run(["cmake", "-DCMAKE_BUILD_TYPE=Release", "../.."], cwd=build_path, check=True)
        if args.debug:
            print(print_prefix, "Running cmake with command:", "'cmake --build .' in directory", build_path)
        subprocess.run(["cmake", "--build", "."], cwd=build_path, check=True)
        print(print_prefix, "Build finished.")
    if not build_path.exists():
        print("Error: result processor not built.")
        sys.exit(1)
    cmd = ["./build/Release/src/EfmSimProcessor", args.outputPathPrefix]
    
    cmd.extend(["-c", str(config_path)])
    cmd.extend(["-s", str(sim_input_path)])
    cmd.extend(["-a", str(output_path)])
    print(print_prefix, "Starting result processing...")
    if args.debug:
        print(print_prefix, "Running result processing with command:", "'" + " ".join(cmd) + "'", "in directory", analysis_path)
    start = time.time()
    try:
        subprocess.run(cmd, cwd=analysis_path, check=True)
    except Exception as e:
        print(print_prefix, f"Result processing crashed for {str(config_path)}, {str(sim_input_path)}, {str(output_path)}. Reason: {e}")
        return -42
    end = time.time()
    print(print_prefix, f"Result processing finished in {(end - start):.2f} seconds.")

def performCompression(sim_output_path, file_prefix, remove_after_compress, run_numbers, debug=False):
    print(print_prefix, "Compressing simulation results...")
    for run_number in run_numbers:
        ### Raw .json --> old variant
        raw_file_pattern = f"{file_prefix}-{run_number}.*json"
        files = list(sim_output_path.glob(raw_file_pattern))
        if files:
            for file in files:
                json_file = file.name
                tar_file = str(json_file).replace("json", "tar.gz")
                if os.path.isfile(os.path.join(sim_output_path, tar_file)):
                    if debug:
                        print(f"{tar_file} already exists. Do not compress twice.")
                    if remove_after_compress:
                        if debug:
                            print("Remove any source files that might still exist")
                        cmd = ["rm", json_file]
                        subprocess.run(cmd, cwd=sim_output_path, check=True)
                else:
                    cmd = ["tar", "-czf", tar_file, json_file]
                    if remove_after_compress:
                        cmd.append("--remove-files")
                    subprocess.run(cmd, cwd=sim_output_path, check=True)

        raw_file_pattern = f"{file_prefix}-{run_number}.*.tar.gz"
        files = list(sim_output_path.glob(raw_file_pattern))
        if files:
            tar_file = f"{file_prefix}-{run_number}.tar.gz"
            if os.path.isfile(os.path.join(sim_output_path, tar_file)):
                if debug:
                    print(f"{tar_file} already exists. Do not compress twice.")
                if remove_after_compress:
                    if debug:
                        print("Remove any source files that might still exist")
                    cmd = ["rm"]
                    cmd.extend(map(lambda x: x.name, files))
                    subprocess.run(cmd, cwd=sim_output_path, check=True)
            else:
                cmd = ["tar", "-czf", f"{file_prefix}-{run_number}.tar.gz"]
                if remove_after_compress:
                    cmd.append("--remove-files")
                cmd.extend(map(lambda x: x.name, files))
                subprocess.run(cmd, cwd=sim_output_path, check=True)
    print(print_prefix, "Compression finished.")

def performDecompression(analysis_input_path, file_prefix, remove_after_decompress, overwrite_existing, run_numbers, debug=False):
    print(print_prefix, "Decompressing simulation results...")    
    for run_number in run_numbers:

        outer_tar_pattern = f"{file_prefix}-{run_number}.tar.gz"
        inner_tar_pattern = f"{file_prefix}-{run_number}.*.tar.gz"

        if debug:
            print("Check outer compression")
        ### Check outer compression
        files = list(analysis_input_path.glob(outer_tar_pattern))
        if files:
            if not overwrite_existing:
                if not list(analysis_input_path.glob(inner_tar_pattern)):
                    cmd = ["tar", "-xzkf", outer_tar_pattern]
                else:
                    print("Inner tar files exist. Cannot decompress without overwriting. [1]")
                    continue
            else:
                cmd = ["tar", "-xzf", outer_tar_pattern]
            subprocess.run(cmd, cwd=analysis_input_path, check=True)
            if remove_after_decompress:
                cmd = ["rm", outer_tar_pattern]
                subprocess.run(cmd, cwd=analysis_input_path, check=True)

        if debug:
            print("Check inner compression")
        ### Check inner compression
        files = list(analysis_input_path.glob(inner_tar_pattern))
        if files:
            for file in files:
                if not overwrite_existing:
                    if os.path.exists(os.path.join(analysis_input_path, file.name.replace(".tar.gz", "json"))):
                        print("Json files exist. Cannot decompress without overwriting. [2]")
                        continue
                    cmd = ["tar", "-xzkf"]
                else:
                    cmd = ["tar", "-xzf"]
                subprocess.run(cmd + [file.name], cwd=analysis_input_path, check=True)
            if remove_after_decompress:
                cmd = ["rm"]
                cmd.extend(map(lambda x: x.name, files))
                subprocess.run(cmd, cwd=analysis_input_path, check=True)

    print(print_prefix, "Decompression finished.")

def main(ns3_parent_folder_path, *,
         from_cmd_line=True,
         build=None,
         compress=None,
         decompress=None,
         remove_after_compress=None,
         confirm_overwrite=None,
         debug=None,
         create_dirs=None,
         sim_only=None,
         sim_config_file=None,
         sim_topology_dir=None,
         sim_output_dir=None,
         sim_run_count=None,
         sim_run_offset=None,
         sim_traces_per_file=None,
         sim_trace_events_per_file=None,
         sim_disable_obsv_traces=None,
         sim_disable_host_traces=None,
         sim_enable_on_the_fly_qlog=None,
         analysis_only=None,
         analysis_config_file=None,
         analysis_input_dir=None,
         analysis_output_dir=None,
         output_path_prefix=None):

    sim_path = ns3_parent_folder_path.joinpath("ns-3-dev-fork")
    analysis_path = ns3_parent_folder_path.joinpath("raw-result-processing")

    if not sim_path.exists():
        print("Error: ns-3-dev-fork folder not found.")
        sys.exit(1)

    if not analysis_path.exists():
        print("Error: raw-result-processing folder not found.")
        sys.exit(1)


    parser = argparse.ArgumentParser()

    parser.add_argument("--build", "-b", action="store_true", help="Build result processor before running (ns3 is always built)")
    parser.add_argument("--compress", "-c", action="store_true", help="Compress simulation results after processing")
    parser.add_argument("--decompress", "-x", action="store_true", help="Decompress simulation results before processing if necessary")
    parser.add_argument("--remove-after-compress", "-r", action="store_true", help="Remove uncompressed simulation results after compressing")
    parser.add_argument("--confirm-overwrite", "-y", action="store_true", help="Confirm that existing files may be overwritten without asking")
    parser.add_argument("--debug", "-d", action="store_true", help="Enable debug output")
    parser.add_argument("--create_dirs", action="store_true", help="Create result directories if they do not exist")

    # Simulation related arguments
    parser.add_argument("--sim-only", action="store_true", help="Only run simulation, do not process results")
    parser.add_argument("--sim-config-file", help="Path and or name of simulation config file to use", default="scratch/EFM-Localization-Stuff/config.json")
    parser.add_argument("--sim-topology-dir", help="Path to topology directory")
    parser.add_argument("--sim-output-dir", help="Path to output directory", default="output/")
    parser.add_argument("--sim-run-count", type=int, help="Number of simulation runs to perform", default=1)
    parser.add_argument("--sim-run-offset", type=int, help="Offset to start simulation run numbering from", default=0)
    parser.add_argument("--sim-traces-per-file", type=int, help="Number of traces per output file (0 = no limit)")
    parser.add_argument("--sim-trace-events-per-file", type=int, help="Number of trace events per output file (0 = no limit)")
    parser.add_argument("--sim-disable-obsv-traces", action="store_true", help="Disable observer traces")
    parser.add_argument("--sim-disable-host-traces", action="store_true", help="Disable host traces")
    parser.add_argument("--sim-enable-onthefly-qlog", action="store_true", help="Enable on-the-fly qlog generation")

    # Analysis related arguments
    parser.add_argument("--analysis-only", action="store_true", help="Only analyze results (they have to exist already), do not run simulation")
    parser.add_argument("--analysis-config-file", help="Name of analysis config file to use", default="./data/analysis-config.json")
    parser.add_argument("--analysis-input-dir", help="Path to simulation output directory")
    parser.add_argument("--analysis-output-dir", help="Prefix of simulation output files", default="./data/analysis-results/")

    parser.add_argument("outputPathPrefix", help="Output path prefix for simulation results and processed results, e.g., dfn/download/all-to-all-50MB")

    args = None
    if from_cmd_line:
        args = parser.parse_args()
    else:
        arg_list_adaptor = []
        if build:
            arg_list_adaptor.append("--build")
        if compress:
            arg_list_adaptor.append("--compress")
        if decompress:
            arg_list_adaptor.append("--decompress")
        if remove_after_compress:
            arg_list_adaptor.append("--remove-after-compress")
        if confirm_overwrite:
            arg_list_adaptor.append("--confirm-overwrite")
        if debug:
            arg_list_adaptor.append("--debug")
        if create_dirs:
            arg_list_adaptor.append("--create_dirs")

        if sim_only:
            arg_list_adaptor.append("--sim-only")
        if sim_config_file:
            arg_list_adaptor.append("--sim-config-file")
            arg_list_adaptor.append(str(sim_config_file))
        if sim_topology_dir:
            arg_list_adaptor.append("--sim-topology-dir")
            arg_list_adaptor.append(str(sim_topology_dir))
        if sim_output_dir:
            arg_list_adaptor.append("--sim-output-dir")
            arg_list_adaptor.append(str(sim_output_dir))
        if sim_run_count:
            arg_list_adaptor.append("--sim-run-count")
            arg_list_adaptor.append(str(sim_run_count))
        if sim_run_offset:
            arg_list_adaptor.append("--sim-run-offset")
            arg_list_adaptor.append(str(sim_run_offset))
        if sim_traces_per_file:
            arg_list_adaptor.append("--sim-traces-per-file")
            arg_list_adaptor.append(str(sim_traces_per_file))
        if sim_trace_events_per_file:
            arg_list_adaptor.append("--sim-trace-events-per-file")
            arg_list_adaptor.append(str(sim_trace_events_per_file))
        if sim_disable_obsv_traces:
            arg_list_adaptor.append("--sim-disable-obsv-traces")
        if sim_disable_host_traces:
            arg_list_adaptor.append("--sim-disable-host-traces")
        if sim_enable_on_the_fly_qlog:
            arg_list_adaptor.append("--sim-enable-onthefly-qlog")

        if analysis_only:
            arg_list_adaptor.append("--analysis-only")
        if analysis_config_file:
            arg_list_adaptor.append("--analysis-config-file")
            arg_list_adaptor.append(str(analysis_config_file))
        if analysis_input_dir:
            arg_list_adaptor.append("--analysis-input-dir")
            arg_list_adaptor.append(str(analysis_input_dir))
        if analysis_output_dir:
            arg_list_adaptor.append("--analysis-output-dir")
            arg_list_adaptor.append(str(analysis_output_dir))
        if output_path_prefix:
            arg_list_adaptor.append(str(output_path_prefix))

        print("Simulation commands: ", arg_list_adaptor)
        args = parser.parse_args(arg_list_adaptor)


    if args.sim_only and args.analysis_only:
        parser.error("--sim-only and --analysis-only are mutually exclusive.")

    if Path(args.outputPathPrefix).is_absolute():
        parser.error("outputPathPrefix must not be an absolute path.")
    
    # ----- Check sim config file -----
    if Path(args.sim_config_file).suffix != ".json":
        parser.error("sim-config-file must be a json file.")
    
    # We allow the config file to be specified as a file name only, in which case we assume it is in the scratch folder
    if len(Path(args.sim_config_file).parts) == 1:
        sim_config_path = sim_path.joinpath("scratch/EFM-Localization-Stuff/" + args.sim_config_file)
    elif Path(args.sim_config_file).is_absolute():
        sim_config_path = Path(args.sim_config_file)
    else:
        sim_config_path = sim_path.joinpath(args.sim_config_file)

    if not sim_config_path.exists() and sim_config_path.is_file():
        print(print_prefix, "Error: Simulation config file not found.")
        sys.exit(1)

    # ----- Check analysis config file -----
    if Path(args.analysis_config_file).suffix != ".json":
        parser.error("analysis-config-file must be a json file.")

    if Path(args.analysis_config_file).is_absolute():
        analysis_config_path = Path(args.analysis_config_file)
    else:
        analysis_config_path = analysis_path.joinpath(args.analysis_config_file)

    if not analysis_config_path.exists() and analysis_config_path.is_file():
        parser.error(f"analysis-config-file {analysis_config_path} does not exist.")

    # ----- Check / Create simulation output dir -----

    if Path(args.sim_output_dir).is_absolute():
        output_path_sim = Path(args.sim_output_dir)
    else:
        output_path_sim = sim_path.joinpath(args.sim_output_dir)

    if not output_path_sim.exists():
        if args.create_dirs:
            os.makedirs(output_path_sim, exist_ok=True)
        if not output_path_sim.exists():
            parser.error(f"Simulation output directory {output_path_sim} does not exist.")

    # The outputPathPrefix contains the filePrefix, so use .parent to get the output directory
    output_path_sim_with_prefix = output_path_sim.joinpath(args.outputPathPrefix).parent

    if not output_path_sim_with_prefix.exists():
        if args.debug:
            print(print_prefix, f"Creating simulation output directory {output_path_sim_with_prefix}")
        output_path_sim_with_prefix.mkdir(parents=True)

    # ----- Check analysis input dir -----
    if not args.analysis_only and args.analysis_input_dir is not None:
        parser.error("--analysis-input-dir can only be used with --analysis-only.") 

    if args.analysis_input_dir is None:
        assert output_path_sim.is_absolute()
        input_path_analysis = output_path_sim
    else:
        if Path(args.analysis_input_dir).is_absolute():
            input_path_analysis = Path(args.analysis_input_dir)
        else:
            input_path_analysis = analysis_path.joinpath(args.analysis_input_dir)

    if not input_path_analysis.exists():
        parser.error(f"analysis-input-dir {input_path_analysis} does not exist.")
    
    # The outputPathPrefix contains the filePrefix, so use .parent to get the input directory
    input_path_analysis_with_prefix = input_path_analysis.joinpath(args.outputPathPrefix).parent

    # If we are running the analysis only, then the input directory including the prefix must exist
    if args.analysis_only and not input_path_analysis_with_prefix.exists():
        parser.error(f"analysis-input-dir {input_path_analysis_with_prefix} does not exist.")

    # ----- Check analysis output dir -----
    if Path(args.analysis_output_dir).is_absolute():
        output_path_analysis = Path(args.analysis_output_dir)
    else:
        output_path_analysis = analysis_path.joinpath(Path(args.analysis_output_dir))

    if not output_path_analysis.exists():
        if args.create_dirs:
            os.makedirs(output_path_analysis, exist_ok=True)
        if not output_path_analysis.exists():
            parser.error(f"analysis-output-dir {output_path_analysis} does not exist.")

    # The outputPathPrefix contains the filePrefix, so use .parent to get the output directory
    output_path_analysis_with_prefix = output_path_analysis.joinpath(args.outputPathPrefix).parent

    if not output_path_analysis_with_prefix.exists():
        if args.debug:
            print(print_prefix, f"Creating analysis output directory {output_path_analysis_with_prefix}")
        output_path_analysis_with_prefix.mkdir(parents=True)


    # ----- Check if simulation or analysis results already exist -----
    file_prefix = Path(args.outputPathPrefix).name

    run_numbers = list(range(args.sim_run_offset, args.sim_run_offset + args.sim_run_count))

    if not args.confirm_overwrite:
        # Check if simulation or analysis results already exist
        if not args.analysis_only:
            for run_number in run_numbers:
                if next(output_path_sim_with_prefix.glob(f"{file_prefix}-{run_number}.*json"), None) is not None:
                    print(print_prefix, "Warning: Simulation results with the specified output prefix already exist and will be overwritten. Continue? (y/n)")
                    if not from_cmd_line:
                        print("We don't allow dynamic selection for manually overwriting results with automated simulation execution.")
                        return (RETURNCODE_OVERWRITE, args)
                    if input().lower() != "y":
                        sys.exit(0)
                    break
            for run_number in run_numbers:
                if output_path_sim_with_prefix.joinpath(f"{file_prefix}-{run_number}*.tar.gz").exists():
                    overwritten = " and will be overwritten " if args.compress else ""
                    print(print_prefix, f"Warning: Compressed simulation results with the specified output prefix already exist{overwritten}. Continue? (y/n)")
                    if not from_cmd_line:
                        print("We don't allow dynamic selection for manually overwriting results with automated simulation execution.")
                        return (-1, args)
                    if input().lower() != "y":
                        sys.exit(0)
                    break
            for run_number in run_numbers:
                if output_path_sim_with_prefix.joinpath(f"{file_prefix}-{run_number}.tar.gz").exists():
                    overwritten = " and will be overwritten " if args.compress else ""
                    print(print_prefix, f"Warning: Compressed simulation results with the specified output prefix already exist{overwritten}. Continue? (y/n)")
                    if not from_cmd_line:
                        print("We don't allow dynamic selection for manually overwriting results with automated simulation execution.")
                        return (RETURNCODE_OVERWRITE, args)
                    if input().lower() != "y":
                        sys.exit(0)
                    break
        if not args.sim_only:
            for run_number in run_numbers:
                if next(output_path_analysis_with_prefix.glob(f"analysis-{file_prefix}-{run_number}.*json"), None) is not None:
                    print(print_prefix, "Warning: Analysis results with the specified output prefix already exist and will be overwritten. Continue? (y/n)")
                    if not from_cmd_line:
                        print("We don't allow dynamic selection for manually overwriting results with automated analysis execution.")
                        return (RETURNCODE_OVERWRITE, args)
                    if input().lower() != "y":
                        sys.exit(0)
                    break

    sim_return = None
    if not args.analysis_only:
        sim_return = performSimulation(sim_path, sim_config_path, output_path_sim, from_cmd_line, args)

    # ----- Check if input files for the analysis exist ---------
    # ----- And compute files to be decompressed if necessary ---
    if not args.sim_only:
        run_numbers_to_decompress = []
        for run_number in run_numbers:
            if next(input_path_analysis_with_prefix.glob(f"{file_prefix}-{run_number}.*json"), None) is None:
                if args.decompress:
                    if input_path_analysis_with_prefix.joinpath(f"{file_prefix}-{run_number}.tar.gz").exists():
                        run_numbers_to_decompress.append(run_number)
                    elif input_path_analysis_with_prefix.glob(f"{file_prefix}-{run_number}.*.tar.gz"):
                        run_numbers_to_decompress.append(run_number)
                    else:
                        print(f"Simulation results for run {run_number} do not exist in {input_path_analysis_with_prefix} in compressed or uncompressed form. Aborting start of analysis.")
                        if from_cmd_line:
                            sys.exit(1)
                        return ("No-Compressed-Results", args)
                else:
                    print(f"Simulation results for run {run_number} do not exist in {input_path_analysis_with_prefix} in uncompressed form. Aborting start of analysis.")
                    if from_cmd_line:
                        sys.exit(1)
                    return ("No-Uncompressed-Results", args)

        if args.decompress and args.debug:
            print(print_prefix, f"Simulation runs to be decompressed: {run_numbers_to_decompress}")

        if run_numbers_to_decompress:
            assert args.decompress
            performDecompression(input_path_analysis_with_prefix, file_prefix, remove_after_decompress=False, overwrite_existing=False, run_numbers= run_numbers_to_decompress)
    else:
        print("Skip decompression logic for sim_only.")

    analysis_return = None
    if not args.sim_only:
        analysis_return = performAnalysis(analysis_path, analysis_config_path, input_path_analysis, output_path_analysis, from_cmd_line, args)

    if not args.analysis_only and args.compress: # Compress simulation results
        performCompression(output_path_sim_with_prefix, file_prefix, args.remove_after_compress, run_numbers, args.debug)
    elif args.analysis_only and args.compress: # Compress files in analysis input directory
        performCompression(input_path_analysis_with_prefix, file_prefix, args.remove_after_compress, run_numbers, args.debug)

    if not from_cmd_line:

        if not args.analysis_only and not args.sim_only:
            return (sim_return, analysis_return, args)

        elif not args.analysis_only:
            return (sim_return, args)
        
        elif not args.sim_only:
            return (analysis_return, args)
        
if __name__ == "__main__":
    main(ns3_parent_folder_path=Path(os.path.realpath(sys.path[0])).parent)
