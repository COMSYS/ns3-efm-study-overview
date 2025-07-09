#include "sim-vantage-point.h"

#include <iostream>
#include <stdexcept>

namespace simdata {

std::string VantagePointTypeToString(VantagePointType &vpt)
{
  switch (vpt)
  {
    case VantagePointType::CLIENT:
      return "client";
    case VantagePointType::SERVER:
      return "server";
    case VantagePointType::NETWORK:
      return "network";
    case VantagePointType::UNKNOWN:
    default:
      return "unknown";
  }
}

VantagePointType VantagePointTypeFromString(std::string s)
{
  if (s == "client")
    return VantagePointType::CLIENT;
  else if (s == "server")
    return VantagePointType::SERVER;
  else if (s == "network")
    return VantagePointType::NETWORK;
  else
    return VantagePointType::UNKNOWN;
}


// ####### SimVantagePoint #######


// ####### SimHostVantagePoint #######

SimHostVantagePoint::SimHostVantagePoint(VantagePointType type, uint32_t nodeId)
    : SimVantagePoint(type, nodeId)
{
  if (type == VantagePointType::NETWORK)
    throw std::invalid_argument("Try to create host vantage point with type network.");
}

HostVantagePointPointer SimHostVantagePoint::ApplyFilter(const SimFilter &filter)
{
  HostVantagePointPointer hvpp = std::make_shared<SimHostVantagePoint>(*this);

  // Recursively apply filter to all flows
  for (auto it = m_simFlows.begin(); it != m_simFlows.end(); it++)
  {
    hvpp->m_simFlows[it->first] = it->second->ApplyFilter(filter);
  }

  return hvpp;
}

void SimHostVantagePoint::AddEvent(EventPointer simEvent)
{
  auto it = m_simFlows.find(simEvent->flowId);
  SimHostFlowPointer hostFlow;
  if (it == m_simFlows.end())
  {
    hostFlow = std::make_shared<SimHostFlow>(simEvent->flowId);
    m_simFlows.insert(std::make_pair(simEvent->flowId, hostFlow));
  }
  else
    hostFlow = it->second;

  hostFlow->AddEvent(simEvent);
}

uint32_t SimHostVantagePoint::GetEventCount()
{
  uint32_t count = 0;
  for (auto it = m_simFlows.begin(); it != m_simFlows.end(); it++)
  {
    count += it->second->GetEventCount();
  }
  return count;
}

bool SimHostVantagePoint::TryGetFlow(uint32_t flowId, SimHostFlowPointer &hostFlow)
{
  auto it = m_simFlows.find(flowId);
  if (it != m_simFlows.end())
  {
    hostFlow = it->second;
    return true;
  }

  return false;
}

std::set<uint32_t> SimHostVantagePoint::GetFlowIds()
{
  std::set<uint32_t> ids;
  for (auto it = m_simFlows.begin(); it != m_simFlows.end(); it++)
  {
    ids.insert(it->first);
  }

  return ids;
}

// ####### SimObsvVantagePoint #######

SimObsvVantagePoint::SimObsvVantagePoint(VantagePointType type, uint32_t nodeId)
    : SimVantagePoint(type, nodeId)
{
  if (type == VantagePointType::CLIENT || type == VantagePointType::SERVER)
    throw std::invalid_argument("Try to create observer vantage point with type client or server.");
}

ObsvVantagePointPointer SimObsvVantagePoint::ApplyFilter(const SimFilter &filter)
{
  ObsvVantagePointPointer ovpp = std::make_shared<SimObsvVantagePoint>(*this);

  // Recursively apply filter to all flows
  for (auto it = m_simFlows.begin(); it != m_simFlows.end(); it++)
  {
    ovpp->m_simFlows[it->first] = it->second->ApplyFilter(filter);
  }

  return ovpp;
}

void SimObsvVantagePoint::AddEvent(EventPointer simEvent)
{
  if (simEvent->IsPathEvent())
  {
    auto it = m_simPaths.find(simEvent->flowId);
    SimPathPointer path;
    if (it == m_simPaths.end())
    {
      path = std::make_shared<SimPath>(simEvent->flowId);
      m_simPaths.insert(std::make_pair(simEvent->flowId, path));
    }
    else
      path = it->second;
    path->AddEvent(simEvent);
  }
  else if (simEvent->IsPingClientEvent())
  {
    auto it = m_simPingClientPairs.find(simEvent->flowId);
    SimPingPairPointer pp;
    if (it == m_simPingClientPairs.end())
    {
      pp = std::make_shared<SimPingPair>(PingPairType::CLIENT, simEvent->flowId);
      m_simPingClientPairs.insert(std::make_pair(simEvent->flowId, pp));
    }
    else
      pp = it->second;
    pp->AddEvent(simEvent);
  }
  else if (simEvent->IsPingServerEvent())
  {
    auto it = m_simPingServerPairs.find(simEvent->flowId);
    SimPingPairPointer pp;
    if (it == m_simPingServerPairs.end())
    {
      pp = std::make_shared<SimPingPair>(PingPairType::SERVER, simEvent->flowId);
      m_simPingServerPairs.insert(std::make_pair(simEvent->flowId, pp));
    }
    else
      pp = it->second;
    pp->AddEvent(simEvent);
  }
  else
  {
    auto it = m_simFlows.find(simEvent->flowId);
    SimObsvFlowPointer obsvFlow;
    if (it == m_simFlows.end())
    {
      obsvFlow = std::make_shared<SimObserverFlow>(simEvent->flowId);
      m_simFlows.insert(std::make_pair(simEvent->flowId, obsvFlow));
    }
    else
      obsvFlow = it->second;

    obsvFlow->AddEvent(simEvent);
  }
}

uint32_t SimObsvVantagePoint::GetEventCount()
{
  uint32_t count = 0;
  for (auto it = m_simFlows.begin(); it != m_simFlows.end(); it++)
  {
    count += it->second->GetEventCount();
  }
  for (auto it = m_simPaths.begin(); it != m_simPaths.end(); it++)
  {
    count += it->second->GetEventCount();
  }
  return count;
}

bool SimObsvVantagePoint::TryGetFlow(uint32_t flowId, SimObsvFlowPointer &obsvFlow)
{
  auto it = m_simFlows.find(flowId);
  if (it != m_simFlows.end())
  {
    obsvFlow = it->second;
    return true;
  }

  return false;
}

SimObsvFlowPointer SimObsvVantagePoint::GetFlow(uint32_t flowId)
{
  auto it = m_simFlows.find(flowId);
  if (it != m_simFlows.end())
  {
    return it->second;
  }
  throw std::runtime_error("Flow id " + std::to_string(flowId) + " not found on observer " +
                           std::to_string(m_nodeId));
}

std::set<uint32_t> SimObsvVantagePoint::GetFlowIds()
{
  std::set<uint32_t> ids;
  for (auto it = m_simFlows.begin(); it != m_simFlows.end(); it++)
  {
    ids.insert(it->first);
  }

  return ids;
}

SimPathPointer SimObsvVantagePoint::GetPath(uint32_t pathId)
{
  auto it = m_simPaths.find(pathId);
  if (it != m_simPaths.end())
  {
    return it->second;
  }
  throw std::runtime_error("Flow id " + std::to_string(pathId) + " not found on observer " +
                           std::to_string(m_nodeId));
}

std::set<uint32_t> SimObsvVantagePoint::GetPathIds()
{
  std::set<uint32_t> ids;
  for (auto it = m_simPaths.begin(); it != m_simPaths.end(); it++)
  {
    ids.insert(it->first);
  }

  return ids;
}


}  // namespace simdata
