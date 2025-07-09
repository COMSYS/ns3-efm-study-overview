import json
import os
from enum import IntEnum, auto
from dataclasses import dataclass, field
from typing import Any, NamedTuple


class ResultType(IntEnum):
    SEQ_REL_LOSS = auto()
    SEQ_ABS_LOSS = auto()
    ACK_SEQ_REL_LOSS = auto()
    ACK_SEQ_ABS_LOSS = auto()
    Q_REL_LOSS = auto()
    Q_ABS_LOSS = auto()
    R_REL_LOSS = auto()
    R_ABS_LOSS = auto()
    T_REL_FULL_LOSS = auto()
    T_ABS_FULL_LOSS = auto()
    T_REL_HALF_LOSS = auto()
    T_ABS_HALF_LOSS = auto()
    L_REL_LOSS = auto()
    L_ABS_LOSS = auto()
    SPIN_AVG_DELAY = auto()
    SQ_REL_LOSS = auto()
    SQ_ABS_LOSS = auto()
    TCPDART_AVG_DELAY = auto()
    TCPRO_ABS_LOSS = auto()
    TCPRO_REL_LOSS = auto()
    PING_CLNT_ABS_LOSS = auto()
    PING_CLNT_REL_LOSS = auto()
    PING_CLNT_AVG_DELAY = auto()
    PING_SVR_ABS_LOSS = auto()
    PING_SVR_REL_LOSS = auto()
    PING_SVR_AVG_DELAY = auto()
    SPIN_DELAY_RAW = auto()
    TCPDART_DELAY_RAW = auto()
    PING_CLNT_DELAY_RAW = auto()
    PING_SVR_DELAY_RAW = auto()

class LocalizationMethod(IntEnum):
    POSSIBLE = auto()
    PROBABLE = auto()
    WEIGHT_ITER = auto()
    WEIGHT_ITER_LVL = auto()
    WEIGHT_DIR = auto()
    WEIGHT_DIR_LVL = auto()
    DETECTION = auto()
    DLC = auto()
    WEIGHT_BAD = auto()
    WEIGHT_BAD_LVL = auto()
    LIN_LSQR = auto()
    LIN_LSQR_FIXED_FLOWS = auto()
    LIN_LSQR_CORE_ONLY = auto()
    LIN_LSQR_CORE_ONLY_FIXED_FLOWS = auto()
    LIN_LSQR_LVL = auto()
    LP_WITH_SLACK = auto()
    FLOW_COMBINATION = auto()
    FLOW_COMBINATION_FIXED_FLOWS = auto()

    def __str__(self):
        if self == LocalizationMethod.POSSIBLE:
            return "POSS"
        elif self == LocalizationMethod.PROBABLE:
            return "PROB"
        elif self == LocalizationMethod.WEIGHT_ITER:
            return "WITER"
        elif self == LocalizationMethod.WEIGHT_ITER_LVL:
            return "WITERL"
        elif self == LocalizationMethod.WEIGHT_DIR:
            return "WDIR"
        elif self == LocalizationMethod.WEIGHT_DIR_LVL:
            return "WDIRL"
        elif self == LocalizationMethod.WEIGHT_BAD:
            return "WBAD"
        elif self == LocalizationMethod.WEIGHT_BAD_LVL:
            return "WBADL"
        elif self == LocalizationMethod.DLC:
            return "DLC"
        elif self == LocalizationMethod.DETECTION:
            return "DETEC"
        elif self == LocalizationMethod.LIN_LSQR:
            return "LIN_LSQR"
        elif self == LocalizationMethod.LIN_LSQR_FIXED_FLOWS:
            return "LIN_LSQR_FIXED_FLOWS"
        elif self == LocalizationMethod.LIN_LSQR_CORE_ONLY:
            return "LIN_LSQR_CORE_ONLY"
        elif self == LocalizationMethod.LIN_LSQR_CORE_ONLY_FIXED_FLOWS:
            return "LIN_LSQR_CORE_ONLY_FIXED_FLOWS"
        elif self == LocalizationMethod.LIN_LSQR_LVL:
            return "LIN_LSQR_LVL"
        elif self == LocalizationMethod.LP_WITH_SLACK:
            return "LP_SLACK"
        elif self == LocalizationMethod.FLOW_COMBINATION:
            return "FLOW_COMBINATION"
        elif self == LocalizationMethod.FLOW_COMBINATION_FIXED_FLOWS:
            return "FLOW_COMBINATION_FIXED_FLOWS"
        else:
            raise ValueError(f"Unknown localization method {self}")

class EfmBit(IntEnum):
    SEQ = auto()
    Q = auto()
    L = auto()
    R = auto()
    T = auto()
    SPIN = auto()
    QR = auto()
    QL = auto()
    QT = auto()
    LT = auto()
    TCPRO = auto()
    TCPDART = auto()
    PINGDLY = auto()
    PINGLSS = auto()

    def __str__(self):
        if self == EfmBit.SPIN:
            return "SPN"
        if self == EfmBit.TCPRO:
            return "TRO"
        if self == EfmBit.TCPDART:
            return "TDT"
        if self == EfmBit.PINGDLY:
            return "PDL"
        if self == EfmBit.PINGLSS:
            return "PLS"
        return self.name


def extendEfmBit(bit: EfmBit) -> frozenset[EfmBit]:
    if bit == EfmBit.QR:
        return frozenset([EfmBit.Q, EfmBit.R, EfmBit.QR])
    elif bit == EfmBit.QL:
        return frozenset([EfmBit.Q, EfmBit.L, EfmBit.QL])
    elif bit == EfmBit.QT:
        return frozenset([EfmBit.Q, EfmBit.T, EfmBit.QT])
    elif bit == EfmBit.LT:
        return frozenset([EfmBit.L, EfmBit.T, EfmBit.LT])
    else:
        return frozenset([bit])


class ClassificationMode(IntEnum):
    STATIC = auto()
    PERFECT = auto()


class Link(NamedTuple):
    source: int
    dest: int

    def __str__(self):
        return f"({self.source}->{self.dest})"


@dataclass(frozen=True)
class SimFilter:
    l_bit_triggered_monitoring: bool
    remove_last_x_spin_transients: int


@dataclass(frozen=True)
class ClassificationConfig:
    flows: frozenset[str] = field(compare=False)
    observers: frozenset[int]
    observer_set_metadata: Any = field(compare=False)
    loss_rate_th: float
    delay_th: int
    classification_base_id: str
    flow_length_th: int
    classification_mode: ClassificationMode

@dataclass(frozen=True)
class FlowSelection:
    selectionStrategy: str
    flow_count: int
    flowMapping: dict

@dataclass(frozen=True)
class LocalizationConfig():
    sim_filter: SimFilter
    classification_config: ClassificationConfig
    flow_selection: FlowSelection

    def __str__(self):
        sf = self.sim_filter
        cc = self.classification_config
        fs = self.flow_selection
        return f"{cc.classification_base_id} LBT {sf.l_bit_triggered_monitoring}, SPINTR {sf.remove_last_x_spin_transients}, {cc.classification_mode.name}, {len(cc.flows)} F, {len(cc.observers)} O, {cc.loss_rate_th*100}%, {cc.delay_th}ms {fs.selectionStrategy} {fs.flow_count} {cc.observer_set_metadata['algorithm'] if cc.observer_set_metadata else 'All'} {cc.observer_set_metadata['measurement_types'][0] if cc.observer_set_metadata else 'None'}"
    
    def __eq__(self, other):
        if type(other) is type(self):
            if self.sim_filter == other.sim_filter:
                if self.flow_selection == other.flow_selection:
                    if self.classification_config == other.classification_config:
                        if self.classification_config.observer_set_metadata == other.classification_config.observer_set_metadata:
                            return True
        return False

    def __hash__(self):
        return hash((self.sim_filter, self.classification_config, self.flow_selection))


@dataclass(frozen=True)
class LocalizationParams(dict):
    def __init__(self, *args, **kwargs):
        self.update(*args, **kwargs)

    def __getitem__(self, key):
        val = dict.__getitem__(self, key)
        return val

    def __setitem__(self, key, val):
        dict.__setitem__(self, key, val)

    def __repr__(self):
        dictrepr = dict.__repr__(self)
        return "%s(%s)" % (type(self).__name__, dictrepr)

    def update(self, *args, **kwargs):
        for k, v in dict(*args, **kwargs).items():
            self[k] = v


@dataclass(frozen=True)
class FailedLink:
    link: Link
    loss_rate: float
    delay_ms: int


@dataclass(frozen=True)
class LinkConfig:
    link: Link
    delay_mus: int


@dataclass(frozen=True)
class LinkStats:
    lost_packets: int
    received_packets: int
    delayAvgMus: float = None
    delayStdMus: float = None
    delayMedMus: float = None
    delay99thMus: float = None
    delayMinMus: int = None
    delayMaxMus: int = None


@dataclass(frozen=True)
class LocalizationResult:
    failed_links: frozenset[Link]
    method: LocalizationMethod
    params: LocalizationParams
    efm_bits: frozenset[EfmBit]
    link_ratings: dict[Link, float]

    def has_failures(self) -> bool:
        return len(self.failed_links) > 0


@dataclass(frozen=True)
class SimRunResult:
    sim_id: str  # Assume format SIM_baseId_runNumber_runId
    config: dict[str, Any]
    flow_paths: dict[str, list[int]]
    failed_links: list[FailedLink]
    backbone_overrides: list[LinkConfig]
    all_links: set[Link]
    core_links: set[Link]
    edge_links: set[Link]
    link_ground_truth: dict[Link, LinkStats]
    observer_flow_res: dict[int, dict[str, dict[ResultType, float]]]
    observer_path_res: dict[int, dict[str, dict[ResultType, float]]]
    observer_active_res: dict[int, dict[str, dict[ResultType, float]]]

    observer_flow_res_raw: dict[int, dict[str, dict[ResultType, float]]]
    observer_active_res_raw: dict[int, dict[str, dict[ResultType, float]]]

    localization_res: dict[LocalizationConfig, list[LocalizationResult]]

    def __post_init__(self):
        if self.all_links:
            for fl in self.failed_links:
                if fl.link not in self.all_links:
                    raise ValueError(f"Failed link {fl.link} not in all_links")

    def getBaseId(self) -> str:
        return self.sim_id.split("_")[1]

    def getRunNumber(self) -> int:
        id_based = int(self.sim_id.split("_")[2])
        if id_based != int(self.config["rngRun"]):
            raise RuntimeError(
                f"SimRunResult.sim_id ({self.sim_id}) does not match config.rngRun ({self.config['rngRun']})"
            )
        return id_based

    def getRunId(self) -> str:
        return self.sim_id.split("_")[3]


@dataclass(frozen=True)
class SimRunResultSet:
    sim_run_results: list[SimRunResult]

    def __post_init__(self):
        if not self.sim_run_results:
            raise ValueError("sim_run_results must not be empty")
        for index, res in enumerate(self.sim_run_results):
            if res.getBaseId() != self.sim_run_results[0].getBaseId():
                raise ValueError("sim_run_results must have the same baseId")
            if any(
                res.getRunNumber() == run.getRunNumber()
                for run in self.sim_run_results[index + 1 :]
            ):
                raise ValueError(
                    "different sim_run_results must not have the same runNumber"
                )

    def getConfig(self) -> dict[str, Any]:
        return self.sim_run_results[0].config


def ImportSimRunResultSet(folder_path: str, prefix: str = "") -> SimRunResultSet:
    return SimRunResultSet(ImportSimRunResults(folder_path, prefix))


def ImportSimRunResults(folder_path: str, prefix: str = "") -> list[SimRunResult]:
    results = []
    for path in os.scandir(folder_path):
        if (
            path.is_file()
            and path.name.startswith(prefix)
            and path.name.endswith(".json")
            and prefix + "-" + path.name.split("-")[-1] == path.name
        ):
            print(f"Importing {path.name}")
            with open(path, "r") as f:
                data = json.load(
                    f,
                    object_hook=lambda d: {
                        (int(k) if k.lstrip("-").isdigit() else k): v
                        for k, v in d.items()
                    },
                )
            run = ImportSimRunResult(data)
            if results and run.getBaseId() != results[-1].getBaseId():
                raise RuntimeError(
                    f"SimRunResult {run.sim_id} has different baseId than previous results"
                )
            results.append(run)
    return results


def ImportSimRunResult(data: dict) -> SimRunResult:
    _failed_links = []
    for link in data["failedLinks"]:
        _failed_links.append(
            FailedLink(
                link=Link(link["sourceNodeId"], link["destNodeId"]),
                loss_rate=link["lossRate"],
                delay_ms=link["delayMs"],
            )
        )

    _backbone_overrides = []
    if "backboneOverrides" in data:
        for link in data["backboneOverrides"]:
            _backbone_overrides.append(
                LinkConfig(
                    link=Link(link["sourceNodeId"], link["destNodeId"]),
                    delay_mus=link["delayMus"],
                )
            )

    _all_links = set()
    if "allLinks" in data:
        for link in data["allLinks"]:
            lt = Link(link[0], link[1])
            if lt in _all_links:
                raise ValueError(f"Link {lt} already in all_links")
            _all_links.add(lt)

    _core_links = set()
    if "coreLinks" in data:
        for link in data["coreLinks"]:
            lt = Link(link[0], link[1])
            if lt in _core_links:
                raise ValueError(f"Link {lt} already in core_links")
            _core_links.add(lt)

    _edge_links = set()
    if "edgeLinks" in data:
        for link in data["edgeLinks"]:
            lt = Link(link[0], link[1])
            if lt in _edge_links:
                raise ValueError(f"Link {lt} already in edge_links")
            _edge_links.add(lt)

    _link_ground_truth = {}
    if "linkGroundtruthStats" in data:
        for link, stats in data["linkGroundtruthStats"]:
            _link_ground_truth[Link(link[0], link[1])] = LinkStats(
                lost_packets=stats["lostPackets"],
                received_packets=stats["receivedPackets"],
                delayAvgMus=stats.get("delayAvgMus", None),
                delayStdMus=stats.get("delayStdMus", None),
                delayMedMus=stats.get("delayMedMus", None),
                delay99thMus=stats.get("delay99thMus", None),
                delayMinMus=stats.get("delayMinMus", None),
                delayMaxMus=stats.get("delayMaxMus", None),
            )

    _observer_flow_res = {}
    for observer, flow_res in data["observerFlowResults"].items():
        _observer_flow_res[observer] = {
            flow_path: {ResultType[k.upper()]: v for k, v in res.items()}
            for flow_path, res in flow_res.items()
        }

    _observer_flow_res_raw = {}
    if "observerFlowResultsRawValues" in data.keys() and data["observerFlowResultsRawValues"]:
        for observer, flow_res in data["observerFlowResultsRawValues"].items():
            _observer_flow_res_raw[observer] = {
                flow_path: {ResultType[k.upper()]: v for k, v in res.items()}
                for flow_path, res in flow_res.items()
            }

    _observer_path_res = {}
    if "observerPathResults" in data:
        for observer, path_res in data["observerPathResults"].items():
            _observer_path_res[observer] = {
                path: {ResultType[k.upper()]: v for k, v in res.items()}
                for path, res in path_res.items()
            }

    _observer_active_res = {}
    if "observerActiveResults" in data and data["observerActiveResults"]:
        for observer, active_res in data["observerActiveResults"].items():
            _observer_active_res[observer] = {
                path: {ResultType[k.upper()]: v for k, v in res.items()}
                for path, res in active_res.items()
            }


    _observer_active_res_raw = {}
    if "observerActiveResultsRawValues" in data.keys() and data["observerActiveResultsRawValues"]:
        for observer, active_res in data["observerActiveResultsRawValues"].items():
            _observer_active_res_raw[observer] = {
                path: {ResultType[k.upper()]: v for k, v in res.items()}
                for path, res in active_res.items()
            }

    _localization_res = dict()
    counter = 0
    for res in data["localizationResults"]:
        filter = SimFilter(
            res["filter"]["lBitTriggeredMonitoring"],
            res["filter"]["removeLastXSpinTransients"],
        )
        class_conf = ClassificationConfig(
            flows=frozenset(res["config"]["flowIds"]),
            observers=frozenset(res["config"]["observerIds"]),
            observer_set_metadata=res["config"].get("observerSetMetadata", None),
            loss_rate_th=res["config"]["lossRateTh"],
            delay_th=res["config"]["delayTh"],
            classification_base_id=res["config"].get(
                "classification_base_id", f"default_id_py_{counter}"
            ),
            flow_length_th=res["config"].get("flowLengthTh", 0),
            classification_mode=ClassificationMode[
                res["config"]["classificationMode"].upper()
            ],
        )

        flowMappingTuple = tuple()
        for observer_entry in res["flowSelection"]["selectionMapping"]:
            observer_id = observer_entry[0]
            observer_flows = tuple(observer_entry[1])
            flowMappingTuple = flowMappingTuple + ((observer_id, observer_flows),)
        flow_selection = FlowSelection(
                            selectionStrategy=res["flowSelection"]["selectionStrategy"],
                            flow_count=res["flowSelection"]["params"]["flow_count"] if "flow_count" in res["flowSelection"]["params"].keys() else 0,
                            flowMapping=flowMappingTuple,) 
        loc_config = LocalizationConfig(filter, class_conf, flow_selection)
        localization_results = []
        for loc_res in res["results"]:
            _link_ratings = dict()
            if "linkRatings" in loc_res:
                for lr in loc_res["linkRatings"]:
                    _link_ratings[Link(lr[0][0], lr[0][1])] = lr[1]
            localization_results.append(
                LocalizationResult(
                    failed_links=frozenset(
                        Link(*link) for link in loc_res["failedLinks"]
                    ),
                    method=LocalizationMethod[loc_res["method"].upper()],
                    params=LocalizationParams(loc_res["params"]),
                    efm_bits=frozenset(EfmBit[k] for k in loc_res["efmBits"]),
                    link_ratings=_link_ratings,
                )
            )
        if loc_config in _localization_res:
            raise RuntimeError(
                f"Duplicate localization results for config {loc_config}"
            )
        _localization_res[loc_config] = localization_results
        counter += 1

    return SimRunResult(
        sim_id=data["simId"],
        config=data["config"],
        flow_paths=data["flowPathMap"],
        failed_links=_failed_links,
        backbone_overrides=_backbone_overrides,
        all_links=_all_links,
        core_links=_core_links,
        edge_links=_edge_links,
        link_ground_truth=_link_ground_truth,
        observer_flow_res=_observer_flow_res,
        observer_path_res=_observer_path_res,
        observer_active_res=_observer_active_res,
        observer_active_res_raw=_observer_active_res_raw,
        observer_flow_res_raw=_observer_flow_res_raw,
        localization_res=_localization_res,
    )
