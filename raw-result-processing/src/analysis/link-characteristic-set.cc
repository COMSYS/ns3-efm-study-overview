#include "link-characteristic-set.h"

#include "iostream"
#include "sim-ping-pair.h"

namespace analysis {

//----- LinkCharacteristicSet -----
LinkCharacteristicSet::LinkCharacteristicSet(uint32_t flowLengthTh, std::string classification_base_id)
{
  m_config.flowLengthTh = flowLengthTh;
  m_config.classification_base_id = classification_base_id;
}

LinkCharacteristicSet::LinkCharacteristicSet()
{
}

LinkCharacteristicSet LinkCharacteristicSet::CharacterizeAll(const simdata::SimResultSet &srs,
                                                            const std::set<uint32_t> &observerIds,
                                                            const std::map<uint32_t, std::set<uint32_t>> flowSelectionMap,
                                                            const EfmBitSet &bitCombis, 
                                                            uint32_t flowLengthTh,
                                                            bool core_links_only,
                                                            ClassificationMode classificationMode,
                                                            std::string classification_base_id,
                                                            double time_filter)
{

  if (classificationMode == ClassificationMode::PERFECT){
    return LinkCharacteristicSet();
    //throw std::invalid_argument("Characterization does not work with PERFECT mode.");
  }

  // First, fill the LinkIndexMap
  // Iterate over all links of the topology
  LinkVec all_links;
  if (core_links_only){
    all_links =  srs.GetCoreLinks();
  } else{
    all_links = srs.GetAllLinks();
  }
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

LinkCharacteristicSet LinkCharacteristicSet::Characterize(const simdata::SimResultSet &srs,
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

  LinkCharacteristicSet lcs(flowLengthTh, classification_base_id);
  lcs.m_config.flowIds = flowIds;
  lcs.m_config.observerSet.observers = observerIds;
  lcs.m_linkIndexMapping = link_index_map;
  lcs.m_reverseLinkIndexMapping = reverse_link_index_map;

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

    // Iterate over all observers on the flow path
    for (auto &obptr : fp)
    {
      uint32_t observerId = obptr->m_nodeId;

      // Check if observer should be used for classification
      // 1. Observer is in the observerSet
      // 2. Observer has selected the flow
      if (observerIds.find(observerId) != observerIds.end() && flowSelectionMap[observerId].count(fid))
      {
        // Check if flow is observed bidirectionally
        bool bidirectional = reverseLp.ContainsNode(observerId);

        // Generate and classify paths for each bit combination
        for (auto &bit : bitCombis)
        {
          if (IsActiveMmntBit(bit))
            continue;  // We handle active measurement bits later

          
          ConnectivityMatrix &connMatrix = lcs.m_connectivityMatrix[observerId][bit];
          MeasurementVector &measureVector = lcs.m_measurementVector[observerId][bit];

          /*
          if (srs.GetFlowStats(observerId, fid).totalPackets >= flowLengthTh){
          }*/
          // TODO: Consider adding a flow length threshold for stable measurements

          LinkPath path = lcs.GenerateUnidirBitPaths(observerId, bit, lp, reverseLp);
          double _mmnt = lcs.ExtractFlowMeasurement(obptr, fid, bit, time_filter);



          if ((_mmnt > 0)) {
            measureVector.push_back(_mmnt);
            connMatrix.push_back(LinkPathToConnectivityVector(path, lcs.m_linkIndexMapping));
          } else if (IsLossBit(bit)) {
            if (_mmnt < 0)
            {
                // std::cout << "Warning: Negative measurement " + std::to_string(_mmnt) + " for bit " +
                //                  efmbit_to_string(bit)
                //           << ". Correcting to 0." << std::endl;
                _mmnt = 0;
                negative_correction_count++;
            }
            measureVector.push_back(_mmnt);
            connMatrix.push_back(LinkPathToConnectivityVector(path, lcs.m_linkIndexMapping));
          }
          
          // It is hard to split bit path generation and classification for bidirectional flows
          // So let this method handle all of it (if the observer is bidirectional)
          if (bidirectional)
            lcs.HandleBidirBitPathClassification(srs, obptr, bit, fid, reverseFid, lp, reverseLp, time_filter);
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

  // Handle active measurement bits
  lcs.HandleActiveMeasurements(srs, observerIds, bitCombis, classification_base_id);

  return lcs;
}

void LinkCharacteristicSet::HandleActiveMeasurements(const simdata::SimResultSet &srs,
                                                 const std::set<uint32_t> &observerIds,
                                                 const EfmBitSet &bitCombis,
                                                 std::string classification_base_id)
{
  EfmBitSet filteredBits;
  for (auto &bit : bitCombis)
  {
    if (IsActiveMmntBit(bit))
      filteredBits.insert(bit);
  }

  for (auto oid : observerIds)
  {
    auto &clientpp = srs.GetObserverVP(oid)->GetClientPingPairs();

    for (auto &[targetId, pp] : clientpp)
    {
      // Ping client's measurements apply to the RT path from the ping client via the ping server
      // back to the ping client
      auto _lp = GenerateLinkPath(srs.GetPingPath(oid, targetId));
      if (!_lp.has_value())
        continue;
      auto _lp2 = GenerateLinkPath(srs.GetPingPath(targetId, oid));
      if (!_lp2.has_value())
        continue;

      LinkPath path = _lp.value().Append(_lp2.value());
      for (auto &bit : filteredBits)
      {
        ConnectivityMatrix &connMatrix = m_connectivityMatrix[oid][bit];
        MeasurementVector &measureVector = m_measurementVector[oid][bit];

        if (bit == EfmBit::PINGLSS)
        {
            connMatrix.push_back(LinkPathToConnectivityVector(path, m_linkIndexMapping));
            if (pp->GetRelativeLoss() < 0)
            {
              std::cout << "Warning: Negative ping Loss: " << std::to_string(pp->GetRelativeLoss())
                        << std::endl;
            }
            measureVector.push_back(pp->GetRelativeLoss());
        }
        else if (bit == EfmBit::PINGDLY)
        {
            connMatrix.push_back(LinkPathToConnectivityVector(path, m_linkIndexMapping));
            if (pp->GetAvgDelay().value() < 0)
            {
              std::cout << "Warning: Negative ping Delay: "
                        << std::to_string(pp->GetAvgDelay().value()) << std::endl;
            }
            measureVector.push_back(pp->GetAvgDelay().value());
        }
        else
        {
          throw std::runtime_error("Unknown active measurement bit.");
        }
      }
    }

    auto &serverpp = srs.GetObserverVP(oid)->GetServerPingPairs();
    for (auto &[targetId, pp] : serverpp)
    {
      // Ping server's measurements apply to the path from the ping client to the ping server
      auto _lp = GenerateLinkPath(srs.GetPingPath(targetId, oid));
      if (!_lp.has_value())
        continue;
      LinkPath etePath = _lp.value();
      for (auto &bit : filteredBits)
      {
        ConnectivityMatrix &connMatrix = m_connectivityMatrix[oid][bit];
        MeasurementVector &measureVector = m_measurementVector[oid][bit];

        if (bit == EfmBit::PINGLSS)
        {
            connMatrix.push_back(LinkPathToConnectivityVector(etePath, m_linkIndexMapping));
            if (pp->GetRelativeLoss() < 0)
            {
              std::cout << "Warning: Negative ping loss: " << std::to_string(pp->GetRelativeLoss())
                        << std::endl;
            }
            measureVector.push_back(pp->GetRelativeLoss());
          
        }
        else if (bit == EfmBit::PINGDLY)
        {
            connMatrix.push_back(LinkPathToConnectivityVector(etePath, m_linkIndexMapping));
            if (pp->GetAvgDelay().value() < 0)
            {
              std::cout << "Warning: Negative ping delay: "
                        << std::to_string(pp->GetAvgDelay().value()) << std::endl;
            }
            measureVector.push_back(pp->GetAvgDelay().value());
        }
        else
        {
          throw std::runtime_error("Unknown active measurement bit.");
        }
      }
    }
  }
}

std::optional<ConnMatrixMeasVecPair> LinkCharacteristicSet::GetConnectivityMatrixMeasurementVector(
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

ConnMatrixMeasVecPair LinkCharacteristicSet::GetConnectivityMatrixMeasurementVector(const std::set<uint32_t> &observerIds,
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

ConnectivityVector LinkCharacteristicSet::LinkPathToConnectivityVector(const LinkPath &flowPath, const LinkIndexMap &linkIndexMap) 
{
    // Initialize connectivityVector to 0 at all positions 
    ConnectivityVector connectivityVector(linkIndexMap.size(), 0);
    // Add a one for each link that is used on the path 
    for (auto &link : flowPath.links)
    {
        if (linkIndexMap.count(link)){
            connectivityVector[linkIndexMap.at(link)] = 1;
        }
    }
    return connectivityVector;
}



LinkPath LinkCharacteristicSet::GenerateUnidirBitPaths(uint32_t observerId, EfmBit bits,
                                                   LinkPath &flowPath,
                                                   LinkPath &reverseFlowPath) const
{
  switch (bits)
  {
    case EfmBit::SEQ:
    case EfmBit::Q:
      return flowPath.GetUpToX(observerId);
    case EfmBit::L:
    case EfmBit::TCPRO:
      return flowPath;
    case EfmBit::T:
      return flowPath.Append(reverseFlowPath);
    case EfmBit::R:
      return reverseFlowPath.Append(flowPath.GetUpToX(observerId));
    case EfmBit::SPIN:
      return flowPath.Append(reverseFlowPath);
    case EfmBit::QL:
      return flowPath.GetPathFromXToEnd(observerId);
    case EfmBit::QR:
      return reverseFlowPath;
    case EfmBit::QT:
      return flowPath.GetPathFromXToEnd(observerId)
          .Append(reverseFlowPath);  // T loss - Q loss => three quarter RT loss
    case EfmBit::LT:
      return reverseFlowPath;
    case EfmBit::TCPDART:
      return flowPath.GetPathFromXToEnd(observerId).Append(reverseFlowPath.GetUpToX(observerId));
    default:
      throw std::runtime_error("Should never happen.");
      break;
  }
}

void LinkCharacteristicSet::HandleBidirBitPathClassification(
    const simdata::SimResultSet &srs, const simdata::ObsvVantagePointPointer &observer, EfmBit bits, 
    uint32_t flowId, uint32_t reverseFlowId, LinkPath &flowPath, LinkPath &reverseFlowPath, double time_filter)
{
  uint32_t observerId = observer->m_nodeId;
  simdata::SimObsvFlowPointer flow = observer->GetFlow(flowId);
  simdata::SimObsvFlowPointer reverseFlow = observer->GetFlow(reverseFlowId);

  // Important to generate at least an empty vector, even if no paths are generated
  ConnectivityMatrix &connMatrix = m_connectivityMatrix[observerId][bits];
  MeasurementVector &measureVector = m_measurementVector[observerId][bits];
  
  switch (bits)
  {
    case EfmBit::T:
    {
      // Compute half-RT loss (O-C-O or O-S-O)
      // The half measurement for a C-S flow contains the O-C-O loss and vice versa
    /*
    if (srs.GetFlowStats(observerId, flowId).totalPackets >= m_config.flowLengthTh){
    }*/
    // TODO: Consider adding a flow length threshold for stable measurements

      LinkPath path = reverseFlowPath.GetPathFromXToEnd(observerId).Append(flowPath.GetUpToX(observerId));
      connMatrix.push_back(LinkPathToConnectivityVector(path, m_linkIndexMapping));
      measureVector.push_back(flow->GetRelativeTBitHalfLoss());
      break;
    }
    case EfmBit::SPIN:
    {
      // Compute end-to-end delay
        /*
        if (srs.GetFlowStats(observerId, flowId).totalPackets >= m_config.flowLengthTh){
        }*/
        LinkPath path = reverseFlowPath.GetPathFromXToEnd(observerId).Append(flowPath.GetUpToX(observerId));;
        double measurement_result = flow->GetAvgSpinEtEDelay(time_filter).value_or(0);
        if (measurement_result > 0) {
            connMatrix.push_back(LinkPathToConnectivityVector(path, m_linkIndexMapping));
            measureVector.push_back(flow->GetAvgSpinEtEDelay(time_filter).value_or(0));
        }
      break;
    }
    case EfmBit::QR:
    {
      // Compute downstream loss
     /*
        if (srs.GetFlowStats(observerId, flowId).totalPackets >= m_config.flowLengthTh){
        }*/
        LinkPath path = flowPath.GetPathFromXToEnd(observerId);
        double uloss = flow->GetRelativeQBitLoss();
        double ulossRev = reverseFlow->GetRelativeQBitLoss();
        double tqlossRev = reverseFlow->GetRelativeRBitLoss();
        double dsl = (((tqlossRev - ulossRev) / (1 - ulossRev)) - uloss) / (1 - uloss);
        if (dsl < 0){
            dsl = 0.0;
        }
        connMatrix.push_back(LinkPathToConnectivityVector(path, m_linkIndexMapping));
        measureVector.push_back(dsl);


      // Compute half RT loss
    /*
        if (srs.GetFlowStats(observerId, flowId).totalPackets >= m_config.flowLengthTh){
        }*/
        LinkPath path2 = reverseFlowPath.GetPathFromXToEnd(observerId).Append(flowPath.GetUpToX(observerId));
      // We only compute the O-X-O loss here for our X-Y flow, the O-Y-O loss is handled by the
      // computation for the reverse flow

        double loss = ((flow->GetRelativeRBitLoss() - ulossRev) / (1 - ulossRev));
        if (loss < 0){
            loss = 0.0;
        }
        connMatrix.push_back(LinkPathToConnectivityVector(path, m_linkIndexMapping));
        measureVector.push_back(loss);
      break;
    }
    case EfmBit::QT:
    {
     /*
        if (srs.GetFlowStats(observerId, flowId).totalPackets >= m_config.flowLengthTh){
        }*/
        LinkPath path = flowPath.GetPathFromXToEnd(observerId).Append(reverseFlowPath);

        // Compute downstream loss using Tbit-half-loss
        double loss = ((reverseFlow->GetRelativeTBitHalfLoss() - reverseFlow->GetRelativeQBitLoss()) /
             (1 - reverseFlow->GetRelativeQBitLoss()));

        if (loss < 0)
        {
          loss = 0.0;
        }

        connMatrix.push_back(LinkPathToConnectivityVector(path, m_linkIndexMapping));
        measureVector.push_back(loss);
      break;
    }
    default:
      break;
  }
}

double LinkCharacteristicSet::ExtractFlowMeasurement(const simdata::ObsvVantagePointPointer &observer, uint32_t flowId, EfmBit bits, double time_filter)
{
  simdata::SimObsvFlowPointer flow = observer->GetFlow(flowId);
  switch (bits)
  {
    case EfmBit::SEQ:
      return flow->GetRelativeSeqLoss(); 
      break;
    case EfmBit::Q:
      return flow->GetRelativeQBitLoss();
      break;
    case EfmBit::L:
      return flow->GetRelativeLBitLoss();
      break;
    case EfmBit::T:
      return flow->GetRelativeTBitFullLoss();
      break;
    case EfmBit::R:
      return flow->GetRelativeRBitLoss();
      break;
    case EfmBit::SPIN:
      return flow->GetAvgSpinRTDelay(time_filter).value_or(0);
      break;
    case EfmBit::QL:
    {
      double uloss = flow->GetRelativeQBitLoss();
      // TODO: Consider loss rate adaption (e.g., increase end-to-end loss if less than upstream
      // loss; depends on type of flow (ACK dominated, ...))
      double loss = (flow->GetRelativeLBitLoss() - uloss) / (1 - uloss); // Based on EFM draft
      if (loss < 0){
        loss = 0.0;
      } 
      return loss;
      break;
    }
    case EfmBit::QR:
    {
      double uloss = flow->GetRelativeQBitLoss();
      double loss = (flow->GetRelativeRBitLoss() - uloss) / (1 - uloss); // Based on EFM draft
      if (loss < 0){
        loss = 0.0;
      } 
      return loss;
      break;
    }
    case EfmBit::QT:
    {
      double uloss = flow->GetRelativeQBitLoss();
      double loss = (flow->GetRelativeTBitFullLoss() - uloss) / (1 - uloss); // Based on EFM draft
      return loss;
      break;
    }
    case EfmBit::LT:
    {
      double eloss = flow->GetRelativeLBitLoss();
      double loss = (flow->GetRelativeTBitFullLoss() - eloss) / (1 - eloss); // Based on EFM draft
      return loss;
      break;
    }
    case EfmBit::TCPRO:
      return flow->GetRelativeTcpReordering();
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

}  // namespace analysis
