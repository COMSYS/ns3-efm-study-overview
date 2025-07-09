#include "failure-localization.h"
#ifdef USE_GUROBI
#include "gurobi_c++.h"
#endif


using namespace alglib;

namespace analysis {

uint32_t getRandomNumber(uint32_t moduloValue){
    uint32_t x;
    do {
        x = rand();
    } while (x >= (RAND_MAX - RAND_MAX % moduloValue));
    x %= moduloValue;
    return x;
}

std::map<uint32_t, std::set<uint32_t>> SelectFlows(const simdata::SimResultSet &srs, ObserverSet observerSet, FlowSelectionStrategyWithParams selection_strategy, bool flow_combination_required){

    std::map<uint32_t, std::set<uint32_t>> flowSelectionMap;
    std::map<uint32_t, std::set<uint32_t>> randomNumbersMap;
    std::map<uint32_t, std::set<Link>> uncoveredLinksMap;

    switch (selection_strategy.strategy){
        case FlowSelectionStrategy::ALL: {
            for (auto &oid : observerSet.observers)
            {
                flowSelectionMap[oid] = srs.GetObserverFlowIds(oid);
            }
            break;
        }
        case FlowSelectionStrategy::RANDOM: {
            if (!flow_combination_required){
                for (auto &oid : observerSet.observers)
                {
                    auto availableFlows = srs.GetObserverFlowIds(oid);
                    std::set<uint32_t> selectedFlows;
                    std::set<uint32_t> randomNumbers;
                    for (uint32_t count=0; count < selection_strategy.params["flow_count"] && count < availableFlows.size(); count++){  
                        uint32_t randomNumber = getRandomNumber(availableFlows.size());
                        uint32_t selectedFlow = *std::next(availableFlows.begin(), randomNumber);
                        while (randomNumbers.count(randomNumber) || selectedFlows.count(selectedFlow)){
                            randomNumber = getRandomNumber(availableFlows.size());
                            selectedFlow = *std::next(availableFlows.begin(), randomNumber);
                        }
                        selectedFlows.insert(selectedFlow);
                        randomNumbers.insert(randomNumber);
                    }
                    flowSelectionMap[oid] = selectedFlows;
                }
            } else {
                for (auto &oid : observerSet.observers)
                {
                    std::set<uint32_t> selectedFlows;
                    std::set<uint32_t> randomNumbers;

                    // observer already has entries
                    if (flowSelectionMap.count(oid)){
                        //std::cout << std::to_string(oid) << " has entries" << std::endl; 
                        selectedFlows = flowSelectionMap.at(oid);
                    }

                    if (selectedFlows.size() >= selection_strategy.params["flow_count"]){
                        // Observer already has enough flows to track
                        //std::cout << "already enough " << std::endl;
                        continue;
                    } 
                    int remaining_required_entries = selection_strategy.params["flow_count"] - selectedFlows.size();
                    //std::cout << std::to_string(oid) << " requires " << std::to_string(remaining_required_entries) << std::endl; 

                    auto availableFlows = srs.GetObserverFlowIds(oid);
                    for (uint32_t count=0; count < remaining_required_entries && count < availableFlows.size(); count++){  
                        uint32_t randomNumber = getRandomNumber(availableFlows.size());
                        uint32_t selectedFlow = *std::next(availableFlows.begin(), randomNumber);
                        while (randomNumbers.count(randomNumber) || selectedFlows.count(selectedFlow)){
                            randomNumber = getRandomNumber(availableFlows.size());
                            selectedFlow = *std::next(availableFlows.begin(), randomNumber);
                        }
                        //std::cout << std::to_string(oid) << " selects " << std::to_string(selectedFlow) << " with " << std::to_string(randomNumber) << std::endl;
                        selectedFlows.insert(selectedFlow);
                        randomNumbers.insert(randomNumber);


                        std::vector<simdata::ObsvVantagePointPointer> fp = srs.CalculateFlowPath(selectedFlow);
                        uint32_t reverseFid = srs.GetReverseFlowId(selectedFlow);
                        std::vector<simdata::ObsvVantagePointPointer> reverseFp = srs.CalculateFlowPath(reverseFid);

                        for (auto &obptr : fp) {
                            uint32_t observerId = obptr->m_nodeId;
                            std::set<uint32_t> selectedFlowsOtherObserver;
                            std::set<uint32_t> randomNumbersOtherObserver;

                            // observer already has entries
                            if (flowSelectionMap.count(observerId)){
                                //std::cout << std::to_string(observerId) << " has entries" << std::endl; 
                                selectedFlowsOtherObserver = flowSelectionMap.at(observerId);
                            }
                            selectedFlowsOtherObserver.insert(selectedFlow);
                            flowSelectionMap[observerId] = selectedFlowsOtherObserver;
                        }
                    }
                    flowSelectionMap[oid] = selectedFlows;
                    randomNumbersMap[oid] = randomNumbers;
                }
            }
            break;
        }
        case FlowSelectionStrategy::COVERAGE: {

            // Determine all available flows
            std::set<uint32_t> allAvailableFlows;
            for (auto &oid : observerSet.observers)
            {
                auto fids = srs.GetObserverFlowIds(oid);
                allAvailableFlows.insert(fids.begin(), fids.end());
            }

            // Determine the path coverage of all flows
            std::map<uint32_t, std::set<Link>> flow_path_coverage;
            for (auto &fid : allAvailableFlows)
            {
                // Calculate (reverse) flow path as sequence of observers and corresponding link path as
                // sequence of links
                std::vector<simdata::ObsvVantagePointPointer> fp = srs.CalculateFlowPath(fid);
                auto _lp = GenerateLinkPath(fp);
                if (!_lp.has_value())
                continue;
                LinkPath lp = _lp.value();
                uint32_t reverseFid = srs.GetReverseFlowId(fid);
                std::vector<simdata::ObsvVantagePointPointer> reverseFp = srs.CalculateFlowPath(reverseFid);
                auto _reverseLp = GenerateLinkPath(reverseFp);
                if (!_reverseLp.has_value())
                continue;
                LinkPath reverseLp = _reverseLp.value();
                
                std::set<Link> forwardPathLinks;
                for (auto link : lp.links){
                    forwardPathLinks.insert(link);
                }
                flow_path_coverage[fid] = forwardPathLinks;
            }

            // Get information on all links that could be covered in theory
            LinkVec all_links =  srs.GetAllLinks();
            std::set<Link> all_links_set;
            for (auto link: all_links){
                all_links_set.insert(link);
            }

            // Select fitting flows for every observer
            for (auto &oid : observerSet.observers)
            {

                std::set<Link> uncovered_links = all_links_set;
                std::set<uint32_t> observerFlows = srs.GetObserverFlowIds(oid);

                std::set<uint32_t> selectedFlows;

                // observer already has entries
                if (flow_combination_required && flowSelectionMap.count(oid)){
                    //std::cout << std::to_string(oid) << " has entries" << std::endl; 
                    selectedFlows = flowSelectionMap.at(oid);
                    uncovered_links = uncoveredLinksMap.at(oid); 
                }

                if ((selectedFlows.size() >= selection_strategy.params["flow_count"]) || (uncovered_links.size() == 0)){
                    // Observer already has enough flows to track
                    //std::cout << "already enough " << std::endl;
                    continue;
                } 

                int remaining_required_entries = selection_strategy.params["flow_count"] - selectedFlows.size();
                for (uint32_t count=0; count < remaining_required_entries; count++){  

                    uint32_t maximum_coverage = 0;
                    uint32_t selected_flow = -1;
                    bool flow_with_intersection = false;

                    uint32_t longest_flow = -1;
                    uint32_t maximum_flow_length = 0;
                    for (auto flow: flow_path_coverage){

                        // Flow has not yet been selected and crosses the observer (i.e., is part of the observer Flows)
                        if (!selectedFlows.count(flow.first) && observerFlows.count(flow.first)){

                            // Still uncovered links left
                            if (uncovered_links.size() > 0){

                                std::set<Link> intersection;
                                std::set_intersection(uncovered_links.begin(), uncovered_links.end(), 
                                                        flow.second.begin(), flow.second.end(),
                                                        std::inserter(intersection, intersection.begin()));
                                //Larger coverage of this flow
                                if (intersection.size() > maximum_coverage){
                                    maximum_coverage = intersection.size();
                                    selected_flow = flow.first;
                                    flow_with_intersection = true;
                                // Smaller flow id
                                } else if (intersection.size() == maximum_coverage && flow.first < selected_flow){
                                    maximum_coverage = intersection.size();
                                    selected_flow = flow.first;
                                    flow_with_intersection = true;
                                }
                            }
                            // Larger coverage of this flow
                            if (flow.second.size() > maximum_flow_length){
                                longest_flow = flow.first;
                                maximum_flow_length = flow.second.size();
                            // Smaller flow id
                            } else if (flow.second.size() == maximum_flow_length && flow.first < longest_flow){
                                longest_flow = flow.first;
                                maximum_flow_length = flow.second.size();
                            }
                        }
                    }
                    // If we have a flow with new coverage, select that one
                    if (flow_with_intersection){
                        //std::cout << "Monitor " << oid << " selects flow " << selected_flow << " with coverage " << maximum_coverage << std::endl;
                        selectedFlows.insert(selected_flow);
                        for (auto link: flow_path_coverage[selected_flow]){
                            uncovered_links.erase(link);
                        }
                    // We do not have new coverage, just select the flow with the largest path
                    } else{
                        //std::cout << "Monitor " << oid << " selects flow " << longest_flow << " with length " << maximum_flow_length << std::endl;
                        selectedFlows.insert(longest_flow);
                    }

                    if (flow_combination_required){
                        uint32_t flow_to_add;
                        if (flow_with_intersection){
                            flow_to_add = selected_flow;
                        } else{
                            flow_to_add = longest_flow;
                        }

                        std::vector<simdata::ObsvVantagePointPointer> fp = srs.CalculateFlowPath(flow_to_add);
                        uint32_t reverseFid = srs.GetReverseFlowId(flow_to_add);
                        std::vector<simdata::ObsvVantagePointPointer> reverseFp = srs.CalculateFlowPath(reverseFid);

                        for (auto &obptr : fp) {
                            uint32_t observerId = obptr->m_nodeId;
                            std::set<uint32_t> selectedFlowsOtherObserver;
                            std::set<Link> uncoveredLinksOtherObserver;

                            // observer already has entries
                            if (flowSelectionMap.count(observerId)){
                                //std::cout << std::to_string(observerId) << " has entries" << std::endl; 
                                selectedFlowsOtherObserver = flowSelectionMap.at(observerId);
                                uncoveredLinksOtherObserver = uncoveredLinksMap.at(observerId);
                            } else{
                                uncoveredLinksOtherObserver = all_links_set;
                            }
                            selectedFlowsOtherObserver.insert(flow_to_add);
                            for (auto link: flow_path_coverage[flow_to_add]){
                                uncoveredLinksOtherObserver.erase(link);
                            }
                            flowSelectionMap[observerId] = selectedFlowsOtherObserver;
                            uncoveredLinksMap[observerId] = uncoveredLinksOtherObserver;
                        }
                    }
                }
                flowSelectionMap[oid] = selectedFlows;
            }
            break;
        }
        default:
            throw std::runtime_error("Unknown FlowSelectionStrategy.");
    }
    return flowSelectionMap;
}


std::vector<std::pair<ClassificationConfig, std::vector<LocalizationResult>>>
FailureLocalization::LocalizeFailures(
    const simdata::SimResultSet &srs, const std::vector<ObserverSet> &observerSets,
    const std::vector<EfmBitSet> &efmBitSets, double lossRateTh, uint32_t delayTh,
    uint32_t flowLengthTh, ClassificationMode classificationMode,
    const std::map<LocalizationMethod, LocalizationParams> &locMethods,
    std::string classification_base_id,
    double time_filter,
    FlowSelectionStrategyWithParams flowSelectionStrategyWithparams)
{
  std::set<EfmBit> joinedBits;
  for (const EfmBitSet &bits : efmBitSets)
  {
    joinedBits.insert(bits.begin(), bits.end());
  }

  // One map for each observer set
  // The map then includes one map for each observer
  std::map<uint32_t, std::map<uint32_t, std::set<uint32_t>>> selected_flow_ids_per_observer;
  std::map<uint32_t, ObserverSet> observerSetmap;

  std::map<uint32_t, std::map<uint32_t, std::set<uint32_t>>> selected_flow_ids_per_observer_flow_combination;
  std::map<uint32_t, ObserverSet> observerSetmap_flow_combination;

  uint32_t observer_set_id = 0;
  for (const auto &observerSet : observerSets)
  {
    selected_flow_ids_per_observer[observer_set_id] = SelectFlows(srs, observerSet, flowSelectionStrategyWithparams, false);
    observerSetmap[observer_set_id] = observerSet;
    observer_set_id++;
  }

  bool flow_combination_required = false;
  for (const auto &method : locMethods)
    {
        if ((method.first == LocalizationMethod::FLOW_COMBINATION_FIXED_FLOWS) ||  (method.first == LocalizationMethod::LIN_LSQR_CORE_ONLY_FIXED_FLOWS) ||   (method.first == LocalizationMethod::LIN_LSQR_FIXED_FLOWS)) {
            flow_combination_required = true;
        }
    }

  if (flow_combination_required) {
    uint32_t observer_set_id = 0;
    for (const auto &observerSet : observerSets)
    {
        selected_flow_ids_per_observer_flow_combination[observer_set_id] = SelectFlows(srs, observerSet, flowSelectionStrategyWithparams, true);
        observerSetmap_flow_combination[observer_set_id] = observerSet;
        observer_set_id++;
    }
  }

  std::vector<std::pair<ClassificationConfig, std::vector<LocalizationResult>>> results;
  for ( const auto &observerSetEntry : observerSetmap) {
    ObserverSet observerSet = observerSetEntry.second;
    std::map<uint32_t, std::set<uint32_t>> selectedFlowIdsMap = selected_flow_ids_per_observer[observerSetEntry.first];

    ObserverSet observerSet_FlowCombination; // = observerSetEntry.second;
    std::map<uint32_t, std::set<uint32_t>> selectedFlowIdsMap_FlowCombination; // = selected_flow_ids_per_observer[observerSetEntry.first];

    if (flow_combination_required){
        observerSet_FlowCombination = observerSetmap_flow_combination[observerSetEntry.first];
        selectedFlowIdsMap_FlowCombination = selected_flow_ids_per_observer_flow_combination[observerSetEntry.first];
    }

    ClassifiedPathSet cps = ClassifiedPathSet::ClassifyAll(
    srs, observerSet.observers, selectedFlowIdsMap, joinedBits, lossRateTh, delayTh, flowLengthTh, classificationMode, classification_base_id, SMALL_FAIL_FACTOR, LARGE_FAIL_FACTOR, time_filter);
    ClassificationConfig baseConfig = cps.GetConfig();


    /* Process is as follows to solve min|A * x - b = 0|
        1. Prepare a matrix A: columns == number of links, rows == measurements -> specify which links used in which measurements
        2. Prepare a vector b: rows ==  measurement results
        3. Feed both into Sparse LSQR solver: https://www.alglib.net/linear-solvers/lsqr-sparse-linear-least-squares.php
        4. Output: estimations of link characteristics -> feed them into a classification where we can directly apply thresholds on the individual links
        5. Maybe also run against measurements to see for which links we get high errors
    */
    LinkCharacteristicSet lcs_core_only;
    LinkCharacteristicSet lcs;
    LinkCharacteristicSet lcs_core_only_fixed_flows;
    LinkCharacteristicSet lcs_fixed_flows;

    if (classificationMode != ClassificationMode::PERFECT){
        lcs_core_only = LinkCharacteristicSet::CharacterizeAll(srs, observerSet.observers, selectedFlowIdsMap, joinedBits, flowLengthTh, true, classificationMode, classification_base_id, time_filter);
        lcs = LinkCharacteristicSet::CharacterizeAll(srs, observerSet.observers, selectedFlowIdsMap, joinedBits, flowLengthTh, false, classificationMode, classification_base_id, time_filter);

        if (flow_combination_required) {
            lcs_core_only_fixed_flows = LinkCharacteristicSet::CharacterizeAll(srs, observerSet_FlowCombination.observers, selectedFlowIdsMap, joinedBits, flowLengthTh, true, classificationMode, classification_base_id, time_filter);
            lcs_fixed_flows = LinkCharacteristicSet::CharacterizeAll(srs, observerSet_FlowCombination.observers, selectedFlowIdsMap, joinedBits, flowLengthTh, false, classificationMode, classification_base_id, time_filter);
        }
    }

    CombinedFlowSet cfs;
    CombinedFlowSet cfs_fixed_flows;
    if (classificationMode != ClassificationMode::PERFECT && flow_combination_required){
        cfs = CombinedFlowSet::CharacterizeAll(srs, observerSet.observers, selectedFlowIdsMap_FlowCombination, joinedBits, flowLengthTh, classificationMode, classification_base_id, time_filter);

        cfs_fixed_flows = CombinedFlowSet::CharacterizeAll(srs, observerSet_FlowCombination.observers, selectedFlowIdsMap_FlowCombination, joinedBits, flowLengthTh, classificationMode, classification_base_id, time_filter);
    }

    std::vector<LocalizationResult> locResults;
    for (const EfmBitSet &bits : efmBitSets)
    {
      for (const auto &method : locMethods)
      {
        bool use_link_classification = false;
        bool special_treatment = false;
        switch (method.first)
        {
            case LocalizationMethod::POSSIBLE:
            case LocalizationMethod::PROBABLE:
            case LocalizationMethod::WEIGHT_ITER:
            case LocalizationMethod::WEIGHT_DIR:
            case LocalizationMethod::WEIGHT_ITER_LVL:
            case LocalizationMethod::WEIGHT_DIR_LVL:
            case LocalizationMethod::DLC:
            case LocalizationMethod::WEIGHT_BAD:
            case LocalizationMethod::WEIGHT_BAD_LVL:
            case LocalizationMethod::DETECTION:
            case LocalizationMethod::LP_WITH_SLACK:
              use_link_classification = true;
              break;
            case LocalizationMethod::LIN_LSQR_CORE_ONLY:
            case LocalizationMethod::LIN_LSQR_CORE_ONLY_FIXED_FLOWS:
            case LocalizationMethod::LIN_LSQR:
            case LocalizationMethod::LIN_LSQR_FIXED_FLOWS:
              use_link_classification = false;
              break;
            case LocalizationMethod::FLOW_COMBINATION:
            case LocalizationMethod::FLOW_COMBINATION_FIXED_FLOWS:
              special_treatment = true;
              break;
            default:
              throw std::runtime_error("Unknown localization method.");
        }

        if (use_link_classification && !special_treatment){
          std::optional<LocalizationResult> res =
              LocalizeFailures(cps, srs.GetAllLinks(), observerSet.observers, bits, method.first,
                               method.second, lossRateTh, delayTh, time_filter);
          if (res.has_value())
            locResults.push_back(res.value());
        } else if (!use_link_classification && !special_treatment){
            if (classificationMode != ClassificationMode::PERFECT){

                if (method.first == LocalizationMethod::LIN_LSQR_CORE_ONLY){
                    std::optional<LocalizationResult> res =
                        LocalizeFailures(lcs_core_only, srs.GetAllLinks(), observerSet.observers, bits,
                                        method.first, method.second, lossRateTh, delayTh, time_filter);
                    if (res.has_value())
                        locResults.push_back(res.value());
                } else if (method.first == LocalizationMethod::LIN_LSQR_CORE_ONLY_FIXED_FLOWS){
                    std::optional<LocalizationResult> res =
                        LocalizeFailures(lcs_core_only_fixed_flows, srs.GetAllLinks(), observerSet_FlowCombination.observers, bits,
                                        method.first, method.second, lossRateTh, delayTh, time_filter);
                    if (res.has_value())
                        locResults.push_back(res.value());
                } else if (method.first == LocalizationMethod::LIN_LSQR){
                    std::optional<LocalizationResult> res =
                        LocalizeFailures(lcs, srs.GetAllLinks(), observerSet.observers, bits,
                                        method.first, method.second, lossRateTh, delayTh, time_filter);
                    if (res.has_value())
                        locResults.push_back(res.value());
                } else if (method.first == LocalizationMethod::LIN_LSQR_FIXED_FLOWS) {
                    std::optional<LocalizationResult> res =
                        LocalizeFailures(lcs_fixed_flows, srs.GetAllLinks(), observerSet_FlowCombination.observers, bits,
                                        method.first, method.second, lossRateTh, delayTh, time_filter);
                    if (res.has_value())
                        locResults.push_back(res.value());
                } else { 
                    throw std::runtime_error("No other mode supported here.");
                }
            }
        } else if (special_treatment && AreSingleCombinationBit(bits)){
            if (classificationMode != ClassificationMode::PERFECT){

                if (method.first == LocalizationMethod::FLOW_COMBINATION){

                    std::optional<LocalizationResult> res =
                        LocalizeFailures(cfs, srs.GetAllLinks(), observerSet.observers, bits,
                                        method.first, method.second, lossRateTh, delayTh, time_filter);
                    if (res.has_value())
                        locResults.push_back(res.value());
                } else if (method.first == LocalizationMethod::FLOW_COMBINATION_FIXED_FLOWS){

                    std::optional<LocalizationResult> res =
                        LocalizeFailures(cfs_fixed_flows, srs.GetAllLinks(), observerSet_FlowCombination.observers, bits,
                                        method.first, method.second, lossRateTh, delayTh, time_filter);
                    if (res.has_value())
                        locResults.push_back(res.value());
                }
            }
        }
      }
    }
    ClassificationConfig config = baseConfig;
    config.observerSet = observerSet;
    for (auto &oid : observerSet.observers)
    {
      auto fids = srs.GetObserverFlowIds(oid, selectedFlowIdsMap);
      config.flowIds.insert(fids.begin(), fids.end());
    }
    config.flowSelectionMap = selectedFlowIdsMap;
    results.emplace_back(config, locResults);
  }
  return results;
}


static std::string printIntSet(const std::set<uint32_t> &s)
{
  std::stringstream ss;
  ss << "{";
  for (auto it = s.begin(); it != s.end(); it++)
  {
    ss << *it;
    if (std::next(it) != s.end())
      ss << ", ";
  }
  ss << "}";
  return ss.str();
}

std::optional<LocalizationResult> FailureLocalization::LocalizeFailures(
    const ClassifiedPathSet &cps, const LinkVec &all_links, const std::set<uint32_t> &observers,
    const EfmBitSet &efmBits, LocalizationMethod method, const LocalizationParams &locParams,
    double lossRateTh, uint32_t delayTh, double time_filter)
{
  ClassPathVec paths = cps.GetClassifiedPaths(observers, efmBits);
  if (paths.empty())
  {
    std::cout << "Warning: Skip localization results for " << efmbitset_to_string(efmBits)
              << " with observer set " << printIntSet(observers) << " because no paths were found."
              << std::endl;
    return std::nullopt;
  }

  LocalizationResult result;
  result.efmBits = efmBits;
  result.method = method;
  result.params = locParams;


  switch (method)
  {
    case LocalizationMethod::POSSIBLE:
      result.failedLinks = PossibleFailedLinks(paths);
      break;
    case LocalizationMethod::PROBABLE:
      result.failedLinks = ProbableFailedLinks(paths);
      break;
    case LocalizationMethod::WEIGHT_ITER:
      if ((locParams.count("winc") == 1) && (locParams.count("wdec") == 1) && (locParams.count("wscale") == 1) && (locParams.count("wthresh") == 1) && (locParams.count("pathscale") == 1)){
        double normalize = 0.0;
        if (locParams.count("normalization") == 1){
            normalize = locParams.at("normalization");
        } 
        result.failedLinks = IterativeWeightedFailedLinks(paths, all_links, locParams.at("winc"), locParams.at("wdec"), locParams.at("wscale"), locParams.at("wthresh"), locParams.at("pathscale"), normalize);
      } else {
        throw std::runtime_error("Not all params available for WEIGHT_ITER.");
      }
      break;
    case LocalizationMethod::WEIGHT_DIR:
      if ((locParams.count("winc") == 1) && (locParams.count("wdec") == 1) && (locParams.count("wscale") == 1) && (locParams.count("wthresh") == 1) && (locParams.count("pathscale") == 1)){
        double normalize = 0.0;
        if (locParams.count("normalization") == 1){
            normalize = locParams.at("normalization");
        } 
        result.failedLinks = DirectWeightedFailedLinks(paths, all_links, locParams.at("winc"), locParams.at("wdec"), locParams.at("wscale"), locParams.at("wthresh"), locParams.at("pathscale"), normalize);
      } else {
        throw std::runtime_error("Not all params available for WEIGHT_DIR.");
      }
      break;
    case LocalizationMethod::WEIGHT_ITER_LVL:
      if ((locParams.count("winc_lvl1") == 1) && (locParams.count("winc_lvl2") == 1) && (locParams.count("winc_lvl3") == 1) && (locParams.count("wdec") == 1) && (locParams.count("wscale") == 1) && (locParams.count("wthresh") == 1) && (locParams.count("pathscale") == 1)){
        double normalize = 0.0;
        if (locParams.count("normalization") == 1){
            normalize = locParams.at("normalization");
        } 
        result.failedLinks = IterativeWeightedFailedLinks_ThreeLevel(paths, all_links, locParams.at("winc_lvl1"), locParams.at("winc_lvl2"), locParams.at("winc_lvl3"), locParams.at("wdec"), locParams.at("wscale"), locParams.at("wthresh"), locParams.at("pathscale"), normalize);
      } else {
        throw std::runtime_error("Not all params available for WEIGHT_ITER_LVL.");
      }
      break;
    case LocalizationMethod::WEIGHT_DIR_LVL:
      if ((locParams.count("winc_lvl1") == 1) && (locParams.count("winc_lvl2") == 1) && (locParams.count("winc_lvl3") == 1) && (locParams.count("wdec") == 1) && (locParams.count("wscale") == 1) && (locParams.count("wthresh") == 1) && (locParams.count("pathscale") == 1)){
        double normalize = 0.0;
        if (locParams.count("normalization") == 1){
            normalize = locParams.at("normalization");
        } 
        result.failedLinks = DirectWeightedFailedLinks_ThreeLevel(paths, all_links, locParams.at("winc_lvl1"), locParams.at("winc_lvl2"), locParams.at("winc_lvl3"), locParams.at("wdec"), locParams.at("wscale"), locParams.at("wthresh"), locParams.at("pathscale"), normalize);
      } else {
        throw std::runtime_error("Not all params available for WEIGHT_DIR_LVL.");
      }
      break;
    case LocalizationMethod::DLC:
      if ((locParams.count("dlcthresh") == 1)){
        result.failedLinks = DirectLinkCount(paths, locParams.at("dlcthresh"));
      } else {
        throw std::runtime_error("Not all params available for DLC.");
      }
      break;
    case LocalizationMethod::WEIGHT_BAD:
      if ((locParams.count("winc") == 1) && (locParams.count("wdec") == 1) && (locParams.count("wscale") == 1) && (locParams.count("wthresh") == 1) && (locParams.count("pathscale") == 1)){
        double normalize = 0.0;
        if (locParams.count("normalization") == 1){
            normalize = locParams.at("normalization");
        } 
        result.failedLinks = BadPathLinkWeight(paths, all_links, locParams.at("winc"), locParams.at("wdec"), locParams.at("wscale"), locParams.at("wthresh"), locParams.at("pathscale"), normalize);
      } else {
        throw std::runtime_error("Not all params available for WEIGHT_BAD.");
      }
      break;
    case LocalizationMethod::WEIGHT_BAD_LVL:
      if ((locParams.count("winc_lvl1") == 1) && (locParams.count("winc_lvl2") == 1) && (locParams.count("winc_lvl3") == 1) && (locParams.count("wdec") == 1) && (locParams.count("wscale") == 1) && (locParams.count("wthresh") == 1) && (locParams.count("pathscale") == 1)){
        double normalize = 0.0;
        if (locParams.count("normalization") == 1){
            normalize = locParams.at("normalization");
        } 
        result.failedLinks = BadPathLinkWeight_ThreeLevel(paths, all_links, locParams.at("winc_lvl1"), locParams.at("winc_lvl2"), locParams.at("winc_lvl3"), locParams.at("wdec"), locParams.at("wscale"), locParams.at("wthresh"), locParams.at("pathscale"), normalize);
      } else {
        throw std::runtime_error("Not all params available for WEIGHT_BAD_LVL.");
      }
      break;
    case LocalizationMethod::DETECTION:
      result.failedLinks = DetectFailedLinks(paths);
      break;
    case LocalizationMethod::LP_WITH_SLACK:
#ifdef USE_GUROBI
      std::tie(result.failedLinks, result.linkRatings) =
          LPWithSlack(paths, all_links, AreLossBits(efmBits), lossRateTh, delayTh);
#else
      result.failedLinks = LinkSet();
      std::cout << "Gurobi not available, skipping LP_WITH_SLACK." << std::endl;
#endif
      break;
    default:
      throw std::runtime_error("Unknown localization method.");
  }

  return result;
}


std::optional<LocalizationResult> FailureLocalization::LocalizeFailures(
    const LinkCharacteristicSet &lcs, const LinkVec &all_links, const std::set<uint32_t> &observers,
    const EfmBitSet &efmBits, LocalizationMethod method, const LocalizationParams &locParams,
    double lossRateTh, uint32_t delayTh, double time_filter)
{
  ConnMatrixMeasVecPair cmmvp = lcs.GetConnectivityMatrixMeasurementVector(observers, efmBits);
  if (cmmvp.first.empty() || cmmvp.second.empty())
  {
    std::cout << "Warning: Skip localization results for " << efmbitset_to_string(efmBits)
              << " with observer set " << printIntSet(observers)
              << " because no matrix/measurement vectors were found." << std::endl;
    return std::nullopt;
  }

  LocalizationResult result;
  result.efmBits = efmBits;
  result.method = method;
  result.params = locParams;


  /*
  std::cout << "We use the following EFM Bits: ";
  for (EfmBit const& efmBit : efmBits)
    {
        std::cout << efmbit_to_string(efmBit) << ',';
    }
  std::cout << std::endl;
  */
  switch (method)
  {
    case LocalizationMethod::LIN_LSQR_CORE_ONLY:
    case LocalizationMethod::LIN_LSQR_CORE_ONLY_FIXED_FLOWS:
    case LocalizationMethod::LIN_LSQR:
    case LocalizationMethod::LIN_LSQR_FIXED_FLOWS:
      std::tie(result.failedLinks, result.linkRatings) =
          LinearLSQR(cmmvp.first, cmmvp.second, lcs.GetReverseLinkIndexMap(), AreLossBits(efmBits),
                     lossRateTh, delayTh);
      break;
    default:
        std::cout << LocalizationMethodToString(method) << std::endl;
      throw std::runtime_error("Incompatible localization method. Expecting LIN_LSQR*");
  }
  //std::cout << LinkValueMapToString(result.linkRatings) <<std::endl;
  return result;
}



std::optional<LocalizationResult> FailureLocalization::LocalizeFailures(
    const CombinedFlowSet &cfs, const LinkVec &all_links, const std::set<uint32_t> &observers,
    const EfmBitSet &efmBits, LocalizationMethod method, const LocalizationParams &locParams,
    double lossRateTh, uint32_t delayTh, double time_filter)
{
  ConnMatrixMeasVecPair cmmvp = cfs.GetConnectivityMatrixMeasurementVector(observers, efmBits);
  if (cmmvp.first.empty() || cmmvp.second.empty())
  {
    std::cout << "Warning: Skip CFS localization results for " << efmbitset_to_string(efmBits)
              << " with observer set " << printIntSet(observers)
              << " because no matrix/measurement vectors were found." << std::endl;
    return std::nullopt;
  }

  LocalizationResult result;
  result.efmBits = efmBits;
  result.method = method;
  result.params = locParams;

  switch (method)
  {
    case LocalizationMethod::FLOW_COMBINATION:
    case LocalizationMethod::FLOW_COMBINATION_FIXED_FLOWS:
      std::tie(result.failedLinks, result.linkRatings) =
          LinearLSQR(cmmvp.first, cmmvp.second, cfs.GetReverseLinkIndexMap(), AreLossBits(efmBits),
                     lossRateTh, delayTh);
      break;
    default:
      throw std::runtime_error("Incompatible localization method. Expecting FLOW_COMBINATION*");
  }
  //std::cout << LinkValueMapToString(result.linkRatings) <<std::endl;
  return result;
}



LinkSet FailureLocalization::PossibleFailedLinks(const ClassPathVec &paths)
{
  LinkSet goodLinks;
  LinkSet badLinks;

  for (auto &cp : paths)
  {
    if (cp.failed)
    {
      for (const Link &link : cp.path.links)
      {
        if (goodLinks.find(link) == goodLinks.end())  // Link not classified good
          badLinks.insert(link);
      }
    }
    else
    {
      for (const Link &link : cp.path.links)
      {
        goodLinks.insert(link);
        badLinks.erase(link);
      }
    }
  }

  return badLinks;
}

LinkSet FailureLocalization::ProbableFailedLinks(const ClassPathVec &paths)
{
  LinkSet goodLinks;
  LinkSet badLinks;

  for (auto &cp : paths)
  {
    if (!cp.failed)
    {
      goodLinks.insert(cp.path.links.begin(), cp.path.links.end());
    }
  }

  for (auto &cp : paths)
  {
    if (cp.failed)
    {
      bool exactlyOneBad = false;
      Link badLink;
      for (const Link &link : cp.path.links)
      {
        if (goodLinks.find(link) == goodLinks.end())  // Link not classified good
        {
          if (exactlyOneBad)
          {
            exactlyOneBad = false;
            break;
          }
          else
          {
            exactlyOneBad = true;
            badLink = link;
          }
        }
      }
      if (exactlyOneBad)
        badLinks.insert(badLink);
    }
  }

  return badLinks;
}

namespace {  // anonymous namespace

double link_increase_formula_with_path_scaling(double winc, double wscale, double path_length){

    // w_e \cdot (1 + \alpha - (\alpha \cdot (1 - \frac{1}{\abs{p}}) \cdot \gamma))
    return (1 + winc - (winc * (1 - (1/path_length)) * wscale));
}

double link_increase_formula_without_path_scaling(double winc, double wscale){

    // w_e \cdot (1 + \alpha - (\alpha \cdot (1 - \frac{1}{\abs{p}}) \cdot \gamma))
    return (1 + winc * wscale);
}

std::map<Link, double> CalculateLinkWeights(const ClassPathVec &paths, const LinkVec &all_links, double winc, double wdec,
                                            double wscale, double pathscale, double normalization)
{
  std::map<Link, double> linkWeights;
  for (const ClassifiedLinkPath &cp : paths)
  {
    if (cp.path.links.size() == 0)
      continue;  // Skip empty paths to prevent division by zero
    if (cp.failed)
    {
      // Calculate the increase factor based on path length
      double inc_factor = 1;
      if (pathscale == 1.0) {
        inc_factor = link_increase_formula_with_path_scaling(winc, wscale, cp.path.links.size());
      } else {
        inc_factor = link_increase_formula_without_path_scaling(winc, wscale);
      }
      for (const Link &link : cp.path.links)
      {
        auto lw = linkWeights.find(link);
        // Set the initial weight to 1 if it doesn't exist
        if (lw == linkWeights.end())
        {
            if (normalization == 1.0){
                lw = linkWeights.insert(std::make_pair(link, 1.0 / all_links.size())).first;
            } else {
                lw = linkWeights.insert(std::make_pair(link, 1.0)).first;
            }
        }
        // Increase the weight on failed paths
        lw->second *= inc_factor;
      }
    }
    else
    {
      for (const Link &link : cp.path.links)
      {
        auto lw = linkWeights.find(link);
        // Set the initial weight to 1 if it doesn't exist
        if (lw == linkWeights.end())
        {
            if (normalization == 1.0){
                lw = linkWeights.insert(std::make_pair(link, 1.0 / all_links.size())).first;
            } else {
                lw = linkWeights.insert(std::make_pair(link, 1.0)).first;
            }
        }
        // Decrease the weight on good paths
        lw->second *= wdec;
      }
    }
    if (normalization == 1.0){
        // Determine overall weight
        double overall_weight_sum = 0.0;
        for(auto iter = all_links.begin(); iter < all_links.end(); iter++)
            {
                // Find the weight of the link
                auto lw = linkWeights.find(*iter);
                // Set the initial weight to 1 if it doesn't exist
                if (lw == linkWeights.end())
                {
                    lw = linkWeights.insert(std::make_pair(*iter, 1.0 / all_links.size())).first;
                }
                overall_weight_sum += lw->second;
            }   
        // Do actual normalization
        for (auto &lw : linkWeights)
        {
            lw.second /= overall_weight_sum;
        }
    }
  }
  return linkWeights;
}

std::map<Link, double> CalculateLinkWeights_ThreeLevel(const ClassPathVec &paths, const LinkVec &all_links, double winc_lvl1, double winc_lvl2, double winc_lvl3, double wdec,
                                            double wscale, double pathscale, double normalization)
{
  std::map<Link, double> linkWeights;
  for (const ClassifiedLinkPath &cp : paths)
  {
    if (cp.path.links.size() == 0)
      continue;  // Skip empty paths to prevent division by zero

    if (cp.small_failure || cp.medium_failure || cp.large_failure)
    {
      // Calculate the increase factor based on path length
      //double inc_factor = winc - (wscale * (winc - 1) * (1 - (1 / cp.path.links.size())));
      double inc_factor = 1;


      if (pathscale == 1.0) {
        if (cp.large_failure){
            inc_factor = link_increase_formula_with_path_scaling(winc_lvl3, wscale, cp.path.links.size());
        } else if (cp.medium_failure) {
            inc_factor = link_increase_formula_with_path_scaling(winc_lvl2, wscale, cp.path.links.size());
        } else if (cp.small_failure) {
            inc_factor = link_increase_formula_with_path_scaling(winc_lvl1, wscale, cp.path.links.size());
        } else {
            throw std::runtime_error("This flow should have a failure, but hasn't."); 
        }
      } else {
        if (cp.large_failure){
            inc_factor = link_increase_formula_without_path_scaling(winc_lvl3, wscale);
        } else if (cp.medium_failure) {
            inc_factor = link_increase_formula_without_path_scaling(winc_lvl2, wscale);
        } else if (cp.small_failure) {
            inc_factor = link_increase_formula_without_path_scaling(winc_lvl1, wscale);
        } else {
            throw std::runtime_error("This flow should have a failure, but hasn't."); 
        }
      }
      
      

      for (const Link &link : cp.path.links)
      {
        auto lw = linkWeights.find(link);
        // Set the initial weight to 1 if it doesn't exist
        if (lw == linkWeights.end())
        {
            if (normalization == 1.0){
                lw = linkWeights.insert(std::make_pair(link, 1.0 / all_links.size())).first;
            } else {
                lw = linkWeights.insert(std::make_pair(link, 1.0)).first;
            }
        }
        // Increase the weight on failed paths
        lw->second *= inc_factor;
      }
    } 
    else
    {
      for (const Link &link : cp.path.links)
      {
        auto lw = linkWeights.find(link);
        // Set the initial weight to 1 if it doesn't exist
        if (lw == linkWeights.end())
        {
            if (normalization == 1.0){
                lw = linkWeights.insert(std::make_pair(link, 1.0 / all_links.size())).first;
            } else {
                lw = linkWeights.insert(std::make_pair(link, 1.0)).first;
            }
        }
        // Decrease the weight on good paths
        lw->second *= wdec;
      }
    }

    if (normalization == 1.0){
        // Determine overall weight
        double overall_weight_sum = 0.0;
        for(auto iter = all_links.begin(); iter < all_links.end(); iter++)
            {
                // Find the weight of the link
                auto lw = linkWeights.find(*iter);
                // Set the initial weight to 1 if it doesn't exist
                if (lw == linkWeights.end())
                {
                    lw = linkWeights.insert(std::make_pair(*iter, 1.0 / all_links.size())).first;
                }
                overall_weight_sum += lw->second;
            }   
        // Do actual normalization
        for (auto &lw : linkWeights)
        {
            lw.second /= overall_weight_sum;
        }
    }
  }
  return linkWeights;
}

std::map<Link, double> CalculateLinkWeights_BadPathsOnly(const ClassPathVec &paths, const LinkVec &all_links, double winc, double wdec,
                                            double wscale, double pathscale, double normalization)
{
  std::map<Link, double> linkWeights;
  // TODO Initialize ALL links to 1
  for (const ClassifiedLinkPath &cp : paths)
  { 
    if (cp.path.links.size() == 0)
      continue;  // Skip empty paths to prevent division by zero
    if (cp.failed)
    {
      double overall_weight_sum = 0.0;
      // Calculate the increase factor based on path length
      //double inc_factor = winc - (wscale * (winc - 1) * (1 - (1 / cp.path.links.size())));
      double inc_factor = 1;
      if (pathscale == 1.0) {
        inc_factor = link_increase_formula_with_path_scaling(winc, wscale, cp.path.links.size());
      } else {
        inc_factor = link_increase_formula_without_path_scaling(winc, wscale);
      }
    
      // Iterate over all links of the topology
      for(auto iter = all_links.begin(); iter < all_links.end(); iter++)
      {
        // Check if the link is on the studied path
        bool on_path = false;
        if (std::find(cp.path.links.begin(), cp.path.links.end(), *iter) != cp.path.links.end()) {
            on_path = true;
        }

        // Find the weight of the link
        auto lw = linkWeights.find(*iter);
        // Set the initial weight to 1 if it doesn't exist
        if (lw == linkWeights.end())
        {
            if (normalization == 1.0){
                lw = linkWeights.insert(std::make_pair(*iter, 1.0 / all_links.size())).first;
            } else {
                lw = linkWeights.insert(std::make_pair(*iter, 1.0)).first;
            }
        }

        if (on_path){
            lw->second *= inc_factor;
        } else{
            lw->second *= wdec;
        }
        overall_weight_sum += lw->second;
      }
      
      if (normalization == 1.0){
        // Do actual normalization
        for (auto &lw : linkWeights)
        {
            lw.second /= overall_weight_sum;
        }
      }      
    }

    
  }
  return linkWeights;
}

std::map<Link, double> CalculateLinkWeights_BadPathsOnly_ThreeLevel(const ClassPathVec &paths, const LinkVec &all_links, double winc_lvl1, double winc_lvl2, double winc_lvl3, double wdec,
                                            double wscale, double pathscale, double normalization)
{
  std::map<Link, double> linkWeights;
  // TODO Initialize ALL links to 1
  for (const ClassifiedLinkPath &cp : paths)
  { 
    if (cp.path.links.size() == 0)
      continue;  // Skip empty paths to prevent division by zero
    
    if (cp.small_failure || cp.medium_failure || cp.large_failure)
    {

      double overall_weight_sum = 0.0;
      // Calculate the increase factor based on path length
      //double inc_factor = winc - (wscale * (winc - 1) * (1 - (1 / cp.path.links.size())));
      double inc_factor;
      

      if (pathscale == 1.0) {
        if (cp.large_failure){
            inc_factor = link_increase_formula_with_path_scaling(winc_lvl3, wscale, cp.path.links.size());
        } else if (cp.medium_failure) {
            inc_factor = link_increase_formula_with_path_scaling(winc_lvl2, wscale, cp.path.links.size());
        } else if (cp.small_failure) {
            inc_factor = link_increase_formula_with_path_scaling(winc_lvl1, wscale, cp.path.links.size());
        } else {
            throw std::runtime_error("This flow should have a failure, but hasn't."); 
        }
      } else {
        if (cp.large_failure){
            inc_factor = link_increase_formula_without_path_scaling(winc_lvl3, wscale);
        } else if (cp.medium_failure) {
            inc_factor = link_increase_formula_without_path_scaling(winc_lvl2, wscale);
        } else if (cp.small_failure) {
            inc_factor = link_increase_formula_without_path_scaling(winc_lvl1, wscale);
        } else {
            throw std::runtime_error("This flow should have a failure, but hasn't."); 
        }
      }

      // Iterate over all links of the topology
      for(auto iter = all_links.begin(); iter < all_links.end(); iter++)
      {
        // Check if the link is on the studied path
        bool on_path = false;
        if (std::find(cp.path.links.begin(), cp.path.links.end(), *iter) != cp.path.links.end()) {
            on_path = true;
        }

        // Find the weight of the link
        auto lw = linkWeights.find(*iter);
        // Set the initial weight to 1 if it doesn't exist
        if (lw == linkWeights.end())
        {
            if (normalization == 1.0){
                lw = linkWeights.insert(std::make_pair(*iter, 1.0 / all_links.size())).first;
            } else {
                lw = linkWeights.insert(std::make_pair(*iter, 1.0)).first;
            }
        }

        if (on_path){
            lw->second *= inc_factor;
        } else{
            lw->second *= wdec;
        }
        overall_weight_sum += lw->second;

      }      

        if (normalization == 1.0){
            // Do actual normalization
            for (auto &lw : linkWeights)
            {
                lw.second /= overall_weight_sum;
            }
        }
    }
  }
  return linkWeights;
}

std::map<Link, double> CalculateLinkCounts(const ClassPathVec &paths)
{
  std::map<Link, double> linkCounts;
  double overall_count = 0.0;
  for (const ClassifiedLinkPath &cp : paths)
  {
    if (cp.path.links.size() == 0)
      continue;  // Skip empty paths to prevent division by zero
    if (cp.failed)
    {
      // Count the overall number of used measurements
      overall_count += 1;

      // Increase the count for each link contained on the failed path
      for (const Link &link : cp.path.links)
      {
        auto lw = linkCounts.find(link);
        // Set the initial count to 1 if it doesn't exist (as it is on this failed path)
        if (lw == linkCounts.end())
        {
          lw = linkCounts.insert(std::make_pair(link, 1.0)).first;
        }
        else 
        {
          lw->second += 1.0;
        }
      }
    }
  }
  
  // Divide the link count by the overall number of faulty measurements to normalize result to [0,1] 
  if (overall_count != 0) {
    for(auto& lc_val : linkCounts) {
        lc_val.second /= overall_count;
    }
  }
  return linkCounts;
}

}  // anonymous namespace

LinkSet FailureLocalization::IterativeWeightedFailedLinks(const ClassPathVec &paths, const LinkVec &all_links, double winc,
                                                          double wdec, double wscale,
                                                          double wthresh,
                                                          double pathscale, double normalization)
{
  LinkSet badLinks;
  ClassPathVec itPaths = paths;  // Copy path set because the iteration modifies it
  double maxWeight = 0;
  Link maxLink;
  do
  {
    maxWeight = 0;
    std::map<Link, double> linkWeights = CalculateLinkWeights(itPaths, all_links, winc, wdec, wscale, pathscale, normalization);
    for (const auto &lw : linkWeights)
    {
      if (lw.second > maxWeight)
      {
        maxLink = lw.first;
        maxWeight = lw.second;
      }
    }
    if (maxWeight > wthresh)
    {
      badLinks.insert(maxLink);
      // Remove all paths that contain the failed link with max weight
      for (auto it = itPaths.begin(); it != itPaths.end();)
      {
        if (std::find(it->path.links.begin(), it->path.links.end(), maxLink) !=
            it->path.links.end())
        {
          it = itPaths.erase(it);
        }
        else
        {
          it++;
        }
      }
    }
  } while (maxWeight > wthresh);

  return badLinks;
}

LinkSet FailureLocalization::DirectWeightedFailedLinks(const ClassPathVec &paths, const LinkVec &all_links, double winc,
                                                       double wdec, double wscale, double wthresh, double pathscale, double normalization)
{
  LinkSet badLinks;
  std::map<Link, double> linkWeights = CalculateLinkWeights(paths, all_links, winc, wdec, wscale, pathscale, normalization);
  for (const auto &lw : linkWeights)
  {
    if (lw.second > wthresh)
      badLinks.insert(lw.first);
  }
  return badLinks;
}

LinkSet FailureLocalization::IterativeWeightedFailedLinks_ThreeLevel(const ClassPathVec &paths, const LinkVec &all_links, double winc_lvl1, double winc_lvl2, double winc_lvl3,
                                                          double wdec, double wscale,
                                                          double wthresh, double pathscale, double normalization)
{
  LinkSet badLinks;
  ClassPathVec itPaths = paths;  // Copy path set because the iteration modifies it
  double maxWeight = 0;
  Link maxLink;
  do
  {
    maxWeight = 0;
    std::map<Link, double> linkWeights = CalculateLinkWeights_ThreeLevel(itPaths, all_links, winc_lvl1, winc_lvl2, winc_lvl3, wdec, wscale, pathscale, normalization);
    for (const auto &lw : linkWeights)
    {
      if (lw.second > maxWeight)
      {
        maxLink = lw.first;
        maxWeight = lw.second;
      }
    }
    if (maxWeight > wthresh)
    {
      badLinks.insert(maxLink);
      // Remove all paths that contain the failed link with max weight
      for (auto it = itPaths.begin(); it != itPaths.end();)
      {
        if (std::find(it->path.links.begin(), it->path.links.end(), maxLink) !=
            it->path.links.end())
        {
          it = itPaths.erase(it);
        }
        else
        {
          it++;
        }
      }
    }
  } while (maxWeight > wthresh);

  return badLinks;
}

LinkSet FailureLocalization::DirectWeightedFailedLinks_ThreeLevel(const ClassPathVec &paths, const LinkVec &all_links, double winc_lvl1, double winc_lvl2, double winc_lvl3,
                                                       double wdec, double wscale, double wthresh, double pathscale, double normalization)
{
  LinkSet badLinks;
  std::map<Link, double> linkWeights = CalculateLinkWeights_ThreeLevel(paths, all_links, winc_lvl1, winc_lvl2, winc_lvl3, wdec, wscale, pathscale, normalization);
  for (const auto &lw : linkWeights)
  {
    if (lw.second > wthresh)
      badLinks.insert(lw.first);
  }
  return badLinks;
}

LinkSet FailureLocalization::DirectLinkCount(const ClassPathVec &paths, double dlcthresh)
{
  LinkSet badLinks;
  std::map<Link, double> linkCounts = CalculateLinkCounts(paths);
  for (const auto &lw : linkCounts)
  {
    if (lw.second > dlcthresh)
      badLinks.insert(lw.first);
  }
  return badLinks;
}

LinkSet FailureLocalization::BadPathLinkWeight(const ClassPathVec &paths, const LinkVec &all_links, double winc,
                                                       double wdec, double wscale, double wthresh, double pathscale, double normalization)
{
  LinkSet badLinks;
  std::map<Link, double> linkWeights = CalculateLinkWeights_BadPathsOnly(paths, all_links, winc, wdec, wscale, pathscale, normalization);
  for (const auto &lw : linkWeights)
  {
    if (lw.second > wthresh)
      badLinks.insert(lw.first);
  }
  return badLinks;
}

LinkSet FailureLocalization::BadPathLinkWeight_ThreeLevel(const ClassPathVec &paths, const LinkVec &all_links, double winc_lvl1, 
                                                            double winc_lvl2, double winc_lvl3, 
                                                            double wdec, double wscale, double wthresh, double pathscale, double normalization)
{
  LinkSet badLinks;
  std::map<Link, double> linkWeights = CalculateLinkWeights_BadPathsOnly_ThreeLevel(paths, all_links, winc_lvl1, winc_lvl2, winc_lvl3, wdec, wscale, pathscale, normalization);
  for (const auto &lw : linkWeights)
  {
    if (lw.second > wthresh)
      badLinks.insert(lw.first);
  }
  return badLinks;
}


LinkSet FailureLocalization::DetectFailedLinks(const ClassPathVec &paths)
{
  LinkSet badLinks;

  for (auto &cp : paths)
  {
    if (cp.failed)
    {
      for (const Link &link : cp.path.links)
      {
        badLinks.insert(link);
      }
    }
  }

  return badLinks;
}


alglib::sparsematrix FailureLocalization::ConnectivityMatrixToSparseMatrix(const ConnectivityMatrix &connectivityMatrix){
    int num_rows = connectivityMatrix.size();
    int num_cols = connectivityMatrix[0].size();    
    alglib::sparsematrix a;
    alglib::sparsecreate(num_rows, num_cols, a);

    int row_counter = 0;
    for (auto &row : connectivityMatrix)
    {
        auto it = row.begin();
        while ((it = std::find_if(it, row.end(), [&] (int const &e) { return e == 1; }))
            != row.end())
        {
            alglib::sparseset(a, row_counter, std::distance(row.begin(), it), 1.0);
            it++;
        }
        row_counter++;
    }
    alglib::sparseconverttocrs(a);
    return a;
}

alglib::real_1d_array FailureLocalization::MeasurementVectorToReal1dArray(const MeasurementVector &measurementVector, bool isLoss){

    std::string resultVectorString = "[";
    int counter = 0;
    int vec_size = measurementVector.size();

    for (auto &value : measurementVector)
    {
        if (!isLoss){
            resultVectorString.append(std::to_string(value));
        // For loss measurements, rephrase the problem to a problem of estimating the packet delivery rates to avoid having log of 0
        } else {
            if (value < 0) {
              throw std::runtime_error("We have a loss rate of " + std::to_string(value) +
                                       " which is less than 0%. That is not possible.");
            } else if (value >= 1) {
                throw std::runtime_error("We have a loss rate of exactly or more than 100% which is not possible.");
            } else {
                resultVectorString.append(std::to_string(log(1-value)));
            } 
        }
        if (counter < vec_size-1){
            resultVectorString.append(",");
        }
        counter ++;
    }
    resultVectorString.append("]");
    real_1d_array b = resultVectorString.c_str();
    return b;
}


LinkSet FailureLocalization::LinearLSQR_ThreeLevel(const ConnectivityMatrix &connectivityMatrix, const MeasurementVector &measurementVector, const ReverseLinkIndexMap &reverse_link_index_map, double lossRateTh, uint32_t delayTh, double smallFailFactor, double largeFailFactor)
{
  LinkSet badLinks;
  return badLinks;
}

std::string LinkValueMapToString(LinkValueMap linkValueMap){

    std::stringstream ss; 
    ss << "[";
    for(std::map<Link,double>::iterator iter = linkValueMap.begin(); iter != linkValueMap.end(); ++iter)
    {
        Link link =  iter->first;
        double value = iter->second;
        ss << "(" << std::to_string(link.first) << "-" << std::to_string(link.second) << ":" << std::to_string(value) << ")";
    }
    ss << "]";
    return ss.str();
}


std::pair<LinkSet, LinkValueMap> FailureLocalization::LinearLSQR(
    const ConnectivityMatrix &connectivityMatrix, const MeasurementVector &measurementVector,
    const ReverseLinkIndexMap &reverse_link_index_map, bool localize_loss, double lossRateTh,
    uint32_t delayTh)
{
  // This example:
  // https://www.alglib.net/translator/man/manual.cpp.html#example_linlsqr_d_1

  LinkSet badLinks;
  LinkValueMap linkRatings;

  alglib::sparsematrix inputMatrix =  FailureLocalization::ConnectivityMatrixToSparseMatrix(connectivityMatrix);

  alglib::real_1d_array res_vec = FailureLocalization::MeasurementVectorToReal1dArray(measurementVector, localize_loss);

  alglib::linlsqrstate s;
  alglib::linlsqrreport rep;
  alglib::real_1d_array x; 
  try
    {
        alglib::linlsqrcreate(connectivityMatrix.size(), connectivityMatrix[0].size(), s);
        alglib::linlsqrsolvesparse(s, inputMatrix, res_vec);
        alglib::linlsqrresults(s, x, rep);
        //std::cout << "TerminiationType: " << std::to_string(int(rep.terminationtype)) <<std::endl;
        //std::cout << "Values : ";
        int counter = 0;
        while (counter < x.length()){

            /*
            if (1 - exp(x[counter]) >= lossRateTh) {
                std::cout << "(" << std::to_string(reverse_link_index_map.at(counter).first) << "->" << std::to_string(reverse_link_index_map.at(counter).second)  << "," << std::to_string(exp(x[counter])) << ")" << std::endl;
            }*/
            auto currLink = reverse_link_index_map.at(counter);
            if (localize_loss)
            {
              auto linkRating = 1 - exp(x[counter]);
              linkRatings[currLink] = linkRating;
              if (linkRating >= lossRateTh)
              {
                badLinks.insert(currLink);
                // std::cout << "(" << counter << "," << reverse_link_index_map.at(counter).first <<
                // "," << reverse_link_index_map.at(counter).second << "," << x[counter] << "), ";
              }
            }
            else
            {
              auto linkRating = x[counter];
              linkRatings[currLink] = linkRating;
              if (linkRating >= delayTh)
              {
                badLinks.insert(currLink);
                // std::cout << "(" << counter << "," << reverse_link_index_map.at(counter).first <<
                // "," << reverse_link_index_map.at(counter).second << "," << x[counter] << "), ";
              }
            }
            counter ++;
        } 

        //std::cout << std::endl;
        //std::cout << "Result: " << x.tostring(measurementVector.size()).c_str() << std::endl;

    }
  catch(alglib::ap_error e)
    { 
        std::cout << "AlgLib Error: " << e.msg << std::endl;
        throw std::runtime_error("AlgLib Error (see above)");
    }
    return std::make_pair(badLinks, linkRatings);
}

#ifdef USE_GUROBI
std::pair<LinkSet, LinkValueMap> FailureLocalization::LPWithSlack(const ClassPathVec &paths,
                                                                  const LinkVec &all_links,
                                                                  bool localize_loss,
                                                                  double lossRateTh,
                                                                  uint32_t delayTh)
{
  LinkSet badLinks;
  LinkValueMap linkRatings;

  try
  {
        // Create an environment
        GRBEnv env = GRBEnv(true);
        env.set(GRB_IntParam_OutputFlag, 0);  // Disable output (comment out to enable output)
        env.start();

        // Create an empty model
        GRBModel model = GRBModel(env);

        std::map<Link, GRBVar> linkVars;

        double upperBound =
            GRB_INFINITY;  // No upper bound for loss, because for link loss x, var(x) = -log(1-x)
        if (!localize_loss)
        {
            upperBound = 10000;  // More than 10s delay is improbable
        }

        for (const auto &link : all_links)
        {
            linkVars[link] = model.addVar(0.0, upperBound, 0.0, GRB_CONTINUOUS);
        }

        std::unique_ptr<GRBVar[]> positiveSlackVars =
            std::unique_ptr<GRBVar[]>(model.addVars(paths.size(), GRB_CONTINUOUS));

        std::unique_ptr<GRBVar[]> negativeSlackVars =
            std::unique_ptr<GRBVar[]>(model.addVars(paths.size(), GRB_CONTINUOUS));

        model.update();

        // Add objective: minimize sum of slack variables
        GRBLinExpr objExpr;
        std::unique_ptr<double[]> objCoeffs = std::make_unique<double[]>(paths.size());
        std::fill_n(objCoeffs.get(), paths.size(), 1.0);
        objExpr.addTerms(objCoeffs.get(), positiveSlackVars.get(), paths.size());
        objExpr.addTerms(objCoeffs.get(), negativeSlackVars.get(), paths.size());
        model.setObjective(objExpr, GRB_MINIMIZE);

        // Add constraints for each path
        uint32_t pathIndex = 0;
        if (localize_loss)
        {
            for (const auto &clp : paths)
            {
                GRBLinExpr pathExpr;
                for (const auto &link : clp.path.links)
                {
          pathExpr += linkVars[link];
                }

                pathExpr += positiveSlackVars[pathIndex];
                pathExpr -= negativeSlackVars[pathIndex];
                if (clp.measurement < 0.0)
                {
          throw std::runtime_error("Negative loss rate not allowed.");
                }
                else if (clp.measurement >= 1.0)
                {
          // throw std::runtime_error("Loss rate of 100% not allowed.");
          std::cout << "Warning: Loss rate of 100% not allowed. Skipping constraint..."
                    << std::endl;
                }
                else
                {
          model.addConstr(pathExpr == -log(1 - clp.measurement));
                }
                pathIndex++;
            }
        }
        else
        {
            for (const auto &clp : paths)
            {
                GRBLinExpr pathExpr;
                for (const auto &link : clp.path.links)
                {
          pathExpr += linkVars[link];
                }

                pathExpr += positiveSlackVars[pathIndex];
                pathExpr -= negativeSlackVars[pathIndex];
                if (clp.measurement < 0.0)
                {
          throw std::runtime_error("Negative delay not allowed.");
                }
                model.addConstr(pathExpr == clp.measurement);
                pathIndex++;
            }
        }

        // Optimize model
        model.optimize();

        if (localize_loss)
        {
            for (const auto &[link, var] : linkVars)
            {
              linkRatings[link] = 1 - exp(-var.get(GRB_DoubleAttr_X));
              if ((1 - exp(-var.get(GRB_DoubleAttr_X))) >= lossRateTh)
              {
                badLinks.insert(link);
              }
            }
        }
        else
        {
            for (const auto &[link, var] : linkVars)
            {
              linkRatings[link] = var.get(GRB_DoubleAttr_X);
              if (var.get(GRB_DoubleAttr_X) >= delayTh)
              {
                badLinks.insert(link);
              }
            }
        }
        // std::cout << "Gurobi obj. val: " << localize_loss << " " <<
        // model.get(GRB_DoubleAttr_ObjVal)
        // << std::endl;
  }
  catch (GRBException e)
  {
        std::cout << "Gurobi Error code = " << e.getErrorCode() << std::endl;
        std::cout << e.getMessage() << std::endl;
  }
  return std::make_pair(badLinks, linkRatings);
}
#endif


}  // namespace analysis
