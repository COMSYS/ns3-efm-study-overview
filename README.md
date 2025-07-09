# Central Overview Repository for *Using Explicit (Host-to-Network) Flow Measurements for Network Tomography*

This is the central overview repository for the source code accompanying our paper *Using Explicit (Host-to-Network) Flow Measurements for Network Tomography*.
It provides the general capabilities for reproducing our evaluation and relies on a patched version of ns-3.36.1.

## Publication
This work has been created in the context of the following publication:

* Ike Kunze, Constantin Sander, Alexander Ruhrmann, Niklas Rieken, and Klaus Wehrle: *Using Explicit (Host-to-Network) Flow Measurements for Network Tomography*. In ANRW '25: Proceedings of the ACM/IRTF Applied Networking Research Workshop 2025

If you use any portion of our work, please consider citing our publication.

```
@inproceedings{2025_kunze_efm-network-tomography,
  author = {Kunze, Ike and Sander, Constantin and Ruhrmann, Alexander and Rieken, Niklas and Wehrle, Klaus},
  title = {{Using Explicit (Host-to-Network) Flow Measurements for Network Tomography}},
  booktitle = {{Proceedings of the ACM/IRTF Applied Networking Research Workshop 2025}},
  year = {2025},
  doi = {10.1145/3744200.3744769},
  url = {https://www.comsys.rwth-aachen.de/publication/2025/2025_kunze_efm-network-tomography/2025_kunze_efm-network-tomography.pdf},
  publisher = {{ACM/IRTF}},
}
```

## Content

Folder | Purpose
------ | ------- 
``automation``                         | Contains the main simulation orchestration which this repository uses for conducting the experiments
``observer-placement-generator``       | Contains scripts for generating different observer placements
``plotting``                           | Contains scripts for plotting the evaluation results
``setup-scripts``                      | Contains scripts for the initial setup of the simulation and evaluation environments
``raw-result-processing``              | Contains code that analyzes and processes the raw simulation results and extracts the contained information


## Initial Setup

1. Clone this repository into desired path (WITH submodules!)
```bash
git clone git@github.com:COMSYS/ns3-efm-study-overview.git --recurse-submodules
```

2. Initialize ns-3 simulation and evaluation environment 

```bash
bash ./setup-scripts/setup-all.sh
```

3. Create python virtual environment and install requirements
```bash
python3 -m venv venv && source venv/bin/activate && pip install -r requirements.txt
```

## Experiments

Our experiment pipeline uses different wrappers, which are designed to facilitate conducting large numbers of simulation and analysis runs.
For this, we make use of extensive configuration files (see [the corresponding README](automation/README.md) for instructions on how to create the configuration files).

To run simulations based on configuration file `Test-Simulation.json`:

```bash

python3 automation/run-simulations.py -m Test-Simulation.json
```

To analyze simulation results based on configuration file `Test-Analysis.json`:

```bash

python3 automation/run-analyses.py -m Test-Analysis.json
```



## Results Reproduction

Should you aim to reproduce our results, please follow these steps.

1. Adjust paths in [simulation](automation/configs-meta/config-meta-sim-default.json) and [analysis](automation/configs-meta/config-meta-analysis-default.json) configurations.
These paths define where you intend to store the results of the simulation and the analysis.
Please provide absolute paths.

2. Generate and execute auxiliary simulation configuration file.

```bash

## Generate an auxiliary simulation configuration that provides additional input for later (more specifically, it explores the paths the ping flows will later take)
python3 automation/simulation/configuration_generator_sim.py -m generation-config-sim-PING-ROUTES.json
python3 automation/run-simulations.py -m PINGROUTE-Sim.json
```


3. Generate simulation and analysis configurations

```bash
## Generate three sets of simulation configurations
python3 automation/simulation/configuration_generator_sim.py -m generation-config-sim-ANRW-5.json
python3 automation/simulation/configuration_generator_sim.py -m generation-config-sim-ANRW-10.json
python3 automation/simulation/configuration_generator_sim.py -m generation-config-sim-ANRW-11.json

## Generate three sets of corresponding analysis configurations
python3 automation/analysis/configuration_generator_analysis.py -m generation-config-analysis-ANRW-5-FlowAndMonitorSelection_PERC.json
python3 automation/analysis/configuration_generator_analysis.py -m generation-config-analysis-ANRW-10-FlowAndMonitorSelection_PERC.json
python3 automation/analysis/configuration_generator_analysis.py -m generation-config-analysis-ANRW-11-FlowAndMonitorSelection_PERC.json
```

4. Execute simulations

```bash
## Run the simulations. Note that this can take a while.
python3 automation/run-simulations.py -m ANRW-5-Sim.json
python3 automation/run-simulations.py -m ANRW-10-Sim.json
python3 automation/run-simulations.py -m ANRW-11-Sim.json
```

5. Execute analysis as soon as simulations have completed

```bash
## Execute the analysis. Note that this can take a while.
python3 automation/run-analyses.py -m ANRW-5-Eval-FlowAndMonitorSelectionPerc.json
python3 automation/run-analyses.py -m ANRW-10-Eval-FlowAndMonitorSelectionPerc.json
python3 automation/run-analyses.py -m ANRW-11-Eval-FlowAndMonitorSelectionPerc.json
```

6. Adjust paths in the [main plot configuration](plotting/configs/plot-config-ANRW-Complete.json).

```json
{
    "plot_cache_folder": "/REPLACE/WITH/PATH/TO/WHERE_YOU_WANT_TO_STORE_PLOT_CACHE_FILES",
    "plot_output_folder": "/REPLACE/WITH/PATH/TO/WHERE_YOU_WANT_TO_STORE_PLOTS"
}
```

7. Plot the results

```bash
python3 plotting/plot-main.py 
```

Note that you will likely not be able to exactly replicate our results as the generation of the simulation configuration files entails randomness.