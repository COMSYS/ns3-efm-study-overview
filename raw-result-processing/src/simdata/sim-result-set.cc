#include "sim-result-set.h"

#include <iostream>

namespace simdata {


SimResultSet::SimResultSet(simdjson::ondemand::object &qlog)
{
  using namespace simdjson;

  m_simId = qlog["title"].get_string().value();

  ondemand::object summary = qlog["summary"];
  try
  {
    ImportSummary(summary);
  }
  catch (const std::exception &e)
  {
    std::cerr << "SimResultSet failed to import summary: " << e.what() << '\n';
    throw;
  }

  ondemand::array traces = qlog["traces"];
  try
  {
    for (ondemand::object trace : traces)
    {
      ImportTrace(trace);
    }
  }
  catch (const std::exception &e)
  {
    std::cerr << "SimResultSet failed to import trace: " << e.what() << '\n';
    throw;
  }
}

// Private constructor for internal use
SimResultSet::SimResultSet() {}

void SimResultSet::ImportAndAppendResult(simdjson::ondemand::object &result)
{
  using namespace simdjson;
  ondemand::array traces = result["traces"];
  try
  {
    for (ondemand::object trace : traces)
    {
      ImportTrace(trace);
    }
  }
  catch (const std::exception &e)
  {
    std::cerr << "SimResultSet failed to import trace: " << e.what() << '\n';
    throw;
  }
}

void SimResultSet::ImportSummary(simdjson::ondemand::object &summary)
{
  using namespace simdjson;
  ondemand::object clientStats = summary["client_stats"];
  for (auto it : clientStats)
  {
    m_clientIds.insert(std::stoul(std::string(it.unescaped_key().value())));
  }

  ondemand::object serverStats = summary["server_stats"];
  for (auto it : serverStats)
  {
    m_serverIds.insert(std::stoul(std::string(it.unescaped_key().value())));
  }

  ondemand::object observerStats = summary["observer_stats"];
  for (auto it : observerStats)
  {
    uint32_t observerId = std::stoul(std::string(it.unescaped_key().value()));
    m_observerIds.insert(observerId);
    for (auto flow : it.value().get_object())
    {
      uint32_t flowId = std::stoul(std::string(flow.unescaped_key().value()));
      FlowStats fs;
      fs.totalPackets = flow.value()["total_packets"].get_uint64();
      fs.totalEfmPackets = flow.value()["total_efm_packets"].get_uint64();
      m_observerFlowStats[std::make_pair(observerId, flowId)] = fs;
    }
  }

  ondemand::object simConfig = summary["config"];

  m_simConfigJson = simConfig.raw_json().value();

  ondemand::array failedLinks = summary["failed_links"];
  for (ondemand::object it : failedLinks)
  {
    FailedLink fl;
    fl.delayMs = it["delayMs"].get_uint64();
    fl.lossRate = it["lossRate"].get_double();
    fl.sourceNodeId = it["nodeA"].get_uint64();
    fl.destNodeId = it["nodeB"].get_uint64();
    m_failedLinks[std::make_pair(fl.sourceNodeId, fl.destNodeId)] = fl;
  }


  ondemand::object hostConnStats = summary["host_connections"];
  for (auto it : hostConnStats)
  {
    FiveTuple ft;
    ft.sourceNodeId = it.value()["client_node_id"].get_uint64();
    ft.sourcePort = it.value()["client_port"].get_uint64();
    ft.destNodeId = it.value()["server_node_id"].get_uint64();
    ft.destPort = it.value()["server_port"].get_uint64();
    ft.protocol = it.value()["prot"].get_uint64();
    m_hostConnInfo[std::stoul(std::string(it.unescaped_key().value()))] = ft;
  }

  ondemand::object obsvFlowStats = summary["observer_flows"];
  for (auto it : obsvFlowStats)
  {
    FiveTuple ft;
    ft.sourceNodeId = it.value()["src_node_id"].get_uint64();
    ft.sourcePort = it.value()["src_port"].get_uint64();
    ft.destNodeId = it.value()["dst_node_id"].get_uint64();
    ft.destPort = it.value()["dst_port"].get_uint64();
    ft.protocol = it.value()["prot"].get_uint64();
    m_observerFlowInfo[std::stoul(std::string(it.unescaped_key().value()))] = ft;
  }

  // This is accessed more safely, because it was added later than the other fields
  ondemand::object obsvPathStats;
  auto error = summary["observer_paths"].get(obsvPathStats);
  if (error == NO_SUCH_FIELD)
  {
    std::cout << "Warning: Field 'observer_paths' not found in summary." << std::endl;
  }
  else if (error != SUCCESS)
  {
    std::cerr << "Error: Failed to parse 'observer_paths' field in summary." << std::endl;
    throw;
  }
  else
  {
    for (auto it : obsvPathStats)
    {
      PathInfo pinf;
      pinf.sourceNet = it.value()["src_net_addr"].get_string().value();
      pinf.destNet = it.value()["dst_net_addr"].get_string().value();
      pinf.sourceNodeIds = std::vector<uint32_t>();
      for (uint64_t srcNodeId : it.value()["src_node_ids"])
      {
        pinf.sourceNodeIds.push_back(srcNodeId);
      }
      pinf.destNodeIds = std::vector<uint32_t>();
      for (uint64_t dstNodeId : it.value()["dst_node_ids"])
      {
        pinf.destNodeIds.push_back(dstNodeId);
      }
      m_observerPathInfo[std::stoul(std::string(it.unescaped_key().value()))] = pinf;
    }
  }

  // This is accessed more safely, because it was added later than the other fields
  ondemand::object pingRoutes;
  error = summary["ping_routes"].get(pingRoutes);
  if (error == NO_SUCH_FIELD)
  {
    std::cout << "Warning: Field 'ping_routes' not found in summary." << std::endl;
  }
  else if (error != SUCCESS)
  {
    std::cerr << "Error: Failed to parse 'ping_routes' field in summary." << std::endl;
    throw;
  }
  else
  {
    for (auto it : pingRoutes)
    {
      std::string srcDstPair = std::string(it.unescaped_key().value());
      size_t index = srcDstPair.find('/');
      uint32_t srcNodeId = std::stoul(srcDstPair.substr(0, index));
      uint32_t dstNodeId = std::stoul(srcDstPair.substr(index + 1));
      auto &pathVec = m_pingPaths[std::make_pair(srcNodeId, dstNodeId)];
      for (uint64_t nodeId : it.value())
      {
        pathVec.push_back(nodeId);
      }
    }
  }

  // This is accessed more safely, because it was added later than the other fields
  ondemand::object linkSets;
  error = summary["link_sets"].get(linkSets);
  if (error == NO_SUCH_FIELD)
  {
    std::cout << "Warning: Field 'link_sets' not found in summary." << std::endl;
  }
  else if (error != SUCCESS)
  {
    std::cerr << "Error: Failed to parse 'link_sets' field in summary." << std::endl;
    throw;
  }
  else
  {
    ondemand::array coreLinks = linkSets["core_links"];
    for (ondemand::object link : coreLinks)
    {
      m_coreLinks.push_back(std::make_pair(link["src"].get_uint64(), link["dst"].get_uint64()));
    }
    ondemand::array edgeLinks = linkSets["edge_links"];
    for (ondemand::object link : edgeLinks)
    {
      m_edgeLinks.push_back(std::make_pair(link["src"].get_uint64(), link["dst"].get_uint64()));
    }
  }

  // This is accessed more safely, because it was added later than the other fields
  ondemand::array groundTruthStats;
  error = summary["gt_stats"].get(groundTruthStats);
  if (error == NO_SUCH_FIELD)
  {
    std::cout << "Warning: Field 'gt_stats' not found in summary." << std::endl;
  }
  else if (error != SUCCESS)
  {
    std::cerr << "Error: Failed to parse 'gt_stats' field in summary." << std::endl;
    throw;
  }
  else
  {
    for (ondemand::object gt : groundTruthStats)
    {
      auto &linkStats =
          m_linkGroundtruthStats[std::make_pair(gt["src"].get_uint64(), gt["dst"].get_uint64())];
      linkStats.lostPackets = gt["lost"].get_uint64();
      linkStats.receivedPackets = gt["recv"].get_uint64();
      ondemand::value _val;
      if (gt["dy_avg"].get(_val) == SUCCESS)
      {
        linkStats.delayAvgMus = _val.get_double();
      }
      if (gt["dy_std"].get(_val) == SUCCESS)
      {
        linkStats.delayStdMus = _val.get_double();
      }
      if (gt["dy_med"].get(_val) == SUCCESS)
      {
        linkStats.delayMedMus = _val.get_double();
      }
      if (gt["dy_99"].get(_val) == SUCCESS)
      {
        linkStats.delay99thMus = _val.get_double();
      }
      if (gt["dy_min"].get(_val) == SUCCESS)
      {
        linkStats.delayMinMus = _val.get_uint64();
      }
      if (gt["dy_max"].get(_val) == SUCCESS)
      {
        linkStats.delayMaxMus = _val.get_uint64();
      }
    }
  }

  // This is accessed more safely, because it was added later than the other fields
  ondemand::array backboneOverrides;
  error = summary["backbone_overrides"].get(backboneOverrides);
  if (error == NO_SUCH_FIELD)
  {
    std::cout << "Warning: Field 'backbone_overrides' not found in summary." << std::endl;
  }
  else if (error != SUCCESS)
  {
    std::cerr << "Error: Failed to parse 'backbone_overrides' field in summary." << std::endl;
    throw;
  }
  else
  {
    for (ondemand::object it : backboneOverrides)
    {
      LinkConfig lc;
      lc.delayMus = it["delayMus"].get_uint64();
      lc.sourceNodeId = it["nodeA"].get_uint64();
      lc.destNodeId = it["nodeB"].get_uint64();
      m_backboneOverrides[std::make_pair(lc.sourceNodeId, lc.destNodeId)] = lc;
    }
  }
}

void SimResultSet::ImportTrace(simdjson::ondemand::object &trace)
{
  using namespace simdjson;

  ondemand::object vp = trace["vantage_point"];

  std::string vp_name(vp["name"].get_string().value());
  size_t index = vp_name.find('/');
  uint32_t vp_nodeId = std::stoul(vp_name.substr(0, index));

  VantagePointType vp_type =
      VantagePointTypeFromString(std::string(vp["type"].get_string().value()));

  VantagePointPointer vp_p;
  HostVantagePointMap::iterator hvp_it;
  ObsvVantagePointMap::iterator ovp_it;

  switch (vp_type)
  {
    case VantagePointType::CLIENT:
      hvp_it = m_vpClients.find(vp_nodeId);
      if (hvp_it == m_vpClients.end())
      {
        HostVantagePointPointer hvp_p = std::make_shared<SimHostVantagePoint>(vp_type, vp_nodeId);
        m_vpClients.insert(std::make_pair(vp_nodeId, hvp_p));
        vp_p = hvp_p;
      }
      else
        vp_p = hvp_it->second;
      break;
    case VantagePointType::SERVER:
      hvp_it = m_vpServers.find(vp_nodeId);
      if (hvp_it == m_vpServers.end())
      {
        HostVantagePointPointer hvp_p = std::make_shared<SimHostVantagePoint>(vp_type, vp_nodeId);
        m_vpServers.insert(std::make_pair(vp_nodeId, hvp_p));
        vp_p = hvp_p;
      }
      else
        vp_p = hvp_it->second;
      break;
    case VantagePointType::NETWORK:
      ovp_it = m_vpObservers.find(vp_nodeId);
      if (ovp_it == m_vpObservers.end())
      {
        ObsvVantagePointPointer ovp_p = std::make_shared<SimObsvVantagePoint>(vp_type, vp_nodeId);
        m_vpObservers.insert(std::make_pair(vp_nodeId, ovp_p));
        vp_p = ovp_p;
      }
      else
        vp_p = ovp_it->second;
      break;
    case VantagePointType::UNKNOWN:
      std::cerr << "Unkown Vantage Point Type" << std::endl;
      return;
      break;
  }
  try
  {
    ondemand::array events = trace["events"];
    CreateAndStoreEvents(events, vp_p);
  }
  catch (const std::exception &e)
  {
    std::cerr << "SimResultSet failed to import trace (CreateAndStoreEvents): " << e.what() << '\n';
    throw;
  }
}

void SimResultSet::CreateAndStoreEvents(simdjson::ondemand::array &events, VantagePointPointer &vp)
{
  for (simdjson::ondemand::object it : events)
  {
    EventPointer ev_p = CreateEvent(it);
    vp->AddEvent(ev_p);
    m_eventCount[ev_p->eventType]++;
  }
}


SimResultSetPointer SimResultSet::ApplyFilter(const SimFilter &filter) const
{
  // Clone result set
  SimResultSetPointer srs = std::make_shared<SimResultSet>(*this);

  srs->m_filter = filter;


  // Apply filter recursively to all VantagePoints

  for (auto it = srs->m_vpClients.begin(); it != srs->m_vpClients.end(); it++)
  {
    srs->m_vpClients[it->first] = it->second->ApplyFilter(filter);
  }

  for (auto it = srs->m_vpServers.begin(); it != srs->m_vpServers.end(); it++)
  {
    srs->m_vpServers[it->first] = it->second->ApplyFilter(filter);
  }

  for (auto it = srs->m_vpObservers.begin(); it != srs->m_vpObservers.end(); it++)
  {
    srs->m_vpObservers[it->first] = it->second->ApplyFilter(filter);
  }

  return srs;
}


std::optional<FailedLink> SimResultSet::GetFailedLink(uint32_t sourceNodeId,
                                                      uint32_t destNodeId) const
{
  auto it = m_failedLinks.find(std::make_pair(sourceNodeId, destNodeId));
  if (it != m_failedLinks.end())
    return it->second;
  else
    return std::nullopt;
}

SimResultSet::LinkVector SimResultSet::GetAllLinks() const
{
  LinkVector allLinks;
  allLinks.reserve(m_edgeLinks.size() + m_coreLinks.size());
  allLinks.insert(allLinks.end(), m_edgeLinks.begin(), m_edgeLinks.end());
  allLinks.insert(allLinks.end(), m_coreLinks.begin(), m_coreLinks.end());
  return allLinks;
}

const FlowStats &SimResultSet::GetFlowStats(uint32_t observerId, uint32_t flowId) const
{
  auto it = m_observerFlowStats.find(std::make_pair(observerId, flowId));
  if (it != m_observerFlowStats.end())
    return it->second;
  else
    throw std::runtime_error("FlowStats not found");
}

bool SimResultSet::TryGetServerVP(uint32_t id, HostVantagePointPointer &vp) const
{
  auto it = m_vpServers.find(id);
  if (it == m_vpServers.end())
    return false;

  vp = it->second;
  return true;
}

bool SimResultSet::TryGetClientVP(uint32_t id, HostVantagePointPointer &vp) const
{
  auto it = m_vpClients.find(id);
  if (it == m_vpClients.end())
    return false;

  vp = it->second;
  return true;
}


bool SimResultSet::TryGetObserverVP(uint32_t id, ObsvVantagePointPointer &vp) const
{
  auto it = m_vpObservers.find(id);
  if (it == m_vpObservers.end())
    return false;

  vp = it->second;
  return true;
}

HostVantagePointPointer SimResultSet::GetServerVP(uint32_t id) const
{
  HostVantagePointPointer ptr;
  if (TryGetServerVP(id, ptr))
    return ptr;
  else
    throw std::runtime_error("SimResultSet::GetServerVP: No such VP");
}

HostVantagePointPointer SimResultSet::GetClientVP(uint32_t id) const
{
  HostVantagePointPointer ptr;
  if (TryGetClientVP(id, ptr))
    return ptr;
  else
    throw std::runtime_error("SimResultSet::GetClientVP: No such VP");
}

ObsvVantagePointPointer SimResultSet::GetObserverVP(uint32_t id) const
{
  ObsvVantagePointPointer ptr;
  if (TryGetObserverVP(id, ptr))
    return ptr;
  else
    throw std::runtime_error("SimResultSet::GetObserverVP: No such VP");
}

std::set<uint32_t> SimResultSet::GetServerVPIds(bool relevantOnly) const
{
  if (relevantOnly)
  {
    std::set<uint32_t> ids;
    for (auto it = m_vpServers.begin(); it != m_vpServers.end(); it++)
    {
      if (it->second->GetEventCount() > 0)
        ids.insert(it->first);
    }
    return ids;
  }
  else
    return m_serverIds;
}

std::set<uint32_t> SimResultSet::GetClientVPIds(bool relevantOnly) const
{
  if (relevantOnly)
  {
    std::set<uint32_t> ids;
    for (auto it = m_vpClients.begin(); it != m_vpClients.end(); it++)
    {
      if (it->second->GetEventCount() > 0)
        ids.insert(it->first);
    }
    return ids;
  }
  else
    return m_clientIds;
}

std::set<uint32_t> SimResultSet::GetObserverVPIds(bool relevantOnly, bool realOnly) const
{
  std::set<uint32_t> ids;
  if (relevantOnly)
  {
    for (auto it = m_vpObservers.begin(); it != m_vpObservers.end(); it++)
    {
      if (it->second->GetEventCount() > 0)
        ids.insert(it->first);
    }
  }
  else
  {
    ids = m_observerIds;
  }

  if (realOnly)
  {
    std::set<uint32_t> realIds;
    auto serverIds = GetServerVPIds(false);
    auto clientIds = GetClientVPIds(false);

    for (auto &id : ids)
    {
      if (serverIds.find(id) == serverIds.end() && clientIds.find(id) == clientIds.end())
        realIds.insert(id);
    }
    return realIds;
  }
  else
    return ids;
}

std::set<uint32_t> SimResultSet::GetClientConnIds() const
{
  std::set<uint32_t> ids;
  for (auto &cid : GetClientVPIds(true))
  {
    auto fids = GetClientConnIds(cid);
    ids.insert(fids.begin(), fids.end());
  }
  return ids;
}

std::set<uint32_t> SimResultSet::GetClientConnIds(uint32_t clientId) const
{
  std::set<uint32_t> ids;
  HostVantagePointPointer client;
  if (!TryGetClientVP(clientId, client))
    throw std::runtime_error("Invalid client id.");

  return client->GetFlowIds();
}

std::set<uint32_t> SimResultSet::GetServerConnIds() const
{
  std::set<uint32_t> ids;
  for (auto &sid : GetServerVPIds(true))
  {
    auto fids = GetServerConnIds(sid);
    ids.insert(fids.begin(), fids.end());
  }
  return ids;
}

std::set<uint32_t> SimResultSet::GetServerConnIds(uint32_t serverId) const
{
  std::set<uint32_t> ids;
  HostVantagePointPointer server;
  if (!TryGetServerVP(serverId, server))
    throw std::runtime_error("Invalid server id.");

  return server->GetFlowIds();
}

std::set<uint32_t> SimResultSet::GetObserverFlowIds(bool realOnly) const
{
  std::set<uint32_t> ids;
  for (auto &oid : GetObserverVPIds(true, realOnly))
  {
    auto fids = GetObserverFlowIds(oid);
    ids.insert(fids.begin(), fids.end());
  }
  return ids;
}

std::set<uint32_t> SimResultSet::GetObserverFlowIds(uint32_t observerId) const
{
  std::set<uint32_t> ids;
  ObsvVantagePointPointer obsv;
  if (!TryGetObserverVP(observerId, obsv))
    throw std::runtime_error("Invalid observer id.");

  return obsv->GetFlowIds();
}

std::set<uint32_t> SimResultSet::GetObserverFlowIds(uint32_t observerId, std::map<uint32_t, std::set<uint32_t>> flowSelectionMap) const
{
  std::set<uint32_t> ids;
  ObsvVantagePointPointer obsv;
  if (!TryGetObserverVP(observerId, obsv))
    throw std::runtime_error("Invalid observer id.");
  std::set<uint32_t> tmp_flow_ids = obsv->GetFlowIds();
  std::set<uint32_t> final_flow_ids;
  for (const auto flow_id: tmp_flow_ids){
    if (flowSelectionMap[observerId].count(flow_id)){
        final_flow_ids.insert(flow_id);
    }
  }
  return final_flow_ids;
}

std::vector<ObsvVantagePointPointer> SimResultSet::CalculateFlowPath(uint32_t flowId) const
{
  std::vector<ObsvVantagePointPointer> flowPathVec;

  std::map<double, ObsvVantagePointPointer> flowPath;


  for (auto it = m_vpObservers.begin(); it != m_vpObservers.end(); it++)
  {
    SimObsvFlowPointer obsvFlow;
    if (it->second->TryGetFlow(flowId, obsvFlow))
    {
      double begin = obsvFlow->GetFlowBegin();
      if (flowPath.find(begin) != flowPath.end())
        throw std::runtime_error("Duplicate flow begin time.");
      flowPath[begin] = it->second;
    }
  }

  for (auto it = flowPath.begin(); it != flowPath.end(); it++) flowPathVec.push_back(it->second);

  return flowPathVec;
}

uint32_t SimResultSet::GetReverseFlowId(uint32_t flowId) const
{
  auto iter = m_observerFlowInfo.find(flowId);
  if (iter == m_observerFlowInfo.end())
    throw std::runtime_error("FlowId not found.");

  const FiveTuple &ft = iter->second;

  FiveTuple rev = ft.GetReversed();

  for (auto it = m_observerFlowInfo.begin(); it != m_observerFlowInfo.end(); it++)
  {
    if (it->second == rev)
      return it->first;
  }

  throw std::runtime_error("Reverse flow ID not found.");
}

uint32_t SimResultSet::GetObserverFlowStart(uint32_t flowId) const
{
  auto iter = m_observerFlowInfo.find(flowId);
  if (iter == m_observerFlowInfo.end())
    throw std::runtime_error("FlowId not found.");
  return iter->second.sourceNodeId;
}

uint32_t SimResultSet::GetObserverFlowEnd(uint32_t flowId) const
{
  auto iter = m_observerFlowInfo.find(flowId);
  if (iter == m_observerFlowInfo.end())
    throw std::runtime_error("FlowId not found.");
  return iter->second.destNodeId;
}

std::vector<uint32_t> SimResultSet::GetPingPath(uint32_t srcNodeId, uint32_t destNodeId) const
{
  auto it = m_pingPaths.find(std::make_pair(srcNodeId, destNodeId));
  if (it == m_pingPaths.end())
    throw std::runtime_error("Ping path between " + std::to_string(srcNodeId) + " and " +
                             std::to_string(destNodeId) + " not found.");
  else
    return it->second;
}

void SimResultSet::PrintEventCounts()
{
  std::cout << "Event Counts:" << std::endl;
  for (auto it = m_eventCount.begin(); it != m_eventCount.end(); it++)
  {
    std::cout << "\t" << EventTypeToString(it->first) << ": " << it->second << std::endl;
  }
}


FiveTuple FiveTuple::GetReversed() const
{
  FiveTuple newFt;
  newFt.sourceNodeId = destNodeId;
  newFt.destNodeId = sourceNodeId;
  newFt.sourcePort = destPort;
  newFt.destPort = sourcePort;
  newFt.protocol = protocol;
  return newFt;
}

std::string FiveTuple::Serialize() const
{
  return std::to_string(sourceNodeId) + ":" + std::to_string(sourcePort) + ":" +
         std::to_string(destNodeId) + ":" + std::to_string(destPort) + ":" +
         std::to_string(protocol);
}

bool operator==(const FiveTuple &lhs, const FiveTuple &rhs)
{
  if (lhs.sourceNodeId != rhs.sourceNodeId)
    return false;
  if (lhs.destNodeId != rhs.destNodeId)
    return false;
  if (lhs.sourcePort != rhs.sourcePort)
    return false;
  if (lhs.destPort != rhs.destPort)
    return false;
  if (lhs.protocol != rhs.protocol)
    return false;
  return true;
}

std::string PathInfo::Serialize() const { return sourceNet + ":" + destNet; }

void to_json(nlohmann::json &j, const LinkStats &stats)
{
  j["lostPackets"] = stats.lostPackets;
  j["receivedPackets"] = stats.receivedPackets;
  if (stats.delayAvgMus.has_value())
    j["delayAvgMus"] = stats.delayAvgMus.value();
  else
    j["delayAvgMus"] = nullptr;

  if (stats.delayStdMus.has_value())
    j["delayStdMus"] = stats.delayStdMus.value();
  else
    j["delayStdMus"] = nullptr;

  if (stats.delayMedMus.has_value())
    j["delayMedMus"] = stats.delayMedMus.value();
  else
    j["delayMedMus"] = nullptr;

  if (stats.delay99thMus.has_value())
    j["delay99thMus"] = stats.delay99thMus.value();
  else
    j["delay99thMus"] = nullptr;

  if (stats.delayMinMus.has_value())
    j["delayMinMus"] = stats.delayMinMus.value();
  else
    j["delayMinMus"] = nullptr;

  if (stats.delayMaxMus.has_value())
    j["delayMaxMus"] = stats.delayMaxMus.value();
  else
    j["delayMaxMus"] = nullptr;
}

void from_json(const nlohmann::json &j, LinkStats &stats)
{
  j.at("lostPackets").get_to(stats.lostPackets);
  j.at("receivedPackets").get_to(stats.receivedPackets);
  if (j.contains("delayAvgMus"))
    stats.delayAvgMus = j.at("delayAvgMus").get<double>();
  if (j.contains("delayStdMus"))
    stats.delayStdMus = j.at("delayStdMus").get<double>();
  if (j.contains("delayMedMus"))
    stats.delayMedMus = j.at("delayMedMus").get<double>();
  if (j.contains("delay99thMus"))
    stats.delay99thMus = j.at("delay99thMus").get<double>();
  if (j.contains("delayMinMus"))
    stats.delayMinMus = j.at("delayMinMus").get<uint32_t>();
  if (j.contains("delayMaxMus"))
    stats.delayMaxMus = j.at("delayMaxMus").get<uint32_t>();
}

}  // namespace simdata
