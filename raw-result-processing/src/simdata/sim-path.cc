#include "sim-path.h"


namespace simdata {
void SimPath::AddEvent(EventPointer simEvent)
{
  if (!simEvent->IsPathEvent())
    throw std::runtime_error("SimPath::AddEvent: Event is not a path event");
  m_simEvents[simEvent->eventType].insert(simEvent);
}

uint32_t SimPath::GetEventCount() const
{
  uint32_t count = 0;
  for (auto it = m_simEvents.begin(); it != m_simEvents.end(); it++)
  {
    count += it->second.size();
  }
  return count;
}

uint32_t SimPath::GetSQPacketCount() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_P_SQ_BITS_LOSS);
  if (it == m_simEvents.end())
  {
    return 0;
  }

  // The multiset is ordered by time, so the last element is the final one
  return std::static_pointer_cast<EfmSignedLossMeasurementEvent>(*it->second.rbegin())->pkt_count;
}

uint32_t SimPath::GetAbsoluteLBitLoss() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_P_L_BIT_SET);
  if (it == m_simEvents.end())
    return 0;

  return it->second.size();
}

int32_t SimPath::GetAbsoluteFinalSQBitsLoss() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_P_SQ_BITS_LOSS);
  if (it == m_simEvents.end())
  {
    return 0;
  }

  // The multiset is ordered by time, so the last element is the final one
  return std::static_pointer_cast<EfmSignedLossMeasurementEvent>(*it->second.rbegin())->loss;
}

double SimPath::GetAbsoluteAvgSQBitsLoss() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_P_SQ_BITS_LOSS);
  if (it == m_simEvents.end())
  {
    return 0;
  }

  double avg = 0.0;
  for (const auto &ev : it->second)
  {
    avg += std::static_pointer_cast<EfmSignedLossMeasurementEvent>(ev)->loss;
  }
  return avg / it->second.size();
}

double SimPath::GetRelativeLBitLoss() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_P_L_BIT_SET);
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

double SimPath::GetRelativeFinalSQBitsLoss() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_P_SQ_BITS_LOSS);
  if (it == m_simEvents.end())
  {
    return 0;
  }

  // The multiset is ordered by time, so the last element is the final one
  auto event = std::static_pointer_cast<EfmSignedLossMeasurementEvent>(*it->second.rbegin());
  return (double)event->loss / (event->pkt_count + event->loss);  // TODO: Rethink this computation
}

double SimPath::GetRelativeAvgSQBitsLoss() const
{
  auto it = m_simEvents.find(SimEventType::OBSV_P_SQ_BITS_LOSS);
  if (it == m_simEvents.end())
  {
    return 0;
  }

  double avg = 0.0;
  for (const auto &ev : it->second)
  {
    auto event = std::static_pointer_cast<EfmSignedLossMeasurementEvent>(ev);
    avg +=
        (double)event->loss / (event->pkt_count + event->loss);  // TODO: Rethink this computation
  }
  return avg / it->second.size();
}

}  // namespace simdata
