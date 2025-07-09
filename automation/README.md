# Simulation Automation

This folder contains configs and scripts designed for automating the execution of the simulation and the generation of config files as much as possible.

## Manual simulation pipeline overview

The simulation pipeline consists of three steps:

1. Run the simulation with ns-3 and a given simulation configuration (`ns-3-dev-fork/scratch/EFM-Localization-Stuff/some-config.json`)

2. Read the simulation output and process it with the EfmSimProcessor according to a specified analysis configuration (`raw-result-processing/data/some-config.json`)

3. Read the analysis results, perform aggregation if necessary, and display the results

Steps one and two can be executed conveniently using the python script `execute_simulation_pipeline.py` provided in this repository.

The script takes the *output-path-prefix* as a required argument, which specifies the filename prefix and potentially subfolder structure of the simulation and analysis results. For example, the prefix `dfn/download/some-50MB` would create simulation result files `ns-3-dev-fork/output/dfn/download/some-50MB-0.json` (for run 0) and analysis result files `raw-result-processing/data/analysis-results/dfn/download/analysis-some-50MB-0.json`. For different simulation configurations, a different prefix must be chosen.

Additionally, the script accepts several options to influence simulation and result processing behavior and enable compression of simulation results after processing. Use `execute_simulation_pipeline.py --help` to get an overview of the available options.

### File splitting

The result processor can only consume JSON files of up to 4GB in size. For larger simulation scenarios, the output file size would greatly exceed this limit. Thus, the simulation (and the processor) support file splitting. Splitted output files follow the schema `prefix-runNumber.part.json`, e.g., `some-50MB-0.0.json`.\
To enable splitting, use the `--sim-trace-events-per-file` option of the python wrapper and specify a number other than zero. To prevent fiel count explosion, it is **highly** recommended to set this option to at least 10000000.\
The result processor automatically detects split files, verifies that they belong to the same simulation run and combines them for analysis.

### Configuration of the simulation scenario

The simulation parameters are based on a configuration file in JSON format, by default located in `scratch/EFM-Localization-Stuff/` and named `config.json`. You can use the `--sim-config-file` option of the python wrapper to use a different filename.
In the same folder a `config-schema.json` file is present, which specifies the options and their possible values for the configuration file.
After modifying the configuration file, you can use the provided python script `config-validator.py` to validate it against the schema (or just use the python wrapper for the pipeline).

The simulation automatically saves the configuration in the json output-file along with the simulation results, so you do not need to keep track of which configuration you used to generate a set of results. Furthermore, the config is read only once at the beginning of the first run, so you are free to modify it after simulation has started.

It follows a brief explanation of all options of the simulation config:

- *usedProt*: The transport protocol to use (TCP/QUIC).
- *topology*: The topology to use (TELEKOM_GLOBAL, TELEKOM_GERMANY, GEANT, DFN).
- *rngSeed*: The seed to use for the RNG.
- *rngRun*: Used by the RNG and the output file name. No need to specify, gets overwritten by the run number offset (CLI or default 0) anyway.
- *simStopMs*: When to stop the simulation (in ms).
- *bblDataRate*: The data rate of backbone links (number with unit, e.g., 100Gbps).
- *bblDelayMus*: The delay of backbone links (in microseconds).
- *bblQueueSize*: The queue size of backbone links (number with unit, e.g., 100kp).
- *ehlDataRate*: The data rate of edge links (number with unit, e.g., 100Gbps).
- *ehlDelayMus*: The delay of edge links (in microseconds).
- *ehlQueueSize*: The queue size of edge links (number with unit, e.g., 100kp).
- *l4MSS*: The maximum segment size of the transport protocol (in bytes).
- *l4SndBufMaxBytes*: The maximum send buffer size of the transport protocol (in bytes).
- *l4RcvBufMaxBytes*: The maximum receive buffer size of the transport protocol (in bytes).
- *l4RTOms*: The retransmission timeout of the transport protocol (in milliseconds).
- *tcpSACK*: Whether to enable selective acknowledgements for TCP.
- *ehSStartMs*: The start time of the edge host applications (in milliseconds).
- *ehSStartRandOffsetMs*: The maximum random offset to add to the edge host application start time (in milliseconds).
- *ehSStopMs*: The stop time of the edge host applications (in milliseconds).
- *serverTPConnEst*: The transmission profile (see below) executed by server applications when a connection with a client is established.
- *serverTPNormalPktRcv*: The transmission profile (see below) executed by server applications when receiving a normal packet.
- *serverTPFinalPktRcv*: The transmission profile (see below) executed by server applications when receiving a packet marked as final.
- *clientTPConnEst*: The transmission profile (see below) executed by client applications when a connection with a server is established.
- *clientTPNormalPktRcv*: The transmission profile (see below) executed by client applications when receiving a normal packet.
- *clientTPFinalPktRcv*: The transmission profile (see below) executed by client applications when receiving a packet marked as final.
- *trafficScenario*: The traffic scenario to use (ALL_TO_ALL, ALL_TO_ANY, SOME_TO_SOME).
- *cSMatching*: The client-server matching to use (array of strings, e.g., ["1:3"] to connect client 1 to server 3). Must be not empty if the traffic scenario is SOME_TO_SOME and has no effect if the traffic scenario is ALL_TO_ALL.
- *failedLinks*: A list of links that should fail. Each failed link object has the following properties:
    - *link*: The link that should fail, specified as a string in the format `node1:node2`. Use simple numbers to refer to backbone nodes, prepend *S* to refer to servers and prepend *C* to refer to clients. E.g., `S1:1` refers to the link from server 1 to backbone node 1.
    - *lossRate*: The packet loss rate of the link (optional if *delayMs* is specified).
    - *delayMs*: The delay of the link in milliseconds (optional if *lossRate* is specified).

#### Transmission profiles

Transmission profiles (TPs) are objects that define transmission behavior of applications. They have the following properties:

- *packets*: Limit the number of packets to send (0 for unlimited). Takes precedence over *maxBytes*, but has no effect in bulk transmission mode.
- *maxBytes*: The maximum number of bytes to send (0 for unlimited).
- *packetSize*: Size of each packet in bytes. Just a guideline, might be higher or lower in certain scenarios. If in doubt, set equal to the configured MSS.
- *transmissionIntervalMus*: Packet transmission interval in microseconds, 0 for bulk transmission.
- *pauseAfterPackets*: Pause after sending this number of packets (0 for no pause). Has no effect in bulk transmission mode.
- *pauseAfterBytes*: Pause after sending this number of bytes (0 for no pause).
- *transmissionPauseMus*: Length of the pause (after x packets / bytes) in microseconds.
- *initialDelayMus*: The initial delay before transmission when executing the TP (in microseconds).

A transmission profile execution is triggered by certain events as specified in the simulation config: On connection establishment, on normal packet receive and on final packet receive. The last packet generated by the execution of a TP is always marked as final. This property can be used to simulate a request-response-flow.

Example for a download scenario: Configure the client to send a single packet on connection establishment and on receiving a final packet. Configure the server to send X bytes in bulk mode on receiving a final packet. This way, the client sends a request, the server sends a large chunk of data, the client sends another request, the server sends another large chunk of data, and so on.

### Configuration of the analysis

The analysis can also be configured using a JSON-based config file, by default `raw-result-processing/data/analysis-config.json` is used. You can use the `--analysis-config-file` option of the python wrapper to use a different filename. The associated schema and config validator are located in `raw-result-processing/src/analysis`.

The config file contains an array of configuration objects. For each object, the analysis is performed as specified, and the results of all analyses are combined in a single output file per simulation run.

It follows a brief explanation of all options of the analysis config object:

- *storeMeasurements*: Wether to store the measured delay and absolute/relative loss rates for each combination of observer, bit and flow.
- *performLocalization*: Wether to perform failure localization as specified in the latter options.
- *lossRateTh*: The loss rate threshold to use for failure classification. Alternatively, use *autoLossRateThOffset*.
- *delayThMs*: The delay threshold to use for failure classification. Alternatively, use *autoDelayThOffsetMs*.
- *autoLossRateThOffset*: Compute the loss rate threshold as the minimum loss rate over all failed links (as configured, not as measured) and add the specified offset (can be negative).
- *autoDelayThOffsetMs*: Compute the delay threshold as the minimum delay over all failed links (as configured, not as measured) and add the specified offset (can be negative).
- *classificationModes*: An array of classification modes to use for measurement path classifications. Possible values are *STATIC* (uses the loss/delay measured by the bits and does not consider path length) and *PERFECT* (directly uses the information which links are configured to be failed).
- *simFilter*: Specifies simFilter options.
    - *lBitTriggeredMonitoring*: Wether to filter the measurements to simulate L-Bit triggered monitoring.
    - *removeLastXSpinTransients*: Remove the last X spin measurements from the measurements.
- *localizationMethods*: Specify the failure localization methods to be used. For each method, all localization parameters need to be specified (even if they do not apply to the specific method).
- *observerSets*: Specify the observer sets to be used for localization. Localizations are performed for each observer set separately. Use the empty set to perform localizations for all observers.
- *efmBitSets*: Specify the EFM bit sets to be used for localization. Localizations are performed for each bit set separately. Note that the set `["QR"]` would only use measurement paths created by the combination of the Q and R bits, but not the paths produced by the Q and R bits themselves. Thus, the set `["Q", "R", "QR"]` is usually more useful.


## Automated simulation pipeline overview

- `run-simulations.py`
    - Meta script which wraps `execute_simulation_pipeline.py` and executes a given list of configurations (one after the other / simultaneously)
    - Uses meta configs stored in `configs-meta`
    - Execute as follows:
        - `python3 run-simulations.py --meta_config config-meta-sim-default.json`

- `run-analyses.py`
    - Meta script which wraps `execute_simulation_pipeline.py` and performs analyses based on a given configuration file
    - Uses meta configs stored in `configs-meta`
    - Execute as follows:
        - `python3 run-analyses.py --meta_config config-meta-analysis-default.json`
    - Note: 
        - The execution of the analysis requires a mapping to output_path_prefixes that were fed to the simulation
        - When using the configuration generation (see below), the `configuration_generator_analysis.py` takes the simulation configuration file as input and automatically applies all analysis configs to all of the includes output_path_prefixes


### Configuration generation

To automate the generation of simulation/analysis configurations, we also have dedicated scripts handling that process.

#### Simulation Configuration

- `simulation/configuration_generator_sim.py`
    - Generator script for generating simulation configurations
    - Will use a `generation-config` file located in `simulations/` for creating different configurations 
        - We provide four corresponding configurations that we used to generate the simulation configurations for our paper
    - The configuration files will be generated based on the default config file (as specified by `default_config_relative_in_scratch`)
        - The argument is a relative link that gives the config file name within the `scratch/EFM-Localization-Stuff/` folder
    - Currently, we support
        - "desired_topologies" : Which topologies to use (supported: all topologies from `automation/topologies` folder. For adding more see above)
        - "server_max_bytes" : How many bytes should the servers send?  
        - "traffic_scenarios" : Determine which traffic scenario to use
        - "failed_links" : Explicitly define which link fails and how
    - We can further specify the prefix of the output config files which will be stored in `simulation/configs-sim`
        - "output_file_prefix" 
    - Execute as follows:
        - `python3 simulation/configuration_generator_sim.py --meta_config generation-config-sim-ANRW-5.json`

#### Analysis Configuration

- `analysis/configuration_generator_analysis.py`
    - Generator script for generating simulation configurations
    - Will use a `generation-config` file located in `analysis/` for creating different configurations
        - We provide three corresponding configurations that we used to generate the analysis configurations for analyzing the simulations conducted in our paper
    - The configuration files will be generated based on the default analysis config file (as specified by `default_config_relative_result_processing_data`)
        - The argument is a relative link that gives the config file name within the `raw-result-processing/data/` folder
    - We can specify the prefix of the output config files which will be stored in `configs-analysis`
        - "output_file_prefix" 
    - As the analysis requires information on the `output_path_prefixes`, the `generation-config-analyses-default.json` contains a field that defines the simulation for which the analysis is to be performed
        - "simulation_meta_file_name": file name of the meta configuration used to execute the simulation
        - "analysis_meta_file_name": desired file name of the meta configuration that will be fed to the analysis script
    - Furthermore, the generation config file supports a number of fields used to modify the default config
        - Supported fields: "storeMeasurements", "performLocalization", "lossRateTh", "delayThMs", "classificationModes", "simFilter", "localizationMethods", "observerSets", "efmBitSets"
        - Each field is a list of values and a separate config file will be created for each combination of values
        - Each value in the field lists has to be supported by the corresponding field in the analysis config file
        - **Except** for "observerSets", which contains a list of two-element lists, where the first element is either `"explicit"` or `"generated"`
            - If `"explicit"` is given, the second element is a list of observer sets (i.e., a list of lists of integer), as expected by the analysis config file
            - If `"generated"` is given, the second element is a list of *observer-placement-generation-configurations*. For each configuration, an observer set is generated (a list of integers), the list of all generated sets is used as a value for the analysis config file
            - An *observer-placement-generation-configuration* is an object with the following fields:
                - "efmBitSets": A list of efm bit sets (i.e., a list of lists of strings); A placement is generated for each specified efm bit set
                - "matchEfmBits": boolean; If true, replace the value of "efmBitSets" with the list of efm bit sets currently used for config file generation (i.e., the current value in the list of the "efmBitSets" field in the generation config file)
                - "algorithm": string; The algorithm to use for observer placement generation; Has to match an enum value of the *ObserverPlacementAlgorithm* enum specified in `observer-placement-generator/op_generators.py`
                - "edgeLinks": boolean; Wether to consider an additional up- and downstream link for each node in the topology when generating observer placements
    - `python3 analysis/configuration_generator_analysis.py --meta_config generation-config-analysis-default.json`


#### Example
- Execute `python3 simulation/configuration_generator_sim.py --meta_config generation-config-sim-ANRW-5.json`
    - Will create a meta configuration (`ANRW-5-Sim.json`) in `configs-meta`
    - Will create individual configuration files (`sim_anrw_configs_5`) in `simulation/configs-sim`
- Execute `python3 analysis/configuration_generator_analysis.py --meta_config generation-config-analysis-ANRW-5-FlowAndMonitorSelection_PERC.json`
    - Will create a meta configuration (`ANRW-5-Eval-FlowAndMonitorSelectionPerc.json`) in `configs-meta`
    - Will create individual configuration files (`analysis_ANRW_5_configs_flow_and_monitor_selection_perc`) in `analysis/configs-analysis`


#### How to create a new simulation configuration
- Go to `simulation` and copy `generation-config-sim-ANRW-5.json`
- Define the specific properties of the simulation, specifying lists where multiple attributes are possible/desired
- Decide on a `meta_file_name` which you will later use to start the simulation (`python3 run-simulations.py --meta_config <meta_file_name>`)
- Define an `experiment_identifier`
- Define an `output_path_prefix` which will be used to store the results (Path: `os.path.join(base_path, experiment_identifier, output_path_prefix)`) 
- General configuration aspects are defined in the config file given in `"default_config_relative_in_scratch": "config.json",`, but these probably do not need to be touched

#### How to create a new analysis configuration
- Go to `analysis` and copy `generation-config-analysis-ANRW-5-FlowAndMonitorSelection_PERC.json`
- Decide on an `analysis_meta_file_name` which you will later use to start the analysis (`python3 run-analyses.py --meta_config <analysis_meta_file_name>`)
- Define the simulation config from which to draw the per experiment information (`simulation_meta_file_name`)
- As of now, the configurations given in `"default_config_relative_result_processing_data": "analysis-default-config.json",` will be copied to separate configuration files and no new analysis configuration will be created


### Details for run-simulations/-analyses

- `configs-meta`
    - Contains meta configuration files for `run-simulations.py` and `run-analyses.py`
    - `run-simulations.py`
        - Currently supported fields:
            - "which_config_files": list of list containing
                - config file names to be used; will automatically be chosen from `simulation/configs-sim`, i.e., give relative names
                - corresponding `output_path_prefix`  
            - "compress_results" : if output is to be compressed
            - "sim_only": if only the simulation is to be performed
            - "sim_run_count": number of individual runs
            - "sim_output_dir": where the output is to be stored 
            - "sim_traces_per_file": 0
            - "sim_trace_events_per_file": 0
            - "sim_enable_on_the_fly_qlog": enable qlog on the fly
            - "sim_disable_host_traces": disable the host traces (to save memory)
    - `run-analyses.py`
        - Currently supported fields:
            - "which_config_files": list of list containing
                - config file names to be used; will automatically be chosen from `analysis/configs-analysis`, i.e., give relative names
                - corresponding `output_path_prefix`  
            - "compress_results" : if output is to be compressed
            - "analysis_only": if only the analysis is to be performed
            - "analysis_config_file": name of the analysis config file
            - "analysis_input_dir": where the simulation output is stored
            - "analysis_output_dir": where the analysis output is to be stored

- `topologies`
    - Contains topologies that are available for the simulations
    - How to add new topologies
        1. Generate a new topology with the `topology-generator`
        2. Copy the `$ASN_topology.txt` from `output` into this folder
        3. Some more steps: TBD


