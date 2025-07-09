#include "combined-flow-set.h"

#include "iostream"
#include "sim-ping-pair.h"

namespace analysis {

//----- CombinedFlowSet -----
CombinedFlowSet::CombinedFlowSet(uint32_t flowLengthTh, std::string classification_base_id)
{
  m_config.flowLengthTh = flowLengthTh;
  m_config.classification_base_id = classification_base_id;
}

CombinedFlowSet::CombinedFlowSet()
{
}

std::string ConnectivityVectorToString(ConnectivityVector vector){

    std::stringstream ss; 
    ss << "Vector: [";
    for (auto &element : vector){
        ss << std::to_string(element) << ",";
    }
    ss << "]";
    return ss.str();
}

std::string MeasurementVectorToString(MeasurementVector vector){
    std::stringstream ss; 
    ss << "Vector: [";
    for (auto &element : vector){
        ss << std::to_string(element) << ",";
    }
    ss << "]";
    return ss.str();
}

std::string ConnectivityVectorToStringWithLinkMapping(ConnectivityVector vector, ReverseLinkIndexMap linkIndexMap){

    std::stringstream ss; 
    ss << "Vector: [";
    for (int index = 0; index < vector.size(); ++index)
    {
        ss << std::to_string(vector[index]) << "(" << LinkToString(linkIndexMap.at(index)) << ")" << ",";
    }
    ss << "]";
    return ss.str();
}

CombinedFlowSet CombinedFlowSet::CharacterizeAll(const simdata::SimResultSet &srs,
                                                            const std::set<uint32_t> &observerIds,
                                                            const std::map<uint32_t, std::set<uint32_t>> flowSelectionMap,
                                                            const EfmBitSet &bitCombis, 
                                                            uint32_t flowLengthTh,
                                                            ClassificationMode classificationMode,
                                                            std::string classification_base_id,
                                                            double time_filter)
{

  if (classificationMode == ClassificationMode::PERFECT){
    return CombinedFlowSet();
    //throw std::invalid_argument("Characterization does not work with PERFECT mode.");
  }

  // First, fill the LinkIndexMap
  // Iterate over all links of the topology
  LinkVec all_links =  srs.GetCoreLinks();
  LinkIndexMap link_index_map;
  ReverseLinkIndexMap reverse_link_index_map;
  uint32_t link_counter = 0;
  for(auto iter = all_links.begin(); iter < all_links.end(); iter++)
  {
    if (link_index_map.insert(std::pair<Link,uint32_t>(*iter,link_counter)).second == false){
        throw std::runtime_error("Link already in LinkIndexMap");
    }
    if (reverse_link_index_map.insert(std::pair<uint32_t,Link>(link_counter,*iter)).second == false){
        throw std::runtime_error("Index already in ReverseLinkIndexMap");
    }

    /*
    if (link_counter == 266){
        std::cout << "Poss broken link: " << std::to_string(iter->first) << "," << std::to_string(iter->second) << std::endl;
    
    }*/
    link_counter++;
  }
    /*
    std::cout << "[";
      for(std::map<Link,uint32_t>::iterator iter = link_index_map.begin(); iter != link_index_map.end(); ++iter)
    {
        std::cout << "(" << LinkToString(iter->first) << "," << std::to_string(iter->second) << ")";
    }
    std::cout << std::endl;

    std::cout << "[";
      for(std::map<uint32_t, Link>::iterator iter = reverse_link_index_map.begin(); iter != reverse_link_index_map.end(); ++iter)
    {
        std::cout << "(" << LinkToString(iter->second) << "," << std::to_string(iter->first) << ")";
    }
    std::cout << std::endl;
    */
//typedef std::map<Link, uint32_t> LinkIndexMap;
//typedef std::map<uint32_t, Link> ReverseLinkIndexMap;

  // Now we have an index map of all links so their position in Ax - b = 0 is fixed

  // Collect all flows 
  std::set<uint32_t> flowIds;
  for (auto &oid : observerIds)
  {
    auto fids = srs.GetObserverFlowIds(oid);
    flowIds.insert(fids.begin(), fids.end());
  }
  return Characterize(srs, observerIds, flowIds, flowSelectionMap, bitCombis, link_index_map, reverse_link_index_map, flowLengthTh, classification_base_id, time_filter);
}

CombinedFlowSet CombinedFlowSet::Characterize(const simdata::SimResultSet &srs,
                                              const std::set<uint32_t> &observerIds,
                                              const std::set<uint32_t> &flowIds,
                                              std::map<uint32_t, std::set<uint32_t>> flowSelectionMap,
                                              const EfmBitSet &bitCombis, 
                                              const LinkIndexMap link_index_map, 
                                              const ReverseLinkIndexMap reverse_link_index_map, 
                                              uint32_t flowLengthTh,
                                              std::string classification_base_id,
                                              double time_filter)
{
  if (observerIds.empty() || flowIds.empty() || bitCombis.empty() || link_index_map.empty())
    throw std::invalid_argument("Empty input arg.");

  CombinedFlowSet cfs(flowLengthTh, classification_base_id);
  cfs.m_config.flowIds = flowIds;
  cfs.m_config.observerSet.observers = observerIds;
  cfs.m_linkIndexMapping = link_index_map;
  cfs.m_reverseLinkIndexMapping = reverse_link_index_map;

  uint32_t negative_correction_count = 0;

  // Iterate over all flows
  for (auto &fid : flowIds)
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

    /*
    std::cout << LinkPathToString(lp) << std::endl;
    //[(44,14)(14,13)(13,16)(16,1)(1,4)(4,64)]
    std::cout << ObsvVantagePointPointerVectorToString(fp) << std::endl;
    //[(44)(14)(13)(16)(1)(4)(64)]
    std::cout << LinkPathToString(reverseLp) << std::endl;
    //[(64,4)(4,1)(1,16)(16,13)(13,14)(14,44)]
    std::cout << ObsvVantagePointPointerVectorToString(reverseFp) << std::endl;
    //[(64)(4)(1)(16)(13)(14)(44)]
    */
    // First, iterate over available bit combinations
    for (auto &bit : bitCombis)
    {
        // We only do this for some selected measurement bits
        if (IsActiveMmntBit(bit) || !IsCombinationBit(bit))
        continue; 

        if (bit == EfmBit::Q){
            std::vector<FC_TupleQBit> measurementCollectionVector;
            for (auto &obptr : fp)
            {
                uint32_t observerId = obptr->m_nodeId;
                // Check if observer should be used for classification
                // 1. Observer is in the observerSet
                // 2. Observer has selected the flow
                // 3. Observer is bidirectional (required by both variants)
                if (observerIds.find(observerId) != observerIds.end() && flowSelectionMap[observerId].count(fid))
                {
                    LinkPath path = cfs.GeneratePathVisibilityForFlowCombination(observerId, bit, lp, reverseLp);
                    std::pair<uint32_t,uint32_t> _mmnt = cfs.ExtractFlowMeasurementPair(obptr, fid, bit, time_filter);
                    // second == 0 means that we have not registered any packets... does not make that much sense
                    if (_mmnt.second > 0){
                        FC_TupleQBit collected = std::make_tuple(observerId, path, _mmnt);
                        measurementCollectionVector.push_back(collected);
                    }
                }
            }
            // Now we have all measurements collected
            // Decide which actual measurement we get from this
            // If we only have a single measurement, we cannot combine anything
            if (measurementCollectionVector.size() < 2){
                continue;
            }

            // This assumes that the longer paths are earlier in the measurementCollectionVector
            for (int index = measurementCollectionVector.size()-1; index > 0; index--)
            {
                LinkPath currentPath = std::get<1>(measurementCollectionVector[index]);
                std::pair<uint32_t,uint32_t> currentMeasurement = std::get<2>(measurementCollectionVector[index]);//[2];

                LinkPath nextPath = std::get<1>(measurementCollectionVector[index-1]);//[1];
                std::pair<uint32_t,uint32_t> nextMeasurement = std::get<2>(measurementCollectionVector[index-1]);//[2];

                LinkPath linkDifference = cfs.GetLinkPathDifference(currentPath, nextPath);


                // Determine the difference in packet loss
                // Check if the baseline of transmitted packet is the same
                if (currentMeasurement.second > nextMeasurement.second || currentMeasurement.first > currentMeasurement.second) {
                    // There are fewer packets / missing q bits for the later measurement which should not really happen.
                    continue;
                } 
                
                uint32_t loss_difference = currentMeasurement.first - nextMeasurement.first;
                // Account for the packets that were lost before the link
                double loss_difference_relative_shit = (double)loss_difference / ((double)currentMeasurement.second - (double)nextMeasurement.first);

                uint32_t observerId = std::get<0>(measurementCollectionVector[index]);
                ConnectivityMatrix &connMatrix = cfs.m_connectivityMatrix[observerId][bit];
                MeasurementVector &measureVector = cfs.m_measurementVector[observerId][bit];
                connMatrix.push_back(LinkPathToConnectivityVector(linkDifference, cfs.m_linkIndexMapping));
                measureVector.push_back(loss_difference_relative_shit);

                /*
                if (loss_difference != 0){
                    std::cout << LinkPathToString(currentPath) << ":" << std::to_string(currentMeasurement.first) << ":" << std::to_string(currentMeasurement.second) << " to " << LinkPathToString(nextPath) << ":" << std::to_string(nextMeasurement.first) << ":" << std::to_string(nextMeasurement.second) << std::endl; 
                    std::cout << "Result ->" << LinkPathToString(linkDifference) << ":" << std::to_string(loss_difference) << ":" << std::to_string(loss_difference_relative_shit) << std::endl;
                }*/
                //std::cout << "Pushback done" << std::endl;
            }
        }

        if ((bit == EfmBit::TCPDART) || (bit == EfmBit::SPIN)){

            bool useForwardPath = true;

            if (useForwardPath){
                std::vector<FC_Tuple> measurementCollectionVector;
                for (auto &obptr : fp)
                {
                    uint32_t observerId = obptr->m_nodeId;
                    // Check if observer should be used for classification
                    // 1. Observer is in the observerSet
                    // 2. Observer has selected the flow
                    // 3. Observer is bidirectional (required by both variants)
                    if (observerIds.find(observerId) != observerIds.end() && flowSelectionMap[observerId].count(fid) && reverseLp.ContainsNode(observerId))
                    {
                        LinkPath path = cfs.GeneratePathVisibilityForFlowCombination(observerId, bit, lp, reverseLp);
                        double _mmnt = cfs.ExtractFlowMeasurement(obptr, fid, bit, time_filter);
                        if (_mmnt > 0) {
                            FC_Tuple collected = std::make_tuple(observerId, path, _mmnt);
                            measurementCollectionVector.push_back(collected);
                        }
                    }
                }
                // Now we have all measurements collected
                // Decide which actual measurement we get from this
                // If we only have a single measurement, we cannot combine anything
                if (measurementCollectionVector.size() < 2){
                    continue;
                }

                if (bit == EfmBit::TCPDART){
                    std::reverse(measurementCollectionVector.begin(), measurementCollectionVector.end());
                }
                // This assumes that the longer paths are earlier in the measurementCollectionVector
                for (int index = measurementCollectionVector.size()-1; index > 0; index--)
                {
                    LinkPath currentPath = std::get<1>(measurementCollectionVector[index]);
                    double currentMeasurement = std::get<2>(measurementCollectionVector[index]);//[2];

                    LinkPath nextPath = std::get<1>(measurementCollectionVector[index-1]);//[1];
                    double nextMeasurement = std::get<2>(measurementCollectionVector[index-1]);//[2];

                    //std::cout << LinkPathToString(currentPath) << ":" << std::to_string(currentMeasurement) << " to " << LinkPathToString(nextPath) << ":" << std::to_string(nextMeasurement) << std::endl; 
                    LinkPath linkDifference = cfs.GetLinkPathDifference(currentPath, nextPath);
                    double measurementDifference = currentMeasurement - nextMeasurement;
                    if (measurementDifference < 0){
                        continue;
                    }
                    //std::cout << "Result ->" << LinkPathToString(linkDifference) << ":" << std::to_string(measurementDifference) << std::endl;


                    uint32_t observerId = std::get<0>(measurementCollectionVector[index]);
                    ConnectivityMatrix &connMatrix = cfs.m_connectivityMatrix[observerId][bit];
                    MeasurementVector &measureVector = cfs.m_measurementVector[observerId][bit];
                    connMatrix.push_back(LinkPathToConnectivityVector(linkDifference, cfs.m_linkIndexMapping));
                    measureVector.push_back(measurementDifference);
                    //std::cout << "Pushback done" << std::endl;
                }
            }

            // We do not need the reverse path.
            // The reverse flow should be covered explicitly while iterating through all flow IDs 
            bool useReversePath = false;

            if (useReversePath){
                std::vector<FC_Tuple> measurementCollectionVectorReversePath;
                for (auto &obptr : reverseFp)
                {
                    uint32_t observerId = obptr->m_nodeId;
                    // Check if observer should be used for classification
                    // 1. Observer is in the observerSet
                    // 2. Observer has selected the flow
                    // 3. Observer is bidirectional (required by both variants)
                    if (observerIds.find(observerId) != observerIds.end() && flowSelectionMap[observerId].count(reverseFid) && lp.ContainsNode(observerId))
                    {
                        LinkPath path = cfs.GeneratePathVisibilityForFlowCombination(observerId, bit, reverseLp, lp);
                        double _mmnt = cfs.ExtractFlowMeasurement(obptr, reverseFid, bit, time_filter);
                        if (_mmnt > 0) {
                            FC_Tuple collected = std::make_tuple(observerId, path, _mmnt);
                            measurementCollectionVectorReversePath.push_back(collected);
                            //measurementCollectionVectorReversePath.push_back({path, _mmnt});
                            //std::cout << std::to_string(_mmnt);
                        }
                    }
                }
                //std::cout << std::endl;

                // Now we have all measurements collected
                // Decide which actual measurement we get from this
                // If we only have a single measurement, we cannot combine anything
                if (measurementCollectionVectorReversePath.size() < 2){
                    continue;
                }

                if (bit == EfmBit::TCPDART){
                    std::reverse(measurementCollectionVectorReversePath.begin(), measurementCollectionVectorReversePath.end());
                }

                // This assumes that the longer paths are earlier in the measurementCollectionVector
                for (int index = measurementCollectionVectorReversePath.size()-1; index > 0; index--)
                {
                    LinkPath currentPath = std::get<1>(measurementCollectionVectorReversePath[index]);
                    double currentMeasurement = std::get<2>(measurementCollectionVectorReversePath[index]);//[2];

                    LinkPath nextPath = std::get<1>(measurementCollectionVectorReversePath[index-1]);//[1];
                    double nextMeasurement = std::get<2>(measurementCollectionVectorReversePath[index-1]);//[2];

                    //std::cout << LinkPathToString(currentPath) << ":" << std::to_string(currentMeasurement) << " to " << LinkPathToString(nextPath) << ":" << std::to_string(nextMeasurement) << std::endl; 

                    LinkPath linkDifference = cfs.GetLinkPathDifference(currentPath, nextPath);
                    double measurementDifference = currentMeasurement - nextMeasurement;
                    if (measurementDifference < 0){
                        continue;
                    }
                    //std::cout << "Result ->" << LinkPathToString(linkDifference) << ":" << std::to_string(measurementDifference) << std::endl;

                    uint32_t observerId = std::get<0>(measurementCollectionVectorReversePath[index]);
                    ConnectivityMatrix &connMatrix = cfs.m_connectivityMatrix[observerId][bit];
                    MeasurementVector &measureVector = cfs.m_measurementVector[observerId][bit];
                    //ConnectivityVector vector = LinkPathToConnectivityVector(linkDifference, cfs.m_linkIndexMapping);
                    //std::cout << ConnectivityVectorToStringWithLinkMapping(vector, cfs.m_reverseLinkIndexMapping) << std::endl;
                    //connMatrix.push_back(vector);
                    //std::cout << measurementDifference << std::endl;
                    connMatrix.push_back(LinkPathToConnectivityVector(linkDifference, cfs.m_linkIndexMapping));
                    measureVector.push_back(measurementDifference);
                    //std::cout << "Pushback done" << std::endl;
                }
            }
        }
    }
  }

  if (negative_correction_count > 0)
  {
    std::cout << "Warning: Corrected " + std::to_string(negative_correction_count) +
                     " negative unidirectional non-active measurements to 0."
              << std::endl;
  }

  return cfs;
}

std::optional<ConnMatrixMeasVecPair> CombinedFlowSet::GetConnectivityMatrixMeasurementVector(
    uint32_t observerId, EfmBit bits) const
{
  auto obsvIt = m_connectivityMatrix.find(observerId);
  if (obsvIt == m_connectivityMatrix.end())
    return std::nullopt;

  auto efmBitIt = obsvIt->second.find(bits);
  if (efmBitIt == obsvIt->second.end())
    return std::nullopt;

  auto obsvIt2 = m_measurementVector.find(observerId);
  if (obsvIt2 == m_measurementVector.end())
    return std::nullopt;

  auto efmBitIt2 = obsvIt2->second.find(bits);
  if (efmBitIt2 == obsvIt2->second.end())
    return std::nullopt;

  ConnMatrixMeasVecPair cmmvpair(efmBitIt->second,efmBitIt2->second);
  return cmmvpair;
}

ConnMatrixMeasVecPair CombinedFlowSet::GetConnectivityMatrixMeasurementVector(const std::set<uint32_t> &observerIds,
                                                   const EfmBitSet &bitCombis) const
{
  ConnectivityMatrix connMatr;
  MeasurementVector measVec;
  for (auto &oid : observerIds)
  {
    for (auto &bit : bitCombis)
    {
      std::optional<ConnMatrixMeasVecPair> _conMatMeaVecPair =
          GetConnectivityMatrixMeasurementVector(oid, bit);
      if (!_conMatMeaVecPair.has_value())
        continue;
      ConnMatrixMeasVecPair conMatMeaVecPair = _conMatMeaVecPair.value();
      connMatr.insert(connMatr.end(), conMatMeaVecPair.first.begin(), conMatMeaVecPair.first.end());
      measVec.insert(measVec.end(), conMatMeaVecPair.second.begin(), conMatMeaVecPair.second.end());
    }
  }
  ConnMatrixMeasVecPair cmmvpair(connMatr,measVec);
  return cmmvpair;
}

ConnectivityVector CombinedFlowSet::LinkPathToConnectivityVector(const LinkPath &flowPath, const LinkIndexMap &linkIndexMap) 
{
    // Initialize connectivityVector to 0 at all positions 
    ConnectivityVector connectivityVector(linkIndexMap.size(), 0);
    // Add a one for each link that is used on the path 
    for (auto &link : flowPath.links)
    {
        connectivityVector[linkIndexMap.at(link)] = 1;
    }
    return connectivityVector;
}



LinkPath CombinedFlowSet::GeneratePathVisibilityForFlowCombination(uint32_t observerId, EfmBit bits,
                                                   LinkPath &flowPath,
                                                   LinkPath &reverseFlowPath) const
{
  switch (bits)
  {
    case EfmBit::Q:
      return flowPath.GetUpToX(observerId);
    case EfmBit::SPIN:
        return flowPath.GetUpToX(observerId).Append(reverseFlowPath.GetPathFromXToEnd(observerId));
    case EfmBit::TCPDART:
        return flowPath.GetPathFromXToEnd(observerId).Append(reverseFlowPath.GetUpToX(observerId));
    default:
      throw std::runtime_error("Should never happen.");
      break;
  }
}

LinkPath CombinedFlowSet::GetLinkPathDifference(LinkPath &longerPath, LinkPath &shorterPath) const
{
    if (longerPath.links.size() <= shorterPath.links.size()){
      throw std::runtime_error("Shorter Path is actually longer or equal in size.");
    }
    for (auto link : shorterPath.links){
        if (!longerPath.ContainsLink(link)){
            throw std::runtime_error("Shorter path not fully included in longer path.");
        }
    }
    LinkPath pathDifference;
    //std::cout << "Longer Path: " << LinkPathToString(longerPath) << ", shorter Path: " << LinkPathToString(shorterPath) << std::endl;
    for (auto link: longerPath.links){
        //std::cout << "Test link " << LinkToString(link) << std::endl;
        if (!shorterPath.ContainsLink(link)){
            //std::cout << std::to_string(link.first) << "," << std::to_string(link.second) << std::endl;
            pathDifference.links.push_back(link);
        }
    }
    return pathDifference;
}


double CombinedFlowSet::ExtractFlowMeasurement(const simdata::ObsvVantagePointPointer &observer, uint32_t flowId, EfmBit bits, double time_filter)
{
  simdata::SimObsvFlowPointer flow = observer->GetFlow(flowId);
  switch (bits)
  {
    case EfmBit::SPIN:
      return flow->GetAvgSpinEtEDelay(time_filter).value_or(0);
      break;
    case EfmBit::TCPDART:
      return flow->GetAvgTcpHRTDelay().value_or(0);
      break;
    default:
      throw std::runtime_error("Should never happen.");
      break;
  }
  return 0.0;
}

std::pair<uint32_t,uint32_t> CombinedFlowSet::ExtractFlowMeasurementPair(const simdata::ObsvVantagePointPointer &observer, uint32_t flowId, EfmBit bits, double time_filter)
{
  simdata::SimObsvFlowPointer flow = observer->GetFlow(flowId);
  switch (bits)
  {
    case EfmBit::Q:
      return {flow->GetAbsoluteQBitLoss(), flow->GetAbsoluteQBitPacketCount()};
      break;
    default:
      throw std::runtime_error("Should never happen.");
      break;
  }
  return {0.0,0.0};
}

}  // namespace analysis
