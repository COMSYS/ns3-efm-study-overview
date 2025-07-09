#include "sim-ping-pair.h"


namespace simdata {
void SimPingPair::AddEvent(EventPointer simEvent)
{
  if ((m_ppType == PingPairType::CLIENT && !simEvent->IsPingClientEvent()) ||
      (m_ppType == PingPairType::SERVER && !simEvent->IsPingServerEvent()))
    throw std::runtime_error("SimPingPair::AddEvent: Event does not have the correct type");
  m_simEvents[simEvent->eventType].insert(simEvent);
}

uint32_t SimPingPair::GetEventCount() const
{
  uint32_t count = 0;
  for (auto it = m_simEvents.begin(); it != m_simEvents.end(); it++)
  {
    count += it->second.size();
  }
  return count;
}


uint32_t SimPingPair::GetAbsoluteLoss() const
{
  if (m_ppType == PingPairType::CLIENT)
  {
    auto it = m_simEvents.find(SimEventType::PING_RT_LOSS);
    if (it == m_simEvents.end() || it->second.size() == 0)
      return 0;

    // The multiset is ordered by time, so the last element is the final one
    return std::static_pointer_cast<EfmLossMeasurementEvent>(*it->second.rbegin())->loss;
  }
  else if (m_ppType == PingPairType::SERVER)
  {
    auto it = m_simEvents.find(SimEventType::PING_ETE_LOSS);
    if (it == m_simEvents.end() || it->second.size() == 0)
      return 0;

    // The multiset is ordered by time, so the last element is the final one
    return std::static_pointer_cast<EfmLossMeasurementEvent>(*it->second.rbegin())->loss;
  }
  else
  {
    throw std::runtime_error("SimPingPair::GetAbsoluteLoss: Ping pair type is not set");
  }
}

std::optional<double> SimPingPair::GetAvgDelay() const
{
  if (m_ppType == PingPairType::CLIENT)
  {
    auto it = m_simEvents.find(SimEventType::PING_RT_DELAY);
    if (it == m_simEvents.end() || it->second.size() == 0)
      return std::nullopt;

    double result = 0.0;
    for (const auto &ev : it->second)
    {
      result += std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms;
    }
    return result / it->second.size();
  }
  else if (m_ppType == PingPairType::SERVER)
  {
    auto it = m_simEvents.find(SimEventType::PING_ETE_DELAY);
    if (it == m_simEvents.end() || it->second.size() == 0)
      return std::nullopt;

    double result = 0.0;
    for (const auto &ev : it->second)
    {
      result += std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms;
    }
    return result / it->second.size();
  }
  else
  {
    throw std::runtime_error("SimPingPair::GetDelay: Ping pair type is not set");
  }
}


std::optional<std::list<double>> SimPingPair::GetRawPingDelayValues() const
{
  if (m_ppType == PingPairType::CLIENT)
  {
    auto it = m_simEvents.find(SimEventType::PING_RT_DELAY);
    if (it == m_simEvents.end() || it->second.size() == 0)
      return std::nullopt;

    std::list<double> result;
    for (const auto &ev : it->second)
    {
      result.push_back(std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms);
    }
    return result;
  }
  else if (m_ppType == PingPairType::SERVER)
  {
    auto it = m_simEvents.find(SimEventType::PING_ETE_DELAY);
    if (it == m_simEvents.end() || it->second.size() == 0)
      return std::nullopt;

    std::list<double> result;
    for (const auto &ev : it->second)
    {
      result.push_back(std::static_pointer_cast<EfmDelayMeasurementEvent>(ev)->full_delay_ms);
    }
    return result;
  }
  else
  {
    throw std::runtime_error("SimPingPair::GetDelay: Ping pair type is not set");
  }
}



double SimPingPair::GetRelativeLoss() const
{
  if (m_ppType == PingPairType::CLIENT)
  {
    auto it = m_simEvents.find(SimEventType::PING_RT_LOSS);
    if (it == m_simEvents.end() || it->second.size() == 0)
      return 0.0;

    // The multiset is ordered by time, so the last element is the final one
    auto event = std::static_pointer_cast<EfmLossMeasurementEvent>(*it->second.rbegin());
    return event->loss / (event->pkt_count + event->loss);
  }
  else if (m_ppType == PingPairType::SERVER)
  {
    auto it = m_simEvents.find(SimEventType::PING_ETE_LOSS);
    if (it == m_simEvents.end() || it->second.size() == 0)
      return 0.0;

    // The multiset is ordered by time, so the last element is the final one
    auto event = std::static_pointer_cast<EfmLossMeasurementEvent>(*it->second.rbegin());
    return event->loss / (event->pkt_count + event->loss);
  }
  else
  {
    throw std::runtime_error("SimPingPair::GetRelativeLoss: Ping pair type is not set");
  }
}


}  // namespace simdata
