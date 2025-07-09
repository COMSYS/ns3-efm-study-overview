#include "classified-path-set.h"

#include "iostream"
#include "sim-ping-pair.h"

namespace analysis {

void to_json(nlohmann::json &jsn, const ObserverSet &obsSet)
{
  jsn["observers"] = obsSet.observers;
  jsn["metadata"] = nlohmann::json::parse(obsSet.metadata);
}

void from_json(const nlohmann::json &jsn, ObserverSet &obsSet)
{
  jsn.at("observers").get_to(obsSet.observers);
  if (jsn.contains("metadata"))
    obsSet.metadata = jsn.at("metadata").dump();
}

//----- ClassifiedPathSet -----

ClassifiedPathSet::ClassifiedPathSet(double lossRateTh, uint32_t delayTh, uint32_t flowLengthTh,
                                     ClassificationMode classificationMode,
                                     std::string classification_base_id)
{
  m_config.lossRateTh = lossRateTh;
  m_config.delayTh = delayTh;
  m_config.flowLengthTh = flowLengthTh;
  m_config.classificationMode = classificationMode;
  m_config.classification_base_id = classification_base_id;
}

/*
ClassifiedPathSet ClassifiedPathSet::ClassifyAll(const simdata::SimResultSet &srs,
                                                 const EfmBitSet &bitCombis, double lossRateTh,
                                                 uint32_t delayTh, uint32_t flowLengthTh,
                                                 ClassificationMode classificationMode,
                                                 std::string classification_base_id,
                                                 double smallFailFactor,
                                                 double largeFailFactor,
                                                 double time_filter)
{
  return Classify(srs, srs.GetObserverVPIds(true, true), srs.GetObserverFlowIds(true), bitCombis,
                  lossRateTh, delayTh, flowLengthTh, classificationMode, classification_base_id, smallFailFactor, largeFailFactor, time_filter);
}*/

ClassifiedPathSet ClassifiedPathSet::ClassifyAll(const simdata::SimResultSet &srs,
                                                 const std::set<uint32_t> &observerIds,
                                                 const std::map<uint32_t, std::set<uint32_t>> flowSelectionMap,
                                                 const EfmBitSet &bitCombis, double lossRateTh,
                                                 uint32_t delayTh, uint32_t flowLengthTh,
                                                 ClassificationMode classificationMode,
                                                 std::string classification_base_id,
                                                 double smallFailFactor,
                                                 double largeFailFactor,
                                                 double time_filter)
{
  std::set<uint32_t> flowIds;
  for (auto &oid : observerIds)
  {
    auto fids = srs.GetObserverFlowIds(oid);
    flowIds.insert(fids.begin(), fids.end());
  }
  return Classify(srs, observerIds, flowIds, flowSelectionMap, bitCombis, lossRateTh, delayTh, flowLengthTh,
                  classificationMode, classification_base_id, smallFailFactor, largeFailFactor, time_filter);
}

ClassifiedPathSet ClassifiedPathSet::Classify(const simdata::SimResultSet &srs,
                                              const std::set<uint32_t> &observerIds,
                                              const std::set<uint32_t> &flowIds,
                                              std::map<uint32_t, std::set<uint32_t>> flowSelectionMap,
                                              const EfmBitSet &bitCombis, double lossRateTh,
                                              uint32_t delayTh, uint32_t flowLengthTh,
                                              ClassificationMode classificationMode,
                                              std::string classification_base_id,
                                              double smallFailFactor,
                                              double largeFailFactor,
                                              double time_filter)
{
  if (observerIds.empty() || flowIds.empty() || bitCombis.empty())
    throw std::invalid_argument("Empty input arg.");

  ClassifiedPathSet cps(lossRateTh, delayTh, flowLengthTh, classificationMode, classification_base_id);
  cps.m_config.flowIds = flowIds;
  cps.m_config.observerSet.observers = observerIds;

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

          ClassifiedLinkPath clp;
          clp.path = cps.GenerateUnidirBitPaths(observerId, bit, lp, reverseLp);

          // Important to generate at least an empty vector, even if no paths are generated
          // so we can later differentiate between invalid observer/bit combinations and
          // combinations that just yielded no paths
          ClassPathVec &cpv = cps.m_classifiedPaths[observerId][bit];

          if (srs.GetFlowStats(observerId, fid).totalEfmPackets == 0)
            continue;  // Skip flows with no EFM packets

          if (classificationMode == ClassificationMode::STATIC)
          {
            FailureStruct failed =
                cps.ClassifyFlow(obptr, fid, bit, smallFailFactor, largeFailFactor, time_filter);
            // Only add paths that are long enough or failed and ignore faulty measurements
            // (negative loss/delay cant exist)
            if ((failed.small_failure ||
                 srs.GetFlowStats(observerId, fid).totalPackets >= flowLengthTh) &&
                failed.measurement >= 0.0)
            {
              clp.failed = failed.failed_and_medium_failure;
              clp.measurement = failed.measurement;
              cpv.push_back(clp);
            }
          }
          else if (classificationMode == ClassificationMode::PERFECT)
          {
            bool isLossBit = IsLossBit(bit);
            clp.failed = cps.ClassifyLinkPathViaGT(srs, clp.path, isLossBit, !isLossBit);
            clp.measurement = 0.0;
            cpv.push_back(clp);
          }

          // It is hard to split bit path generation and classification for bidirectional flows
          // So let this method handle all of it (if the observer is bidirectional)
          if (bidirectional)
            cps.HandleBidirBitPathClassification(srs, obptr, bit, fid, reverseFid, lp, reverseLp, smallFailFactor, largeFailFactor, time_filter);
        }
      }
    }
  }

  // Handle active measurement bits
  cps.HandleActiveMeasurements(srs, observerIds, bitCombis, lossRateTh, delayTh,
                               classificationMode,classification_base_id, smallFailFactor, largeFailFactor);

  return cps;
}

void ClassifiedPathSet::HandleActiveMeasurements(const simdata::SimResultSet &srs,
                                                 const std::set<uint32_t> &observerIds,
                                                 const EfmBitSet &bitCombis, double lossRateTh,
                                                 uint32_t delayTh,
                                                 ClassificationMode classificationMode,
                                                 std::string classification_base_id,
                                                 double smallFailFactor,
                                                 double largeFailFactor)
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

      LinkPath rtPath = _lp.value().Append(_lp2.value());
      for (auto &bit : filteredBits)
      {
        ClassifiedLinkPath clp;
        clp.path = rtPath;

        if (bit == EfmBit::PINGLSS)
        {
          if (classificationMode == ClassificationMode::STATIC){
            clp.failed = clp.medium_failure = pp->GetRelativeLoss() >= lossRateTh;
            clp.small_failure = pp->GetRelativeLoss() >= smallFailFactor * lossRateTh;
            clp.large_failure = pp->GetRelativeLoss() >= largeFailFactor * lossRateTh;
            clp.measurement = pp->GetRelativeLoss();
          }
          else if (classificationMode == ClassificationMode::PERFECT){
            clp.failed = clp.medium_failure = ClassifyLinkPathViaGT(srs, clp.path, true, false);
            clp.small_failure = false;
            clp.large_failure = false;
            clp.measurement = 0.0;
          }
        }
        else if (bit == EfmBit::PINGDLY)
        {
          if (classificationMode == ClassificationMode::STATIC){
            clp.failed = clp.medium_failure = pp->GetAvgDelay() >= delayTh;
            clp.small_failure = pp->GetAvgDelay() >= smallFailFactor * delayTh;
            clp.large_failure = pp->GetAvgDelay() >= largeFailFactor * delayTh;
            clp.measurement = pp->GetAvgDelay().value_or(0);
          }
          else if (classificationMode == ClassificationMode::PERFECT){
            clp.failed = clp.medium_failure = ClassifyLinkPathViaGT(srs, clp.path, false, true);
            clp.small_failure = false;
            clp.large_failure = false;
            clp.measurement = 0.0;
          }
        }
        else
        {
          throw std::runtime_error("Unknown active measurement bit.");
        }
        m_classifiedPaths[oid][bit].push_back(clp);
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
        ClassifiedLinkPath clp;
        clp.path = etePath;

        if (bit == EfmBit::PINGLSS)
        {
          if (classificationMode == ClassificationMode::STATIC){
            clp.failed = clp.medium_failure = pp->GetRelativeLoss() >= lossRateTh;
            clp.small_failure = pp->GetRelativeLoss() >= smallFailFactor * lossRateTh;
            clp.large_failure = pp->GetRelativeLoss() >= largeFailFactor * lossRateTh;
            clp.measurement = pp->GetRelativeLoss();
          }
          else if (classificationMode == ClassificationMode::PERFECT){
            clp.failed = clp.medium_failure = ClassifyLinkPathViaGT(srs, clp.path, true, false);
            clp.small_failure = false;
            clp.large_failure = false;
            clp.measurement = 0.0;
          }
        }
        else if (bit == EfmBit::PINGDLY)
        {
          if (classificationMode == ClassificationMode::STATIC){
            clp.failed = clp.medium_failure = pp->GetAvgDelay() >= delayTh;
            clp.small_failure = pp->GetAvgDelay() >= smallFailFactor * delayTh;
            clp.large_failure = pp->GetAvgDelay() >= largeFailFactor * delayTh;
            clp.measurement = pp->GetAvgDelay().value_or(0);
          }
          else if (classificationMode == ClassificationMode::PERFECT){
            clp.failed = clp.medium_failure = ClassifyLinkPathViaGT(srs, clp.path, false, true);
            clp.small_failure = false;
            clp.large_failure = false;
            clp.measurement = 0.0;
          }
        }
        else
        {
          throw std::runtime_error("Unknown active measurement bit.");
        }
        m_classifiedPaths[oid][bit].push_back(clp);
      }
    }
  }
}

std::string efmbit_to_string(EfmBit bit){
    switch (bit)
    {
        case EfmBit::SEQ:
            return "SEQ";
        case EfmBit::Q:
            return "Q";
        case EfmBit::L:
            return "L";
        case EfmBit::T:
            return "T";
        case EfmBit::R:
            return "R";
        case EfmBit::QR:
            return "QR";
        case EfmBit::QT:
            return "QT";
        case EfmBit::QL:
            return "QL";
        case EfmBit::LT:
            return "LT";
        case EfmBit::TCPRO:
            return "TCPRO";
        case EfmBit::TCPDART:
            return "TCPDART";
        case EfmBit::SPIN:
            return "SPIN";
        case EfmBit::PINGLSS:
            return "PINGLSS";
        case EfmBit::PINGDLY:
            return "PINGDLY";
        default:
        throw std::runtime_error("Should never happen.");
    }
}

std::string efmbitset_to_string(EfmBitSet bitset)
{
  std::string str = "";
  for (auto &bit : bitset)
  {
    str += efmbit_to_string(bit) + " ";
  }
  return str;
}

std::string LinkPathToString(LinkPath linkPath){
    std::stringstream ss; 
    ss << "[";
    for (auto link : linkPath.links){
        ss << "(" << std::to_string(link.first) << "," << std::to_string(link.second) << ")";
    }
    ss << "]";
    return ss.str();
}

std::string LinkToString(Link link){
    std::stringstream ss; 
    ss << "(" << std::to_string(link.first) << "," << std::to_string(link.second) << ")";
    return ss.str();  
}

std::string ObsvVantagePointPointerVectorToString(std::vector<simdata::ObsvVantagePointPointer> flowPath){
    std::stringstream ss; 
    ss << "[";
    for (auto &observer : flowPath){
        ss << "(" << std::to_string(observer->m_nodeId) << ")";
    }
    ss << "]";
    return ss.str();
}

std::optional<ClassPathVec> ClassifiedPathSet::GetClassifiedPaths(uint32_t observerId,
                                                                  EfmBit bits) const
{
  auto obsvIt = m_classifiedPaths.find(observerId);
  if (obsvIt == m_classifiedPaths.end())
    return std::nullopt;

  auto efmBitIt = obsvIt->second.find(bits);
  if (efmBitIt == obsvIt->second.end())
    return std::nullopt;

  return efmBitIt->second;
}

ClassPathVec ClassifiedPathSet::GetClassifiedPaths(const std::set<uint32_t> &observerIds,
                                                   const EfmBitSet &bitCombis) const
{
  ClassPathVec cpv;
  for (auto &oid : observerIds)
  {
    for (auto &bit : bitCombis)
    {
      std::optional<ClassPathVec> c = GetClassifiedPaths(oid, bit);
      if (!c.has_value())
        continue;
      cpv.insert(cpv.end(), c.value().begin(), c.value().end());
    }
  }
  return cpv;
}

LinkPath ClassifiedPathSet::GenerateUnidirBitPaths(uint32_t observerId, EfmBit bits,
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

void ClassifiedPathSet::HandleBidirBitPathClassification(
    const simdata::SimResultSet &srs, const simdata::ObsvVantagePointPointer &observer, EfmBit bits,
    uint32_t flowId, uint32_t reverseFlowId, LinkPath &flowPath, LinkPath &reverseFlowPath, double smallFailFactor, double largeFailFactor, double time_filter)
{
  uint32_t observerId = observer->m_nodeId;
  simdata::SimObsvFlowPointer flow = observer->GetFlow(flowId);
  simdata::SimObsvFlowPointer reverseFlow = observer->GetFlow(reverseFlowId);

  // Important to generate at least an empty vector, even if no paths are generated
  // so we can later differentiate between invalid observer/bit combinations and
  // combinations that just yielded no paths
  ClassPathVec &cpv = m_classifiedPaths[observerId][bits];

  switch (bits)
  {
    case EfmBit::T:
    {
      // Compute half-RT loss (O-C-O or O-S-O)
      ClassifiedLinkPath clp;
      // The half measurement for a C-S flow contains the O-C-O loss and vice versa
      clp.path =
          reverseFlowPath.GetPathFromXToEnd(observerId).Append(flowPath.GetUpToX(observerId));

      if (m_config.classificationMode == ClassificationMode::STATIC)
      {
        double loss = flow->GetRelativeTBitHalfLoss();
        FailureStruct failed = FailureStruct{loss >= m_config.lossRateTh,
                                             loss >= m_config.lossRateTh * smallFailFactor,
                                             loss >= m_config.lossRateTh * largeFailFactor, loss};
        // Only add paths that are long enough or failed and ignore faulty measurements (negative
        // loss/delay cant exist)
        if ((failed.small_failure ||
             srs.GetFlowStats(observerId, flowId).totalPackets >= m_config.flowLengthTh) &&
            failed.measurement >= 0.0)
        {
          clp.failed = clp.medium_failure = failed.failed_and_medium_failure;
          clp.small_failure = failed.small_failure;
          clp.large_failure = failed.large_failure;
          clp.measurement = failed.measurement;
          cpv.push_back(clp);
        }
      }
      else if (m_config.classificationMode == ClassificationMode::PERFECT)
      {
        clp.failed = clp.medium_failure = ClassifyLinkPathViaGT(srs, clp.path, true, false);
        clp.small_failure = false;
        clp.large_failure = false;
        clp.measurement = 0.0;
        cpv.push_back(clp);
      }
      break;
    }
    case EfmBit::SPIN:
    {
      // Compute end-to-end delay
      ClassifiedLinkPath clp;
      clp.path =
          reverseFlowPath.GetPathFromXToEnd(observerId).Append(flowPath.GetUpToX(observerId));

      if (m_config.classificationMode == ClassificationMode::STATIC)
      {
        // TODO: Rethink threshold choice
        double delay = flow->GetAvgSpinEtEDelay(time_filter).value_or(0);
        FailureStruct failed =
            FailureStruct{delay >= m_config.delayTh, delay >= m_config.delayTh * smallFailFactor,
                          delay >= m_config.delayTh * largeFailFactor, delay};
        // Only add paths that are long enough or failed and ignore faulty measurements (negative
        // loss/delay cant exist)
        if ((failed.small_failure ||
             srs.GetFlowStats(observerId, flowId).totalPackets >= m_config.flowLengthTh) &&
            failed.measurement >= 0.0)
        {
          clp.failed = clp.medium_failure = failed.failed_and_medium_failure;
          clp.small_failure = failed.small_failure;
          clp.large_failure = failed.large_failure;
          clp.measurement = failed.measurement;
          cpv.push_back(clp);
        }
      }
      else if (m_config.classificationMode == ClassificationMode::PERFECT)
      {
        clp.failed = clp.medium_failure = ClassifyLinkPathViaGT(srs, clp.path, false, true);
        clp.small_failure = false;
        clp.large_failure = false;
        clp.measurement = 0.0;
        cpv.push_back(clp);
      }

      break;
    }
    case EfmBit::QR:
    {
      // Compute downstream loss
      ClassifiedLinkPath clp;
      clp.path = flowPath.GetPathFromXToEnd(observerId);
      double uloss = flow->GetRelativeQBitLoss();
      double ulossRev = reverseFlow->GetRelativeQBitLoss();
      double tqlossRev = reverseFlow->GetRelativeRBitLoss();
      double dsl = (((tqlossRev - ulossRev) / (1 - ulossRev)) - uloss) / (1 - uloss);

      if (m_config.classificationMode == ClassificationMode::STATIC)
      {
        FailureStruct failed =
            FailureStruct{dsl >= m_config.lossRateTh, dsl >= m_config.lossRateTh * smallFailFactor,
                          dsl >= m_config.lossRateTh * largeFailFactor, dsl};
        // Only add paths that are long enough or failed and ignore faulty measurements (negative
        // loss/delay cant exist)
        if ((failed.small_failure ||
             srs.GetFlowStats(observerId, flowId).totalPackets >= m_config.flowLengthTh) &&
            failed.measurement >= 0.0)
        {
          clp.failed = clp.medium_failure = failed.failed_and_medium_failure;
          clp.small_failure = failed.small_failure;
          clp.large_failure = failed.large_failure;
          clp.measurement = failed.measurement;
          cpv.push_back(clp);
        }
      }
      else if (m_config.classificationMode == ClassificationMode::PERFECT)
      {
        clp.failed = clp.medium_failure = ClassifyLinkPathViaGT(srs, clp.path, true, false);
        clp.small_failure = false;
        clp.large_failure = false;
        clp.measurement = 0.0;
        cpv.push_back(clp);
      }


      // Compute half RT loss
      ClassifiedLinkPath clp2;
      // We only compute the O-X-O loss here for our X-Y flow, the O-Y-O loss is handled by the
      // computation for the reverse flow
      clp2.path =
          reverseFlowPath.GetPathFromXToEnd(observerId).Append(flowPath.GetUpToX(observerId));

      if (m_config.classificationMode == ClassificationMode::STATIC)
      {
        double loss = ((flow->GetRelativeRBitLoss() - ulossRev) / (1 - ulossRev));
        FailureStruct failed = FailureStruct{loss >= m_config.lossRateTh,
                                             loss >= m_config.lossRateTh * smallFailFactor,
                                             loss >= m_config.lossRateTh * largeFailFactor, loss};

        // Only add paths that are long enough or failed and ignore faulty measurements (negative
        // loss/delay cant exist)
        if ((failed.small_failure ||
             srs.GetFlowStats(observerId, flowId).totalPackets >= m_config.flowLengthTh) &&
            failed.measurement >= 0.0)
        {
          clp2.failed = clp2.medium_failure = failed.failed_and_medium_failure;
          clp2.small_failure = failed.small_failure;
          clp2.large_failure = failed.large_failure;
          clp2.measurement = failed.measurement;
          cpv.push_back(clp2);
        }
      }
      else if (m_config.classificationMode == ClassificationMode::PERFECT)
      {
        clp2.failed = clp2.medium_failure = ClassifyLinkPathViaGT(srs, clp2.path, true, false);
        clp2.small_failure = false;
        clp2.large_failure = false;
        clp2.measurement = 0.0;
        cpv.push_back(clp2);
      }

      break;
    }
    case EfmBit::QT:
    {
      ClassifiedLinkPath clp;
      clp.path = flowPath.GetPathFromXToEnd(observerId).Append(reverseFlowPath);

      if (m_config.classificationMode == ClassificationMode::STATIC)
      {
        // Compute downstream loss using Tbit-half-loss
        double loss = ((reverseFlow->GetRelativeTBitHalfLoss() - reverseFlow->GetRelativeQBitLoss()) /
             (1 - reverseFlow->GetRelativeQBitLoss()));
        FailureStruct failed = FailureStruct{loss >= m_config.lossRateTh,
                                             loss >= m_config.lossRateTh * smallFailFactor,
                                             loss >= m_config.lossRateTh * largeFailFactor, loss};
        // Only add paths that are long enough or failed and ignore faulty measurements (negative
        // loss/delay cant exist)
        if ((failed.small_failure ||
             srs.GetFlowStats(observerId, flowId).totalPackets >= m_config.flowLengthTh) &&
            failed.measurement >= 0.0)
        {
          clp.failed = clp.medium_failure = failed.failed_and_medium_failure;
          clp.small_failure = failed.small_failure;
          clp.large_failure = failed.large_failure;
          clp.measurement = failed.measurement;
          cpv.push_back(clp);
        }
      }
      else if (m_config.classificationMode == ClassificationMode::PERFECT)
      {
        clp.failed = clp.medium_failure = ClassifyLinkPathViaGT(srs, clp.path, true, false);
        clp.small_failure = false;
        clp.large_failure = false;
        clp.measurement = 0.0;
        cpv.push_back(clp);
      }

      break;
    }
    default:
      break;
  }
}

FailureStruct ClassifiedPathSet::ClassifyFlow(const simdata::ObsvVantagePointPointer &observer,
                                     uint32_t flowId, EfmBit bits, double smallFailFactor, double largeFailFactor, double time_filter)
{
  simdata::SimObsvFlowPointer flow = observer->GetFlow(flowId);
  switch (bits)
  {
    case EfmBit::SEQ:
    {
      double loss = flow->GetRelativeSeqLoss();
      return FailureStruct{loss >= m_config.lossRateTh,
                           loss >= m_config.lossRateTh * smallFailFactor,
                           loss >= m_config.lossRateTh * largeFailFactor, loss};
      break;
    }
    case EfmBit::Q:
    {
      double loss = flow->GetRelativeQBitLoss();
      return FailureStruct{loss >= m_config.lossRateTh,
                           loss >= m_config.lossRateTh * smallFailFactor,
                           loss >= m_config.lossRateTh * largeFailFactor, loss};
      break;
    }
    case EfmBit::L:
    {
      double loss = flow->GetRelativeLBitLoss();
      return FailureStruct{loss >= m_config.lossRateTh,
                           loss >= m_config.lossRateTh * smallFailFactor,
                           loss >= m_config.lossRateTh * largeFailFactor, loss};
      break;
    }
    case EfmBit::T:
    {
      double loss = flow->GetRelativeTBitFullLoss();
      return FailureStruct{loss >= m_config.lossRateTh,
                           loss >= m_config.lossRateTh * smallFailFactor,
                           loss >= m_config.lossRateTh * largeFailFactor, loss};
      break;
    }
    case EfmBit::R:
    {
      double loss = flow->GetRelativeRBitLoss();
      return FailureStruct{loss >= m_config.lossRateTh,
                           loss >= m_config.lossRateTh * smallFailFactor,
                           loss >= m_config.lossRateTh * largeFailFactor, loss};
      break;
    }
    case EfmBit::SPIN:
    {
      double delay = flow->GetAvgSpinRTDelay(time_filter).value_or(0);
      return FailureStruct{delay >= m_config.delayTh, delay >= m_config.delayTh * smallFailFactor,
                           delay >= m_config.delayTh * largeFailFactor, delay};
      break;
    }
    case EfmBit::QL:
    {
      double uloss = flow->GetRelativeQBitLoss();
      // TODO: Consider loss rate adaption (e.g., increase end-to-end loss if less than upstream
      // loss; depends on type of flow (ACK dominated, ...))
      double loss = (flow->GetRelativeLBitLoss() - uloss) / (1 - uloss); // Based on EFM draft
      return FailureStruct{loss >= m_config.lossRateTh,
                           loss >= m_config.lossRateTh * smallFailFactor,
                           loss >= m_config.lossRateTh * largeFailFactor, loss};
      break;
    }
    case EfmBit::QR:
    {
      double uloss = flow->GetRelativeQBitLoss();
      double loss = (flow->GetRelativeRBitLoss() - uloss) / (1 - uloss); // Based on EFM draft
      return FailureStruct{loss >= m_config.lossRateTh,
                           loss >= m_config.lossRateTh * smallFailFactor,
                           loss >= m_config.lossRateTh * largeFailFactor, loss};
      break;
    }
    case EfmBit::QT:
    {
      double uloss = flow->GetRelativeQBitLoss();
      double loss = (flow->GetRelativeTBitFullLoss() - uloss) / (1 - uloss); // Based on EFM draft
      return FailureStruct{loss >= m_config.lossRateTh,
                           loss >= m_config.lossRateTh * smallFailFactor,
                           loss >= m_config.lossRateTh * largeFailFactor, loss};
      break;
    }
    case EfmBit::LT:
    {
      double eloss = flow->GetRelativeLBitLoss();
      double loss = (flow->GetRelativeTBitFullLoss() - eloss) / (1 - eloss); // Based on EFM draft
      return FailureStruct{loss >= m_config.lossRateTh,
                           loss >= m_config.lossRateTh * smallFailFactor,
                           loss >= m_config.lossRateTh * largeFailFactor, loss};
      break;
    }
    case EfmBit::TCPRO:
    {
      double loss = flow->GetRelativeTcpReordering();
      return FailureStruct{loss >= m_config.lossRateTh,
                           loss >= m_config.lossRateTh * smallFailFactor,
                           loss >= m_config.lossRateTh * largeFailFactor, loss};
      break;
    }
    case EfmBit::TCPDART:
    {
      double delay = flow->GetAvgTcpHRTDelay().value_or(0);
      return FailureStruct{delay >= m_config.delayTh, delay >= m_config.delayTh * smallFailFactor,
                           delay >= m_config.delayTh * largeFailFactor, delay};
      break;
    }
    default:
      throw std::runtime_error("Should never happen.");
      break;
  }
  return FailureStruct{false, false, false, 0.0};
}

bool ClassifiedPathSet::ClassifyLinkPathViaGT(const simdata::SimResultSet &srs,
                                              const LinkPath &path, bool useLoss,
                                              bool useDelay) const
{
  for (auto &link : path.links)
  {
    std::optional<simdata::FailedLink> fl = srs.GetFailedLink(link.first, link.second);
    if (!fl.has_value())
      continue;

    if (useLoss && fl->lossRate >= m_config.lossRateTh)
      return true;
    if (useDelay && fl->delayMs >= m_config.delayTh)
      return true;
  }
  return false;
}

//----- Helper functions -----

std::optional<LinkPath> GenerateLinkPath(std::vector<simdata::ObsvVantagePointPointer> flowPath)
{
  if (flowPath.size() < 2)
  {
    std::cout << "Warning: Flow path too short." << std::endl;
    return std::nullopt;
  }
  LinkPath p;
  for (size_t i = 1; i < flowPath.size(); i++)
  {
    p.links.push_back(std::pair(flowPath[i - 1]->m_nodeId, flowPath[i]->m_nodeId));
  }
  return p;
}

std::optional<LinkPath> GenerateLinkPath(std::vector<uint32_t> flowPath)
{
  if (flowPath.size() < 2)
  {
    std::cout << "Warning: Flow path too short." << std::endl;
    return std::nullopt;
  }

  LinkPath p;
  for (size_t i = 1; i < flowPath.size(); i++)
  {
    p.links.push_back(std::pair(flowPath[i - 1], flowPath[i]));
  }
  return p;
}

bool IsLossBit(EfmBit bit)
{
  switch (bit)
  {
    case EfmBit::SEQ:
    case EfmBit::Q:
    case EfmBit::L:
    case EfmBit::T:
    case EfmBit::R:
    case EfmBit::QR:
    case EfmBit::QT:
    case EfmBit::QL:
    case EfmBit::LT:
    case EfmBit::TCPRO:
    case EfmBit::PINGLSS:
      return true;
    case EfmBit::SPIN:
    case EfmBit::TCPDART:
    case EfmBit::PINGDLY:
      return false;
    default:
      throw std::runtime_error("Should never happen.");
  }
}

bool AreLossBits(EfmBitSet bits)
{
    bool loss_bit = true;
    int count = 0;
    for (auto bit : bits) 
    {
        switch (bit)
        {
            case EfmBit::SEQ:
            case EfmBit::Q:
            case EfmBit::L:
            case EfmBit::T:
            case EfmBit::R:
            case EfmBit::QR:
            case EfmBit::QT:
            case EfmBit::QL:
            case EfmBit::LT:
            case EfmBit::TCPRO:
            case EfmBit::PINGLSS:
            {
                // We have seen a non-loss bit before
                if (!loss_bit) {
                    //std::cout << "Here : " << std::to_string(count) << std::endl;
                    throw std::runtime_error("We don’t allow mixing the bits (Got loss bit, previously saw delay bit).");
                }
                //std::cout << "There: " << std::to_string(count) << std::endl;
                count ++;
                break;
            }
            case EfmBit::SPIN:
            case EfmBit::TCPDART:
            case EfmBit::PINGDLY:
            {
                // We have seen a non-loss bit before
                if (loss_bit && count > 0) {
                    throw std::runtime_error("We don’t allow mixing the bits (Got delay bit, previously saw loss bit).");
                }
                //std::cout << "But here: " << std::to_string(count) << std::endl;
                loss_bit = false;
                count ++;
                break;
            }
            default:
            throw std::runtime_error("Should never happen.");
        }
    }
    return loss_bit;
}

bool AreSingleCombinationBit(EfmBitSet bits)
{
    if (bits.size() > 1 || bits.size() == 0){
        return false;
    }
    for (auto bit : bits) 
    {
        switch (bit)
        {
            case EfmBit::SEQ:
            case EfmBit::L:
            case EfmBit::T:
            case EfmBit::R:
            case EfmBit::QR:
            case EfmBit::QT:
            case EfmBit::QL:
            case EfmBit::LT:
            case EfmBit::TCPRO:
            case EfmBit::PINGDLY:
            case EfmBit::PINGLSS:
                return false;
            case EfmBit::Q:
            case EfmBit::SPIN:
            case EfmBit::TCPDART:
                return true;
            default:
            throw std::runtime_error("Should never happen.");
        }
    }
    return false;
}



bool IsCombinationBit(EfmBit bit)
{
    switch (bit)
    {
        case EfmBit::SEQ:
        case EfmBit::L:
        case EfmBit::T:
        case EfmBit::R:
        case EfmBit::QR:
        case EfmBit::QT:
        case EfmBit::QL:
        case EfmBit::LT:
        case EfmBit::TCPRO:
        case EfmBit::PINGDLY:
        case EfmBit::PINGLSS:
            return false;
        case EfmBit::Q:
        case EfmBit::SPIN:
        case EfmBit::TCPDART:
            return true;
        default:
        throw std::runtime_error("Should never happen.");
    }
    return false;
}


bool IsActiveMmntBit(EfmBit bit)
{
  switch (bit)
  {
    case EfmBit::SEQ:
    case EfmBit::Q:
    case EfmBit::L:
    case EfmBit::T:
    case EfmBit::R:
    case EfmBit::QR:
    case EfmBit::QT:
    case EfmBit::QL:
    case EfmBit::LT:
    case EfmBit::TCPRO:
    case EfmBit::TCPDART:
    case EfmBit::SPIN:
      return false;
    case EfmBit::PINGLSS:
    case EfmBit::PINGDLY:
      return true;
    default:
      throw std::runtime_error("Should never happen.");
  }
}

//----- LinkPath -----

LinkPath LinkPath::GetUpToX(uint32_t nodeId) const
{
  LinkPath newPath;
  // Return empty path if its begins with the node
  if (links.at(0).first == nodeId)
    return newPath;
  for (auto &link : links)
  {
    newPath.links.push_back(link);
    if (link.second == nodeId)
      break;
  }
  return newPath;
}

LinkPath LinkPath::GetPathFromXToEnd(uint32_t nodeId) const
{
  LinkPath newPath;
  bool start = false;
  for (auto &link : links)
  {
    if (link.first == nodeId)
      start = true;
    if (start)
      newPath.links.push_back(link);
  }
  return newPath;
}

bool LinkPath::ContainsNode(uint32_t nodeId) const
{
  for (auto &link : links)
  {
    if (link.first == nodeId || link.second == nodeId)
      return true;
  }
  return false;
}

bool LinkPath::ContainsLink(Link check_link) const
{
  for (auto &link : links)
  {
    if (link.first == check_link.first && link.second == check_link.second)
      return true;
  }
  return false;
}

LinkPath LinkPath::Append(const LinkPath &otherPath) const
{
  LinkPath newPath;
  newPath.links = links;
  newPath.links.insert(newPath.links.end(), otherPath.links.begin(), otherPath.links.end());
  return newPath;
}

LinkPath LinkPath::AppendTo(const LinkPath &otherPath) const
{
  LinkPath newPath;
  newPath.links = otherPath.links;
  newPath.links.insert(newPath.links.end(), links.begin(), links.end());
  return newPath;
}

}  // namespace analysis
