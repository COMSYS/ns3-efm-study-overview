#ifndef SIM_RESULT_SET_H
#define SIM_RESULT_SET_H

#include <simdjson.h>

#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <set>
#include <vector>

#include "sim-events.h"
#include "sim-filter.h"
#include "sim-vantage-point.h"

namespace simdata {

typedef std::string SimId;

class SimResultSet;
typedef std::shared_ptr<SimResultSet> SimResultSetPointer;

struct FiveTuple
{
  uint32_t sourceNodeId;
  uint32_t destNodeId;
  uint16_t sourcePort;
  uint16_t destPort;
  uint8_t protocol;

  FiveTuple GetReversed() const;
  std::string Serialize() const;
};

struct PathInfo
{
  std::string sourceNet;
  std::string destNet;
  std::vector<uint32_t> sourceNodeIds;
  std::vector<uint32_t> destNodeIds;

  std::string Serialize() const;
};

struct FlowStats
{
  uint32_t totalPackets = 0;
  uint32_t totalEfmPackets = 0;
};

struct FailedLink
{
  uint32_t sourceNodeId;
  uint32_t destNodeId;
  double lossRate;
  uint32_t delayMs;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FailedLink, sourceNodeId, destNodeId, lossRate, delayMs)

struct LinkConfig
{
  uint32_t sourceNodeId;
  uint32_t destNodeId;
  uint32_t delayMus;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LinkConfig, sourceNodeId, destNodeId, delayMus)

struct LinkStats
{
  uint32_t lostPackets = 0;
  uint32_t receivedPackets = 0;
  std::optional<double> delayAvgMus;
  std::optional<double> delayStdMus;
  std::optional<double> delayMedMus;
  std::optional<double> delay99thMus;
  std::optional<uint32_t> delayMinMus;
  std::optional<uint32_t> delayMaxMus;
};
void to_json(nlohmann::json &j, const LinkStats &stats);
void from_json(const nlohmann::json &j, LinkStats &stats);


bool operator==(const FiveTuple &lhs, const FiveTuple &rhs);


// Stores all results from a single simulation run
class SimResultSet
{
public:
  SimResultSet(simdjson::ondemand::object &qlog);

  void ImportAndAppendResult(simdjson::ondemand::object &result);

  SimResultSetPointer ApplyFilter(const SimFilter &filter) const;


  SimId GetSimId() const { return m_simId; }
  std::string GetSimConfigJson() const { return m_simConfigJson; }

  typedef std::map<uint32_t, FiveTuple> FlowInfoMap;
  typedef std::map<uint32_t, PathInfo> PathInfoMap;
  typedef std::map<std::pair<uint32_t, uint32_t>, FailedLink> FailedLinkMap;
  typedef std::map<std::pair<uint32_t, uint32_t>, LinkConfig> LinkOverrideMap;
  typedef std::pair<uint32_t, uint32_t> Link;
  typedef std::vector<Link> LinkVector;

  const FlowInfoMap &GetObserverFlowInfo() const { return m_observerFlowInfo; }
  const FlowInfoMap &GetHostConnInfo() const { return m_hostConnInfo; }
  const PathInfoMap &GetObserverPathInfo() const { return m_observerPathInfo; }
  const FailedLinkMap &GetFailedLinks() const { return m_failedLinks; }
  std::optional<FailedLink> GetFailedLink(uint32_t sourceNodeId, uint32_t destNodeId) const;
  const LinkVector &GetEdgeLinks() const { return m_edgeLinks; }
  const LinkVector &GetCoreLinks() const { return m_coreLinks; }
  LinkVector GetAllLinks() const;
  const LinkOverrideMap &GetBackboneOverrides() const { return m_backboneOverrides; }
  const std::map<Link, LinkStats> &GetLinkGroundtruthStats() const
  {
    return m_linkGroundtruthStats;
  }

  const FlowStats &GetFlowStats(uint32_t observerId, uint32_t flowId) const;

  const std::optional<SimFilter> &GetFilter() const { return m_filter; }

  bool TryGetServerVP(uint32_t id, HostVantagePointPointer &vp) const;
  bool TryGetClientVP(uint32_t id, HostVantagePointPointer &vp) const;
  bool TryGetObserverVP(uint32_t id, ObsvVantagePointPointer &vp) const;

  HostVantagePointPointer GetServerVP(uint32_t id) const;
  HostVantagePointPointer GetClientVP(uint32_t id) const;
  ObsvVantagePointPointer GetObserverVP(uint32_t id) const;

  std::set<uint32_t> GetServerVPIds(bool relevantOnly) const;
  std::set<uint32_t> GetClientVPIds(bool relevantOnly) const;
  /// @brief Get all observer VP ids with certain filters
  /// @param relevantOnly Only include observers that recorded any events
  /// @param realOnly Only include observers at core nodes, not the ones at endhost nodes
  std::set<uint32_t> GetObserverVPIds(bool relevantOnly, bool realOnly) const;

  std::set<uint32_t> GetClientConnIds() const;
  std::set<uint32_t> GetClientConnIds(uint32_t clientId) const;
  std::set<uint32_t> GetServerConnIds() const;
  std::set<uint32_t> GetServerConnIds(uint32_t serverId) const;
  std::set<uint32_t> GetObserverFlowIds(bool realOnly) const;
  std::set<uint32_t> GetObserverFlowIds(uint32_t observerId) const;
  std::set<uint32_t> GetObserverFlowIds(uint32_t observerId, std::map<uint32_t, std::set<uint32_t>> flowSelectionMap) const;

  std::vector<ObsvVantagePointPointer> CalculateFlowPath(uint32_t flowId) const;
  uint32_t GetReverseFlowId(uint32_t flowId) const;
  uint32_t GetObserverFlowStart(uint32_t flowId) const;
  uint32_t GetObserverFlowEnd(uint32_t flowId) const;

  std::vector<uint32_t> GetPingPath(uint32_t srcNodeId, uint32_t destNodeId) const;

  void PrintEventCounts();


protected:
  std::map<SimEventType, uint32_t> m_eventCount;  // Stores the number of events of each type

  typedef std::map<uint32_t, HostVantagePointPointer> HostVantagePointMap;
  typedef std::map<uint32_t, ObsvVantagePointPointer> ObsvVantagePointMap;
  HostVantagePointMap m_vpClients;    // Stores all vantage points with type client
  HostVantagePointMap m_vpServers;    // Stores all vantage points with type server
  ObsvVantagePointMap m_vpObservers;  // Stores all vantage points with type network

  // Stores the ids of all clients, servers, and observers based on the summary
  // Useful if host or observer traces are disable, so we cannot use m_vpClients, m_vpServers, or
  // m_vpObservers
  std::set<uint32_t> m_clientIds;
  std::set<uint32_t> m_serverIds;
  std::set<uint32_t> m_observerIds;

  SimId m_simId;

  std::string m_simConfigJson;

  // Stores the five tuple associated with each observer flow id
  FlowInfoMap m_observerFlowInfo;
  // Stores the five tuple associated with each host connection id
  FlowInfoMap m_hostConnInfo;
  // Stores the links configured to fail
  FailedLinkMap m_failedLinks;
  // Stores the backbone link overrides
  LinkOverrideMap m_backboneOverrides;
  // Stores the path info associated with each observer path id
  PathInfoMap m_observerPathInfo;

  // Stores the flow stats per observer id and flow id
  std::map<std::pair<uint32_t, uint32_t>, FlowStats> m_observerFlowStats;

  std::map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>> m_pingPaths;

  LinkVector m_edgeLinks;
  LinkVector m_coreLinks;

  std::map<Link, LinkStats> m_linkGroundtruthStats;

  std::optional<SimFilter> m_filter;


  void ImportSummary(simdjson::ondemand::object &summary);
  void ImportTrace(simdjson::ondemand::object &trace);


  void CreateAndStoreEvents(simdjson::ondemand::array &events, VantagePointPointer &vp);

private:
  SimResultSet();
};

}  // namespace simdata

#endif  // SIM_RESULT_SET_H