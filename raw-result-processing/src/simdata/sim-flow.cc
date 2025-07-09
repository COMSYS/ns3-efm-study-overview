#include "sim-flow.h"

#include <iostream>
#include <stdexcept>

namespace simdata {

#define EFM_Q_BLOCK_SIZE 64

std::string LossMmntTypeToString(const LossMmntType &lossMmntType)
{
  switch (lossMmntType)
  {
    case LossMmntType::EFM_Q:
      return "EFM_Q";
    case LossMmntType::EFM_R:
      return "EFM_R";
    case LossMmntType::EFM_L:
      return "EFM_L";
    case LossMmntType::EFM_T_FULL:
      return "EFM_T_FULL";
    case LossMmntType::EFM_T_HALF:
      return "EFM_T_HALF";
    case LossMmntType::GT_ALL:
      return "GT_ALL";
    case LossMmntType::GT_ACK:
      return "GT_ACK";
    default:
      throw std::runtime_error("Unexpected loss measurement type!");
  }
}

void SimFlow::AddEvent(EventPointer simEvent) { m_simEvents[simEvent->eventType].insert(simEvent); }

uint32_t SimFlow::GetEventCount()
{
  uint32_t count = 0;
  for (auto it = m_simEvents.begin(); it != m_simEvents.end(); it++)
  {
    count += it->second.size();
  }
  return count;
}

uint32_t SimFlow::GetFlowId() { return m_flowId; }

// ---------------------------------------------------
// ----------------- SimObserverFlow -----------------
// ---------------------------------------------------

SimObsvFlowPointer SimObserverFlow::ApplyFilter(const SimFilter &filter)
{
  // Clone flow
  SimObsvFlowPointer ofp = std::make_shared<SimObserverFlow>(*this);

  // Filter flow events
  FilterSimEventSet(ofp->m_simEvents, filter, true, false);

  return ofp;
}

std::optional<std::list<double>> SimObserverFlow::GetRawSpinRTValues(double time_filter) const
{
    auto it = m_simEvents.find(SimEventType::OBSV_SPIN_BIT_DELAY);
    if (it == m_simEvents.end() || it->second.size() == 0)
        return std::nullopt;

    std::list<double> result;
    for (const auto &ev : it->second)
    {
        if (ev-> time < time_filter) {
            result.push_back(std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms);
        }
    }
    return result;
}

std::optional<double> SimObserverFlow::GetAvgSpinRTDelay(double time_filter) const
{
  auto it = m_simEvents.find(SimEventType::OBSV_SPIN_BIT_DELAY);
  if (it == m_simEvents.end() || it->second.size() == 0)
    return std::nullopt;

  double result = 0.0;
  for (const auto &ev : it->second)
  {
    if (ev-> time < time_filter) {
        result += std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms;
    }
  }
  return result / it->second.size();
}

std::optional<uint32_t> SimObserverFlow::GetMinSpinRTDelay(double time_filter) const
{
  auto it = m_simEvents.find(SimEventType::OBSV_SPIN_BIT_DELAY);
  if (it == m_simEvents.end() || it->second.size() == 0)
    return std::nullopt;

  uint32_t result = UINT32_MAX;
  for (const auto &ev : it->second)
  {
    if (ev-> time < time_filter) {
        uint32_t tmp = std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms;
        if (tmp < result)
        result = tmp;
    }
  }

  return result;
}

std::optional<uint32_t> SimObserverFlow::GetMaxSpinRTDelay(double time_filter) const
{
  auto it = m_simEvents.find(SimEventType::OBSV_SPIN_BIT_DELAY);
  if (it == m_simEvents.end() || it->second.size() == 0)
    return std::nullopt;

  uint32_t result = 0;
  for (const auto &ev : it->second)
  {
    if (ev-> time < time_filter) {
        uint32_t tmp = std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms;
        if (tmp > result)
        result = tmp;
    }
  }

  return result;
}

std::optional<double> SimObserverFlow::GetAvgSpinEtEDelay(double time_filter) const
{
  auto it = m_simEvents.find(SimEventType::OBSV_SPIN_BIT_DELAY);
  if (it == m_simEvents.end() || it->second.size() == 0)
    return std::nullopt;

  double result = 0.0;
  uint32_t resCount = 0;
  for (const auto &ev : it->second)
  {
    if (ev-> time < time_filter) {
        auto halfOpt = std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->half_delay_ms;
        if (halfOpt.has_value())
        {
        result += halfOpt.value();
        resCount++;
        }
    }
  }

  if (resCount == 0)
    return std::nullopt;
  return result / resCount;
}

std::optional<uint32_t> SimObserverFlow::GetMinSpinEtEDelay(double time_filter) const
{
  auto it = m_simEvents.find(SimEventType::OBSV_SPIN_BIT_DELAY);
  if (it == m_simEvents.end() || it->second.size() == 0)
    return std::nullopt;
  uint32_t result = UINT32_MAX;
  for (const auto &ev : it->second)
  {
    if (ev-> time < time_filter) {
        auto halfOpt = std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->half_delay_ms;
        if (halfOpt.has_value())
        {
        if (halfOpt.value() < result)
            result = halfOpt.value();
        }
    }
  }

  if (result == UINT32_MAX)
    return std::nullopt;
  return result;
}

std::optional<uint32_t> SimObserverFlow::GetMaxSpinEtEDelay(double time_filter) const
{
  auto it = m_simEvents.find(SimEventType::OBSV_SPIN_BIT_DELAY);
  uint32_t result = 0;
  bool found = false;
  for (const auto &ev : it->second)
  {
    if (ev-> time < time_filter) {
        auto halfOpt = std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->half_delay_ms;
        if (halfOpt.has_value())
        {
        if (halfOpt.value() > result)
            result = halfOpt.value();
        found = true;
        }
    }
  }

  if (!found)
    return std::nullopt;
  return result;
}

std::optional<double> SimObserverFlow::GetAvgTcpHRTDelay() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_TCP_DART_DELAY);

  if (it == m_simEvents.end() || it->second.size() == 0)
    return std::nullopt;

  double result = 0.0;
  for (const auto &ev : it->second)
  {
    result += std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms;
  }
  return result / it->second.size();
}

std::optional<uint32_t> SimObserverFlow::GetMinTcpHRTDelay() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_TCP_DART_DELAY);
  if (it == m_simEvents.end() || it->second.size() == 0)
    return std::nullopt;

  uint32_t result = UINT32_MAX;
  for (const auto &ev : it->second)
  {
    uint32_t tmp = std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms;
    if (tmp < result)
      result = tmp;
  }

  return result;
}

std::optional<uint32_t> SimObserverFlow::GetMaxTcpHRTDelay() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_TCP_DART_DELAY);
  if (it == m_simEvents.end() || it->second.size() == 0)
    return std::nullopt;

  uint32_t result = 0;
  for (const auto &ev : it->second)
  {
    uint32_t tmp = std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms;
    if (tmp > result)
      result = tmp;
  }

  return result;
}

std::optional<std::list<double>> SimObserverFlow::GetRawTcpHRTValues() const
{
    auto it = m_simEvents.find(SimEventType::OBSV_TCP_DART_DELAY);

    if (it == m_simEvents.end() || it->second.size() == 0)
        return std::nullopt;

    std::list<double> result;
    for (const auto &ev : it->second)
    {
        result.push_back(std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms);

    }
    return result;
}


uint32_t SimObserverFlow::GetAbsoluteQBitLoss() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_Q_BIT_LOSS);
  if (it == m_simEvents.end())
    return 0;
  uint32_t result = 0;
  for (const auto &ev : it->second)
  {
    result += std::static_pointer_cast<EfmLossMeasurementEvent>(ev)->loss;
  }

  return result;
}

uint32_t SimObserverFlow::GetAbsoluteQBitPacketCount() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_Q_BIT_LOSS);
  if (it == m_simEvents.end())
    return 0.0;
  return (it->second.size() * EFM_Q_BLOCK_SIZE);
}



uint32_t SimObserverFlow::GetAbsoluteRBitLoss() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_R_BIT_LOSS);
  if (it == m_simEvents.end())
    return 0;
  uint32_t result = 0;
  for (const auto &ev : it->second)
  {
    result += std::static_pointer_cast<EfmLossMeasurementEvent>(ev)->loss;
  }

  return result;
}

uint32_t SimObserverFlow::GetAbsoluteLBitLoss() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_L_BIT_SET);
  if (it == m_simEvents.end())
    return 0;

  return it->second.size();
}

uint32_t SimObserverFlow::GetAbsoluteTBitFullLoss() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_T_BIT_FULL_LOSS);
  if (it == m_simEvents.end())
    return 0;
  uint32_t result = 0;
  for (const auto &ev : it->second)
  {
    result += std::static_pointer_cast<EfmLossMeasurementEvent>(ev)->loss;
  }

  return result;
}

uint32_t SimObserverFlow::GetAbsoluteTBitHalfLoss() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_T_BIT_HALF_LOSS);
  if (it == m_simEvents.end())
    return 0;
  uint32_t result = 0;
  for (const auto &ev : it->second)
  {
    result += std::static_pointer_cast<EfmLossMeasurementEvent>(ev)->loss;
  }

  return result;
}

uint32_t SimObserverFlow::GetAbsoluteSeqLoss() const
{
  uint32_t loss = 0;
  uint32_t pktCount = 0;
  GetFinalSeqLoss(loss, pktCount);
  return loss;
}

uint32_t SimObserverFlow::GetAbsoluteAckSeqLoss() const
{
  uint32_t loss = 0;
  uint32_t pktCount = 0;
  GetFinalAckSeqLoss(loss, pktCount);
  return loss;
}

uint32_t SimObserverFlow::GetAbsoluteTcpReordering() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_TCP_REORDERING);
  if (it == m_simEvents.end())
    return 0;
  uint32_t result = 0;
  for (const auto &ev : it->second)
  {
    result += std::static_pointer_cast<EfmLossMeasurementEvent>(ev)->loss;
  }

  return result;
}

double SimObserverFlow::GetRelativeQBitLoss() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_Q_BIT_LOSS);
  if (it == m_simEvents.end())
    return 0.0;
  uint32_t totalLoss = 0;
  for (const auto &ev : it->second)
  {
    totalLoss += std::static_pointer_cast<EfmLossMeasurementEvent>(ev)->loss;
  }

  // TODO: Recheck computation
  // Each Q loss measurement event corresponds to one Q block
  return ((double)totalLoss) / (it->second.size() * EFM_Q_BLOCK_SIZE);
}

double SimObserverFlow::GetRelativeRBitLoss() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_R_BIT_LOSS);
  if (it == m_simEvents.end())
    return 0.0;
  uint32_t totalLoss = 0;
  for (const auto &ev : it->second)
  {
    totalLoss += std::static_pointer_cast<EfmLossMeasurementEvent>(ev)->loss;
  }

  // TODO: Recheck computation
  // Each R loss measurement event corresponds to one R block
  // Which has the same size as a Q block
  return ((double)totalLoss) / (it->second.size() * EFM_Q_BLOCK_SIZE);
}

double SimObserverFlow::GetRelativeLBitLoss() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_L_BIT_SET);
  if (it == m_simEvents.end())
    return 0.0;
  uint32_t totalPackets = 0;
  for (const auto &ev : it->second)
  {
    uint32_t pkt_count = std::static_pointer_cast<EfmBitSetPCountEvent>(ev)->pkt_count;
    totalPackets = totalPackets > pkt_count ? totalPackets : pkt_count;
  }

  if (totalPackets > 0)
    return ((double)it->second.size()) / (totalPackets);
  else
    return 0.0;
}

double SimObserverFlow::GetRelativeTBitFullLoss() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_T_BIT_FULL_LOSS);
  if (it == m_simEvents.end())
    return 0.0;
  uint32_t totalLoss = 0;
  uint32_t totalPackets = 0;
  for (const auto &ev : it->second)
  {
    totalLoss += std::static_pointer_cast<EfmLossMeasurementEvent>(ev)->loss;
    totalPackets += std::static_pointer_cast<EfmLossMeasurementEvent>(ev)->pkt_count;
  }

  if (totalPackets > 0)
    return ((double)totalLoss) / (totalPackets);
  else
    return 0.0;
}

double SimObserverFlow::GetRelativeTBitHalfLoss() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_T_BIT_HALF_LOSS);
  if (it == m_simEvents.end())
    return 0.0;
  uint32_t totalLoss = 0;
  uint32_t totalPackets = 0;
  for (const auto &ev : it->second)
  {
    totalLoss += std::static_pointer_cast<EfmLossMeasurementEvent>(ev)->loss;
    totalPackets += std::static_pointer_cast<EfmLossMeasurementEvent>(ev)->pkt_count;
  }

  if (totalPackets > 0)
    return ((double)totalLoss) / (totalPackets);  // TODO: Reevaluate this computation
  else
    return 0.0;
}

double SimObserverFlow::GetRelativeSeqLoss() const
{
  uint32_t loss = 0;
  uint32_t pktCount = 0;
  GetFinalSeqLoss(loss, pktCount);
  if (loss + pktCount == 0)
    return 0.0;
  return ((double)loss) / (loss + pktCount);
}

double SimObserverFlow::GetRelativeAckSeqLoss() const
{
  uint32_t loss = 0;
  uint32_t pktCount = 0;
  GetFinalAckSeqLoss(loss, pktCount);
  if (loss + pktCount == 0)
    return 0.0;
  return ((double)loss) / (loss + pktCount);
}

double SimObserverFlow::GetRelativeTcpReordering() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_TCP_REORDERING);
  if (it == m_simEvents.end())
    return 0.0;
  uint32_t totalLoss = 0;
  for (const auto &ev : it->second)
  {
    totalLoss += std::static_pointer_cast<EfmLossMeasurementEvent>(ev)->loss;
  }

  // The multiset is ordered by time, so the last element is the final one
  uint32_t pktCount =
      std::static_pointer_cast<EfmLossMeasurementEvent>(*it->second.rbegin())->pkt_count;

  // TODO: Recheck computation
  return ((double)totalLoss) / (pktCount);
}

double SimObserverFlow::GetFlowBegin()
{
  auto it = m_simEvents.find(SimEventType::OBSV_FLOW_BEGIN);
  if (it == m_simEvents.end())
    throw std::runtime_error("Flow begin event missing!");

  if (it->second.size() > 1)
    throw std::runtime_error("Multiple flow begin events!");

  return (*it->second.begin())->time;
}

void SimObserverFlow::GetFinalSeqLoss(uint32_t &loss, uint32_t &pktCount) const
{
  auto it = m_simEvents.find(SimEventType::OBSV_SEQ_LOSS);
  if (it == m_simEvents.end())
  {
    loss = 0;
    pktCount = 0;
    return;
  }

  // The multiset is ordered by time, so the last element is the final one
  loss = std::static_pointer_cast<EfmLossMeasurementEvent>(*it->second.rbegin())->loss;
  pktCount = std::static_pointer_cast<EfmLossMeasurementEvent>(*it->second.rbegin())->pkt_count;
}

void SimObserverFlow::GetFinalAckSeqLoss(uint32_t &loss, uint32_t &pktCount) const
{
  auto it = m_simEvents.find(SimEventType::OBSV_ACK_SEQ_LOSS);
  if (it == m_simEvents.end())
  {
    loss = 0;
    pktCount = 0;
    return;
  }
  // The multiset is ordered by time, so the last element is the final one
  loss = std::static_pointer_cast<EfmLossMeasurementEvent>(*it->second.rbegin())->loss;
  pktCount = std::static_pointer_cast<EfmLossMeasurementEvent>(*it->second.rbegin())->pkt_count;
}

uint32_t SimObserverFlow::GetAbsolutePacketCountLossMmnt(LossMmntType lossMmntType) const
{
  switch (lossMmntType)
  {
    case LossMmntType::EFM_Q:
      return GetAbsoluteQBitPacketCount();
    default:
      throw std::runtime_error("Getting the packet count only supported for Q bit!");
  }
}

uint32_t SimObserverFlow::GetAbsoluteLossMmnt(LossMmntType lossMmntType) const
{
  switch (lossMmntType)
  {
    case LossMmntType::EFM_Q:
      return GetAbsoluteQBitLoss();
    case LossMmntType::EFM_R:
      return GetAbsoluteRBitLoss();
    case LossMmntType::EFM_L:
      return GetAbsoluteLBitLoss();
    case LossMmntType::EFM_T_FULL:
      return GetAbsoluteTBitFullLoss();
    case LossMmntType::EFM_T_HALF:
      return GetAbsoluteTBitHalfLoss();
    case LossMmntType::GT_ALL:
      return GetAbsoluteSeqLoss();
    case LossMmntType::GT_ACK:
      return GetAbsoluteAckSeqLoss();
    default:
      throw std::runtime_error("Unexpected loss moment type!");
  }
}

double SimObserverFlow::GetRelativeLossMmnt(LossMmntType lossMmntType) const
{
  switch (lossMmntType)
  {
    case LossMmntType::EFM_Q:
      return GetRelativeQBitLoss();
    case LossMmntType::EFM_R:
      return GetRelativeRBitLoss();
    case LossMmntType::EFM_L:
      return GetRelativeLBitLoss();
    case LossMmntType::EFM_T_FULL:
      return GetRelativeTBitFullLoss();
    case LossMmntType::EFM_T_HALF:
      return GetRelativeTBitHalfLoss();
    case LossMmntType::GT_ALL:
      return GetRelativeSeqLoss();
    case LossMmntType::GT_ACK:
      return GetRelativeAckSeqLoss();
    default:
      throw std::runtime_error("Unexpected loss moment type!");
  }
}

// ---------------------------------------------------
// ------------------- SimHostFlow -------------------
// ---------------------------------------------------

SimHostFlowPointer SimHostFlow::ApplyFilter(const SimFilter &filter)
{
  // Clone flow
  SimHostFlowPointer hfp = std::make_shared<SimHostFlow>(*this);

  // Filter flow events
  FilterSimEventSet(hfp->m_simEvents, filter, false, true);

  return hfp;
}

uint32_t SimHostFlow::GetTotalLBitsSent() const
{
  auto it = m_simEvents.find(SimEventType::HOST_L_BIT_SET);
  if (it == m_simEvents.end())
    return 0;

  return it->second.size();
}

std::optional<double> SimHostFlow::GetAvgGtTransDelay() const
{
  auto it = m_simEvents.find(SimEventType::HOST_GT_TRANS_DELAY);
  if (it == m_simEvents.end())
    return std::nullopt;

  double result = 0.0;
  for (const auto &ev : it->second)
    result += std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms;

  return result / it->second.size();
}

std::optional<uint32_t> SimHostFlow::GetMinGtTransDelay() const
{
  auto it = m_simEvents.find(SimEventType::HOST_GT_TRANS_DELAY);
  if (it == m_simEvents.end())
    return std::nullopt;
  uint32_t result = UINT32_MAX;
  for (const auto &ev : it->second)
  {
    uint32_t tmp = std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms;
    if (tmp < result)
      result = tmp;
  }

  return result;
}

std::optional<uint32_t> SimHostFlow::GetMaxGtTransDelay() const
{
  auto it = m_simEvents.find(SimEventType::HOST_GT_TRANS_DELAY);
  if (it == m_simEvents.end())
    return std::nullopt;

  uint32_t result = 0;
  for (const auto &ev : it->second)
  {
    uint32_t tmp = std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms;
    if (tmp > result)
      result = tmp;
  }

  return result;
}

std::optional<double> SimHostFlow::GetAvgGtAppDelay() const
{
  auto it = m_simEvents.find(SimEventType::HOST_GT_APP_DELAY);
  if (it == m_simEvents.end())
    return std::nullopt;
  double result = 0.0;
  for (const auto &ev : it->second)
    result += std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms;

  return result / it->second.size();
}

std::optional<uint32_t> SimHostFlow::GetMinGtAppDelay() const
{
  auto it = m_simEvents.find(SimEventType::HOST_GT_APP_DELAY);
  if (it == m_simEvents.end())
    return std::nullopt;
  uint32_t result = UINT32_MAX;
  for (const auto &ev : it->second)
  {
    uint32_t tmp = std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms;
    if (tmp < result)
      result = tmp;
  }

  return result;
}

std::optional<uint32_t> SimHostFlow::GetMaxGtAppDelay() const
{
  auto it = m_simEvents.find(SimEventType::HOST_GT_APP_DELAY);
  if (it == m_simEvents.end())
    return std::nullopt;
  uint32_t result = 0;
  for (const auto &ev : it->second)
  {
    uint32_t tmp = std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms;
    if (tmp > result)
      result = tmp;
  }

  return result;
}

}  // namespace simdata
