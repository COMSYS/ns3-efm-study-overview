from data_manager import (
    SimRunResult,
    LocalizationResult,
    LocalizationMethod,
    LocalizationParams,
    EfmBit,
    Link,
    ClassificationMode,
    LocalizationConfig,
)
from dataclasses import dataclass
from collections import defaultdict
import math
from typing import Optional


EFM_LOSS_BITS = [
    EfmBit.SEQ,
    EfmBit.Q,
    EfmBit.L,
    EfmBit.R,
    EfmBit.T,
    EfmBit.QR,
    EfmBit.QL,
    EfmBit.QT,
    EfmBit.LT,
    EfmBit.TCPRO,
    EfmBit.PINGLSS,
]
EFM_DELAY_BITS = [EfmBit.SPIN, EfmBit.TCPDART, EfmBit.PINGDLY]


@dataclass(frozen=True)
class CategorizedLocalizationResult:
    localization_result: LocalizationResult
    false_positives: frozenset[Link]
    false_negatives: frozenset[Link]
    true_positives: frozenset[Link]
    true_negatives: frozenset[Link]
    link_rating_errors_abs: dict[Link, float]
    link_rating_errors_rel: dict[Link, float]

    def getRelativeFalsePositives(self) -> float:
        if not self.false_positives:
            return 0
        # Fraction of false positives in relation to all positives
        return len(self.false_positives) / len(self.localization_result.failed_links)

    def getRelativeFalseNegatives(self) -> float:
        if not self.false_negatives:
            return 0
        # Fraction of false negatives in relation to all (actually) failed links
        return len(self.false_negatives) / (
            len(self.false_negatives) + len(self.true_positives)
        )


def CategorizeLocResults(
    srr: SimRunResult, 
    use_configured_loss: bool = False,
    use_configured_delay: bool = False
) -> dict[LocalizationConfig, list[CategorizedLocalizationResult]]:
    """
    Categorize the localization results into false positives, false negatives, and true positives.
    """

    loss_links = set()
    link_loss_rates = dict()
    delay_links = set()
    link_delay = dict()

    coreDelay = int(srr.config["bblDelayMus"]) / 1000
    edgeDelay = int(srr.config["ehlDelayMus"]) / 1000

    # Set default values for all links
    # Use configured delay as groundtruth
    for link in srr.core_links:
        link_delay[link] = coreDelay
        link_loss_rates[link] = 0.0
    for link in srr.edge_links:
        link_delay[link] = edgeDelay
        link_loss_rates[link] = 0.0

    # Overwrite delay data for backbone overrides
    for link_override in srr.backbone_overrides:
        link_delay[link_override.link] = link_override.delay_mus / 1000  # Convert to ms

    # Overwrite loss data for failed links if configured
    # Overwrite delay data for failed links
    for failed_link in srr.failed_links:
        if failed_link.loss_rate > 0:
            loss_links.add(failed_link.link)
            if use_configured_loss:
                link_loss_rates[failed_link.link] = failed_link.loss_rate
        if failed_link.delay_ms > 0:
            delay_links.add(failed_link.link)
            link_delay[failed_link.link] = failed_link.delay_ms

    # Overwrite loss data to actual packet loss experienced by packets
    if not use_configured_loss:
        for link, link_stats in srr.link_ground_truth.items():
            link_loss_rates[link] = link_stats.lost_packets / (
                link_stats.received_packets + link_stats.lost_packets
            )

    # Overwrite delay data to actual delay experienced by packets
    if not use_configured_delay:
        for link, link_stats in srr.link_ground_truth.items():
            if link_stats.delayAvgMus: ## Overwrite for those links where we have actual data
                link_delay[link] = link_stats.delayAvgMus / 1000 #Convert to ms

    categorized_results = dict()
    for loc_config in srr.localization_res:
        categorized_results[loc_config] = []
        for loc_res in srr.localization_res[loc_config]:
            true_positives = set()
            false_negatives = set()

            link_rating_errors_abs = dict()
            link_rating_errors_rel = dict()

            if loc_res.efm_bits.intersection(EFM_LOSS_BITS):
                false_negatives.update(loss_links - loc_res.failed_links)
                true_positives.update(loc_res.failed_links.intersection(loss_links))

                for link, rating in loc_res.link_ratings.items():
                    actual_loss_rate = link_loss_rates[link]
                    link_rating_errors_abs[link] = rating - actual_loss_rate
                    if actual_loss_rate == 0:
                        link_rating_errors_rel[link] = math.inf
                    else:
                        link_rating_errors_rel[link] = (
                            rating - actual_loss_rate
                        ) / actual_loss_rate

            if loc_res.efm_bits.intersection(EFM_DELAY_BITS):
                false_negatives.update(delay_links - loc_res.failed_links)
                true_positives.update(loc_res.failed_links.intersection(delay_links))

                if link_rating_errors_abs or link_rating_errors_rel:
                    raise RuntimeError(
                        "We do not support link rating errors for both loss and delay for a single localization result."
                    )
                for link, rating in loc_res.link_ratings.items():
                    actual_delay = link_delay[link]
                    link_rating_errors_abs[link] = rating - actual_delay
                    if actual_delay == 0:
                        link_rating_errors_rel[link] = math.inf
                    else:
                        link_rating_errors_rel[link] = (
                            rating - actual_delay
                        ) / actual_delay

            categorized_results[loc_config].append(
                CategorizedLocalizationResult(
                    loc_res,
                    false_positives=frozenset(loc_res.failed_links - true_positives),
                    false_negatives=frozenset(false_negatives),
                    true_positives=frozenset(true_positives),
                    true_negatives=frozenset(
                        (srr.all_links - loc_res.failed_links) - false_negatives
                    ),
                    link_rating_errors_abs=link_rating_errors_abs,
                    link_rating_errors_rel=link_rating_errors_rel,
                )
            )
    return categorized_results


def GetUsedEFMBits(srr: SimRunResult) -> list[EfmBit]:
    efm_bits = []
    for loc_config in srr.localization_res:
        for loc_res in srr.localization_res[loc_config]:
            for bit in loc_res.efm_bits:
                if bit not in efm_bits:
                    efm_bits.append(bit)
    return efm_bits


def GetUsedLocAlgos(srr: SimRunResult) -> list[LocalizationMethod]:
    loc_algos = []
    for loc_config in srr.localization_res:
        for loc_res in srr.localization_res[loc_config]:
            if loc_res.method not in loc_algos:
                loc_algos.append(loc_res.method)
    return loc_algos


def GetUsedLocAlgosWithParam(srr: SimRunResult) -> list[LocalizationMethod]:
    loc_algos = []
    loc_algo_strings = []
    for loc_config in srr.localization_res:
        for loc_res in srr.localization_res[loc_config]:
            if str(loc_res.method) + "_" + str(loc_res.params) not in loc_algo_strings:
                loc_algos.append((loc_res.method, loc_res.params))
                loc_algo_strings.append(str(loc_res.method) + "_" + str(loc_res.params))
    return loc_algos


def GetUsedClassMode(srr: SimRunResult) -> list[ClassificationMode]:
    class_modes = []
    for loc_config in srr.localization_res:
        if loc_config.classification_config.classification_mode not in class_modes:
            class_modes.append(loc_config.classification_config.classification_mode)
    return class_modes


def FilterCategorizedLocResults(
    clrs: list[CategorizedLocalizationResult],
    method: Optional[LocalizationMethod] = None,
    efm_bits: Optional[frozenset[EfmBit]] = None,
    params: Optional[LocalizationParams] = None,
    hasFalsePositives: Optional[bool] = None,
    hasFalseNegatives: Optional[bool] = None,
    hasTruePositives: Optional[bool] = None,
) -> list[CategorizedLocalizationResult]:
    filtered_clrs = []
    for clr in clrs:
        if method is not None and clr.localization_result.method != method:
            continue
        if params is not None and clr.localization_result.params != params:
            continue
        if efm_bits is not None and clr.localization_result.efm_bits != efm_bits:
            continue
        if (
            hasFalsePositives is not None
            and (len(clr.false_positives) > 0) != hasFalsePositives
        ):
            continue
        if (
            hasFalseNegatives is not None
            and (len(clr.false_negatives) > 0) != hasFalseNegatives
        ):
            continue
        if (
            hasTruePositives is not None
            and (len(clr.true_positives) > 0) != hasTruePositives
        ):
            continue
        filtered_clrs.append(clr)

    return filtered_clrs


def FilterCategorizedLocResultDict(
    clrs: dict[LocalizationConfig, list[CategorizedLocalizationResult]],
    classification_mode: Optional[ClassificationMode] = None,
    method: Optional[LocalizationMethod] = None,
    observers: Optional[frozenset[int]] = None,
    efm_bits: Optional[frozenset[EfmBit]] = None,
    loss_rate_th: Optional[float] = None,
    delay_th: Optional[int] = None,
    hasFalsePositives: Optional[bool] = None,
    hasFalseNegatives: Optional[bool] = None,
    hasTruePositives: Optional[bool] = None,
) -> dict[LocalizationConfig, list[CategorizedLocalizationResult]]:
    """
    Filter the categorized localization results by the given parameters.
    """
    filtered_clrs = defaultdict(list)
    for lc in clrs:
        cc = lc.classification_config
        if (
            classification_mode is not None
            and cc.classification_mode != classification_mode
        ):
            continue
        if observers is not None and cc.observers != observers:
            continue
        if loss_rate_th is not None and cc.loss_rate_th != loss_rate_th:
            continue
        if delay_th is not None and cc.delay_th != delay_th:
            continue

        filtered_clrs[lc].extend(
            FilterCategorizedLocResults(
                clrs[lc],
                method,
                efm_bits,
                None,
                hasFalsePositives,
                hasFalseNegatives,
                hasTruePositives,
            )
        )
    return filtered_clrs
