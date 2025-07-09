from data_manager import (
    SimRunResultSet,
    LocalizationMethod,
    EfmBit,
    ClassificationMode
)
import subprocess
import os
from pathlib import Path
import json

def performDecompression(simulation_results_path, file_prefix, overwrite_existing, run_numbers):
    print("Decompressing simulation results...")
    for run_number in run_numbers:
        filename = f"{file_prefix}-{run_number}.tar.gz"
        files = list(simulation_results_path.glob(filename))
        if files:
            if not overwrite_existing:
                cmd = ["tar", "-xzkf", filename]
            else:
                cmd = ["tar", "-xzf", filename]
            result1 = subprocess.run(cmd, cwd=simulation_results_path, check=True)
        else:
            raise Exception(f"File {filename} not present.")

        ping_route_filename = f"{file_prefix}-{run_number}.0.tar.gz"
        files = list(simulation_results_path.glob(ping_route_filename))
        if files:
            if not overwrite_existing:
                cmd = ["tar", "-xzkf", ping_route_filename]
            else:
                cmd = ["tar", "-xzf", ping_route_filename]
            result2 = subprocess.run(cmd, cwd=simulation_results_path, check=True)
        else:
            raise Exception(f"File {ping_route_filename} not present.")
    print("Decompression finished.")


def removeDecompressed(simulation_results_path, file_prefix, run_numbers):
    print("Removing extracted results...")
    for run_number in run_numbers:
        files = list(simulation_results_path.glob(f"{file_prefix}-{run_number}*.json"))
        if files:
            cmd = ["rm"]
            cmd.extend(map(lambda x: x.name, files))
            subprocess.run(cmd, cwd=simulation_results_path, check=True)
    for run_number in run_numbers:
        files = list(simulation_results_path.glob(f"{file_prefix}-{run_number}.*.tar.gz"))
        if files:
            cmd = ["rm"]
            cmd.extend(map(lambda x: x.name, files))
            subprocess.run(cmd, cwd=simulation_results_path, check=True)
    print("Removing complete.")



def is_loss_bit(bit):
    return bit in [EfmBit.L, 
                    EfmBit.Q, 
                    EfmBit.R, 
                    EfmBit.QR, 
                    EfmBit.QL, 
                    EfmBit.TCPDART, 
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
    LocalizationMethod.LIN_LSQR: "LIN_LSQR"
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
    "LIN_LSQR" : LocalizationMethod.LIN_LSQR
}


classification_mode_lookup = {
    ClassificationMode.STATIC : "static",
    ClassificationMode.PERFECT : "perfect",
}    
classification_mode_reverse_lookup = {
    "static" : ClassificationMode.STATIC,
    "perfect" : ClassificationMode.PERFECT
}




def extract_ping_routes(
    simulation_results_path: os.path,
    topology: str,
    pingroute_result_name: str,
    experiment_identifier: str,
    sim_result_sets: list[SimRunResultSet] = None
) -> dict:
    """
    @param sim_result_sets: a list of SimRunResultSets. One SimRunResultSet is the outcome of running execute_simulation_pipeline ONCE.
    """

    print(f"Extract ping route for {pingroute_result_name}")

    result_path = Path(os.path.join(simulation_results_path,experiment_identifier))

    files = list(result_path.glob(f"{pingroute_result_name}-0.tar.gz"))

    performDecompression(result_path, pingroute_result_name, True, [0])

    sim_result_file = f"{pingroute_result_name}-0.0.json"

    result_summary_content = None
    with open(os.path.join(result_path, sim_result_file), "r") as result_summary_file:
        result_summary_content = json.load(result_summary_file)

    removeDecompressed(result_path, pingroute_result_name, [0])

    print("Finished extracting ping routes")
    return result_summary_content["summary"]["ping_routes"]
