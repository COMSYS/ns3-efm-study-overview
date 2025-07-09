#ifndef FAILURE_LOCALIZATION_H
#define FAILURE_LOCALIZATION_H

#include <nlohmann/json.hpp>
#include "iostream"

#include "classified-path-set.h"
#include "link-characteristic-set.h"
#include "combined-flow-set.h"
#include <sim-result-set.h>


#include "../../external/alglib/src/linalg.h"
#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include <math.h>
#include "../../external/alglib/src/solvers.h"

namespace analysis {
enum class LocalizationMethod
{
  POSSIBLE,
  PROBABLE,
  WEIGHT_ITER,
  WEIGHT_ITER_LVL,
  WEIGHT_DIR,
  WEIGHT_DIR_LVL,
  DETECTION,
  DLC,
  WEIGHT_BAD,
  WEIGHT_BAD_LVL,
  LIN_LSQR,
  LIN_LSQR_FIXED_FLOWS,
  LIN_LSQR_CORE_ONLY,
  LIN_LSQR_CORE_ONLY_FIXED_FLOWS,
  LIN_LSQR_LVL,
  LP_WITH_SLACK,
  FLOW_COMBINATION,
  FLOW_COMBINATION_FIXED_FLOWS
};

enum class FlowSelectionStrategy
{
  RANDOM,
  COVERAGE,
  ALL
};

NLOHMANN_JSON_SERIALIZE_ENUM(FlowSelectionStrategy,
                             {{FlowSelectionStrategy::RANDOM, "RANDOM"},
                              {FlowSelectionStrategy::COVERAGE, "COVERAGE"},
                              {FlowSelectionStrategy::ALL, "ALL"}})
inline FlowSelectionStrategy FlowSelectionStrategyFromString(const std::string &str)
{
  if (str == "RANDOM")
    return FlowSelectionStrategy::RANDOM;
  else if (str == "COVERAGE")
    return FlowSelectionStrategy::COVERAGE;
  else if (str == "ALL")
    return FlowSelectionStrategy::ALL;
  else {
    std::cout << str << std::endl;
    throw std::invalid_argument("Invalid flow selection strategy string");
  }
}
inline std::string FlowSelectionStrategyToString(const FlowSelectionStrategy &strat)
{
  if (strat == FlowSelectionStrategy::RANDOM)
    return "RANDOM";
  else if (strat == FlowSelectionStrategy::COVERAGE)
    return "COVERAGE";
  else if (strat == FlowSelectionStrategy::ALL)
    return "ALL";
  else {
    //std::cout << str << std::endl;
    throw std::invalid_argument("Invalid flow selection strategy");
  }
}
typedef std::map<std::string, double> FlowSelectionStrategyParams;

struct FlowSelectionStrategyWithParams
{
  FlowSelectionStrategy strategy;
  FlowSelectionStrategyParams params;
};


#define SMALL_FAIL_FACTOR 0.5
#define LARGE_FAIL_FACTOR 2

NLOHMANN_JSON_SERIALIZE_ENUM(LocalizationMethod,
                             {{LocalizationMethod::POSSIBLE, "POSSIBLE"},
                              {LocalizationMethod::PROBABLE, "PROBABLE"},
                              {LocalizationMethod::WEIGHT_ITER, "WEIGHT_ITER"},
                              {LocalizationMethod::WEIGHT_ITER_LVL, "WEIGHT_ITER_LVL"},
                              {LocalizationMethod::WEIGHT_DIR, "WEIGHT_DIR"},
                              {LocalizationMethod::WEIGHT_DIR_LVL, "WEIGHT_DIR_LVL"},
                              {LocalizationMethod::DLC, "DLC"},
                              {LocalizationMethod::DETECTION, "DETECTION"},
                              {LocalizationMethod::WEIGHT_BAD, "WEIGHT_BAD"},
                              {LocalizationMethod::WEIGHT_BAD_LVL, "WEIGHT_BAD_LVL"},
                              {LocalizationMethod::LIN_LSQR, "LIN_LSQR"},
                              {LocalizationMethod::LIN_LSQR_FIXED_FLOWS, "LIN_LSQR_FIXED_FLOWS"},
                              {LocalizationMethod::LIN_LSQR_CORE_ONLY, "LIN_LSQR_CORE_ONLY"},
                              {LocalizationMethod::LIN_LSQR_CORE_ONLY_FIXED_FLOWS, "LIN_LSQR_CORE_ONLY_FIXED_FLOWS"},
                              {LocalizationMethod::LIN_LSQR_LVL, "LIN_LSQR_LVL"},
                              {LocalizationMethod::LP_WITH_SLACK, "LP_WITH_SLACK"},
                              {LocalizationMethod::FLOW_COMBINATION, "FLOW_COMBINATION"},
                              {LocalizationMethod::FLOW_COMBINATION_FIXED_FLOWS, "FLOW_COMBINATION_FIXED_FLOWS"}})

inline LocalizationMethod LocalizationMethodFromString(const std::string &str)
{
  if (str == "POSSIBLE")
    return LocalizationMethod::POSSIBLE;
  else if (str == "PROBABLE")
    return LocalizationMethod::PROBABLE;
  else if (str == "WEIGHT_ITER")
    return LocalizationMethod::WEIGHT_ITER;
  else if (str == "WEIGHT_ITER_LVL")
    return LocalizationMethod::WEIGHT_ITER_LVL;
  else if (str == "WEIGHT_DIR")
    return LocalizationMethod::WEIGHT_DIR;
  else if (str == "WEIGHT_DIR_LVL")
    return LocalizationMethod::WEIGHT_DIR_LVL;
  else if (str == "WEIGHT_BAD")
    return LocalizationMethod::WEIGHT_BAD;
  else if (str == "WEIGHT_BAD_LVL")
    return LocalizationMethod::WEIGHT_BAD_LVL;
  else if (str == "DLC")
    return LocalizationMethod::DLC;
  else if (str == "DETECTION")
    return LocalizationMethod::DETECTION;
  else if (str == "LIN_LSQR")
    return LocalizationMethod::LIN_LSQR;
  else if (str == "LIN_LSQR_FIXED_FLOWS")
    return LocalizationMethod::LIN_LSQR_FIXED_FLOWS;
  else if (str == "LIN_LSQR_CORE_ONLY")
    return LocalizationMethod::LIN_LSQR_CORE_ONLY;
  else if (str == "LIN_LSQR_CORE_ONLY_FIXED_FLOWS")
    return LocalizationMethod::LIN_LSQR_CORE_ONLY_FIXED_FLOWS;
  else if (str == "LIN_LSQR_LVL")
    return LocalizationMethod::LIN_LSQR_LVL;
  else if (str == "LP_WITH_SLACK")
    return LocalizationMethod::LP_WITH_SLACK;
  else if (str == "FLOW_COMBINATION")
    return LocalizationMethod::FLOW_COMBINATION;
  else if (str == "FLOW_COMBINATION_FIXED_FLOWS")
    return LocalizationMethod::FLOW_COMBINATION_FIXED_FLOWS;
  else {
    std::cout << str << std::endl;
    throw std::invalid_argument("Invalid localization method string");
  }
}

inline std::string LocalizationMethodToString(const LocalizationMethod &method)
{
    switch (method){
        case LocalizationMethod::LIN_LSQR:
            return "LIN_LSQR";
        case LocalizationMethod::LIN_LSQR_FIXED_FLOWS:
            return "LIN_LSQR_FIXED_FLOWS";
        case LocalizationMethod::LIN_LSQR_CORE_ONLY:
            return "LIN_LSQR_CORE_ONLY";
        case LocalizationMethod::LIN_LSQR_CORE_ONLY_FIXED_FLOWS:
            return "LIN_LSQR_CORE_ONLY_FIXED_FLOWS";
        case LocalizationMethod::FLOW_COMBINATION:
            return "FLOW_COMBINATION";
        case LocalizationMethod::FLOW_COMBINATION_FIXED_FLOWS:
            return "FLOW_COMBINATION_FIXED_FLOWS";
        default:
            throw std::invalid_argument("Unknown localization method string");
  }
}



/*
struct LocalizationParams
{
  std::map<std::string, double> loc_params = {};
};
*/
typedef std::map<std::string, double> LocalizationParams;
typedef std::map<Link, double> LinkValueMap;


std::string LinkValueMapToString(LinkValueMap linkValueMap);

//NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LocalizationParams, loc_params)

struct LocalizationResult
{
  LinkSet failedLinks;
  LocalizationMethod method;
  LocalizationParams params;
  std::set<EfmBit> efmBits;
  LinkValueMap linkRatings;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LocalizationResult, failedLinks, method, params, efmBits,
                                   linkRatings)


class FailureLocalization
{
public:
  /// @brief Generates localization results for specified combinations of observers, efm bits and
  /// localization methods for specific loss rate and delay thresholds
  /// @param srs The simresultset to perform the operation on
  /// @param observerSets A vector of observer sets to use for localization (separately)
  /// @param efmBitSets A vector of efm bit vectors to use for localization (separately)
  /// @param lossRateTh The loss rate threshold to use for path classification
  /// @param delayTh The delay threshold to use for path classification
  /// @param flowLengthTh The flow length threshold to use for path classification (discard 'good'
  /// paths if flow length is less than this)
  /// @param methods The localization methods to use
  /// @param locMethods The localization methods and parameters to use
  static std::vector<std::pair<ClassificationConfig, std::vector<LocalizationResult>>>
  LocalizeFailures(const simdata::SimResultSet &srs, const std::vector<ObserverSet> &observerSets,
                   const std::vector<EfmBitSet> &efmBitSets, double lossRateTh, uint32_t delayTh,
                   uint32_t flowLengthTh, ClassificationMode classificationMode,
                   const std::map<LocalizationMethod, LocalizationParams> &locMethods,
                   std::string classification_base_id,
                   double time_filter,
                   FlowSelectionStrategyWithParams flowSelectionStrategyWithparams);

  /// @brief Generates a localization result for a specific combination of observers, efm bits and
  /// localization method
  /// @param cps The classified path set to use for localization, must contain classified paths for
  /// the specified observers and efm bits
  /// @param observers The observers to use for localization
  /// @param efmBits The efm bits / bit combis to use for localization
  /// @param method The localization method to use
  /// @param locParams The localization parameters to use for the localization method
  static std::optional<LocalizationResult> LocalizeFailures(
      const ClassifiedPathSet &cps, const LinkVec &all_links, const std::set<uint32_t> &observers,
      const EfmBitSet &efmBits, LocalizationMethod method, const LocalizationParams &locParams,
      double lossRateTh, uint32_t delayTh,
      double time_filter);


  static std::optional<LocalizationResult> LocalizeFailures(
      const LinkCharacteristicSet &lcs, const LinkVec &all_links,
      const std::set<uint32_t> &observers, const EfmBitSet &efmBits, LocalizationMethod method,
      const LocalizationParams &locParams, double lossRateTh, uint32_t delayTh, 
      double time_filter);

  static std::optional<LocalizationResult> LocalizeFailures(
    const CombinedFlowSet &lcs, const LinkVec &all_links, const std::set<uint32_t> &observers,
    const EfmBitSet &efmBits, LocalizationMethod method, const LocalizationParams &locParams,
    double lossRateTh, uint32_t delayTh, double time_filter);

  static alglib::sparsematrix ConnectivityMatrixToSparseMatrix(const ConnectivityMatrix &connectivityMatrix);
  static alglib::real_1d_array MeasurementVectorToReal1dArray(const MeasurementVector &measurementVector, bool isLoss);

  static LinkSet PossibleFailedLinks(const ClassPathVec &paths);
  static LinkSet ProbableFailedLinks(const ClassPathVec &paths);

  /// @brief Performs the iterative weighted localization method
  /// @param paths The classified measurement paths to use for localization
  /// @param winc Weight increase factor for links on failed paths, 1 < winc
  /// @param wdec Weight decrease factor for links on non-failed paths, 0 < wdec < 1
  /// @param wscale Weight scale factor of winc relative to path length, 0 <= wscale <= 1
  /// @param wthresh Weight threshold (excl. lower bound) for links to be considered failed, 1 <=
  /// wthresh
  /// @return The set of links considered failed
  static LinkSet IterativeWeightedFailedLinks(const ClassPathVec &paths, const LinkVec &all_links, double winc, double wdec,
                                              double wscale, double wthresh, double pathscale, double normalization);


  /// @brief Performs the iterative weighted localization method
  /// @param paths The classified measurement paths to use for localization
  /// @param winc Weight increase factor for links on failed paths, 1 < winc
  /// @param wdec Weight decrease factor for links on non-failed paths, 0 < wdec < 1
  /// @param wscale Weight scale factor of winc relative to path length, 0 <= wscale <= 1
  /// @param wthresh Weight threshold (excl. lower bound) for links to be considered failed, 1 <=
  /// wthresh
  /// @return The set of links considered failed
  static LinkSet IterativeWeightedFailedLinks_ThreeLevel(const ClassPathVec &paths, const LinkVec &all_links, double winc_lvl1, 
                                                double winc_lvl2, double winc_lvl3, double wdec, double wscale,
                                                double wthresh, double pathscale, double normalization);                       

  /// @brief Performs the direct weighted localization method
  /// @param paths The classified measurement paths to use for localization
  /// @param winc Weight increase factor for links on failed paths, 1 < winc
  /// @param wdec Weight decrease factor for links on non-failed paths, 0 < wdec < 1
  /// @param wscale Weight scale factor of winc relative to path length, 0 <= wscale <= 1
  /// @param wthresh Weight threshold (excl. lower bound) for links to be considered failed, 1 <=
  /// wthresh
  /// @return The set of links considered failed
  static LinkSet DirectWeightedFailedLinks(const ClassPathVec &paths, const LinkVec &all_links, double winc, double wdec,
                                           double wscale, double wthresh, double pathscale, double normalization);                       


  /// @brief Performs the direct weighted localization method
  /// @param paths The classified measurement paths to use for localization
  /// @param winc Weight increase factor for links on failed paths, 1 < winc
  /// @param wdec Weight decrease factor for links on non-failed paths, 0 < wdec < 1
  /// @param wscale Weight scale factor of winc relative to path length, 0 <= wscale <= 1
  /// @param wthresh Weight threshold (excl. lower bound) for links to be considered failed, 1 <=
  /// wthresh
  /// @return The set of links considered failed
  static LinkSet DirectWeightedFailedLinks_ThreeLevel(const ClassPathVec &paths, const LinkVec &all_links, double winc_lvl1, double winc_lvl2, double winc_lvl3,
                                                       double wdec, double wscale, double wthresh, double pathscale, double normalization);

  /// @brief Counts how often each link occurs on a broken measurement and selects those that occur on more than 'wthresh'. 
  /// @param paths The classified measurement paths to use for localization
  /// @param wthresh Weight threshold for links to be considered failed, 0 <= wthresh <= 1 
  /// @return The set of links considered failed
  static LinkSet DirectLinkCount(const ClassPathVec &paths, double dlcthresh);

  /// @brief Performs the bad link weight localization method
  /// @param paths The classified measurement paths to use for localization
  /// @param winc Weight increase factor for links on failed paths, 1 < winc
  /// @param wdec Weight decrease factor for links on non-failed paths, 0 < wdec < 1
  /// @param wscale Weight scale factor of winc relative to path length, 0 <= wscale <= 1
  /// @param wthresh Weight threshold (excl. lower bound) for links to be considered failed, 1 <=
  /// wthresh
  /// @return The set of links considered failed
  static LinkSet BadPathLinkWeight(const ClassPathVec &paths, const LinkVec &all_links, double winc, double wdec,
                                           double wscale, double wthresh, double pathscale, double normalization);

  /// @brief Performs the bad link weight localization method
  /// @param paths The classified measurement paths to use for localization
  /// @param winc Weight increase factor for links on failed paths, 1 < winc
  /// @param wdec Weight decrease factor for links on non-failed paths, 0 < wdec < 1
  /// @param wscale Weight scale factor of winc relative to path length, 0 <= wscale <= 1
  /// @param wthresh Weight threshold (excl. lower bound) for links to be considered failed, 1 <=
  /// wthresh
  /// @return The set of links considered failed
  static LinkSet BadPathLinkWeight_ThreeLevel(const ClassPathVec &paths, const LinkVec &all_links, double winc_lvl1, 
                                                            double winc_lvl2, double winc_lvl3, 
                                                            double wdec, double wscale, double wthresh, double pathscale, double normalization);
  static LinkSet DetectFailedLinks(const ClassPathVec &paths);

  static std::pair<LinkSet, LinkValueMap> LinearLSQR(
      const ConnectivityMatrix &connectivityMatrix, const MeasurementVector &measurementVector,
      const ReverseLinkIndexMap &reverse_link_index_map, bool localize_loss, double lossRateTh,
      uint32_t delayTh);
  static LinkSet LinearLSQR_ThreeLevel(const ConnectivityMatrix &connectivityMatrix, const MeasurementVector &measurementVector, const ReverseLinkIndexMap &reverse_link_index_map, double lossRateTh, uint32_t delayTh, double smallFailFactor, double largeFailFactor);

  static std::pair<LinkSet, LinkValueMap> LPWithSlack(const ClassPathVec &paths,
                                                      const LinkVec &all_links, bool localize_loss,
                                                      double lossRateTh, uint32_t delayTh);
};


}  // namespace analysis


#endif  // FAILURE_LOCALIZATION_H