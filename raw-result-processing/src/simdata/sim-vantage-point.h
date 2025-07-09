#ifndef SIM_VANTAGE_POINT_H
#define SIM_VANTAGE_POINT_H

#include <stdint.h>

#include <set>

#include "sim-events.h"
#include "sim-filter.h"
#include "sim-flow.h"
#include "sim-path.h"
#include "sim-ping-pair.h"

namespace simdata {

class SimVantagePoint;
class SimHostVantagePoint;
class SimObsvVantagePoint;
typedef std::shared_ptr<SimVantagePoint> VantagePointPointer;
typedef std::shared_ptr<SimHostVantagePoint> HostVantagePointPointer;
typedef std::shared_ptr<SimObsvVantagePoint> ObsvVantagePointPointer;

enum class VantagePointType
{
  CLIENT,
  SERVER,
  NETWORK,
  UNKNOWN
};

std::string VantagePointTypeToString(VantagePointType &vpt);
VantagePointType VantagePointTypeFromString(std::string s);

NLOHMANN_JSON_SERIALIZE_ENUM(VantagePointType, {{VantagePointType::UNKNOWN, "unknown"},
                                                {VantagePointType::CLIENT, "client"},
                                                {VantagePointType::SERVER, "server"},
                                                {VantagePointType::NETWORK, "network"}})

class SimVantagePoint
{
public:
  virtual ~SimVantagePoint() = default;
  const VantagePointType m_type;
  const uint32_t m_nodeId;

  virtual void AddEvent(EventPointer simEvent) = 0;

  virtual uint32_t GetEventCount() = 0;

protected:
  SimVantagePoint(VantagePointType type, uint32_t nodeId) : m_type(type), m_nodeId(nodeId) {}
};

class SimHostVantagePoint : public SimVantagePoint
{
public:
  SimHostVantagePoint(VantagePointType type, uint32_t nodeId);

  HostVantagePointPointer ApplyFilter(const SimFilter &filter);

  virtual void AddEvent(EventPointer simEvent) override;

  virtual uint32_t GetEventCount() override;

  // Returns true if a flow with the specified id is found, false otherwise
  bool TryGetFlow(uint32_t flowId, SimHostFlowPointer &hostFlow);

  std::set<uint32_t> GetFlowIds();

protected:
  typedef std::map<uint32_t, SimHostFlowPointer> SimHostFlowMap;
  SimHostFlowMap m_simFlows;

private:
};

class SimObsvVantagePoint : public SimVantagePoint
{
public:
  SimObsvVantagePoint(VantagePointType type, uint32_t nodeId);

  ObsvVantagePointPointer ApplyFilter(const SimFilter &filter);

  virtual void AddEvent(EventPointer simEvent) override;

  virtual uint32_t GetEventCount() override;

  // Returns true if a flow with the specified id is found, false otherwise
  bool TryGetFlow(uint32_t flowId, SimObsvFlowPointer &obsvFlow);

  // Returns the flow associated with the specified id, throws exception if id not found
  SimObsvFlowPointer GetFlow(uint32_t flowId);

  std::set<uint32_t> GetFlowIds();

  SimPathPointer GetPath(uint32_t pathId);

  std::set<uint32_t> GetPathIds();

  typedef std::map<uint32_t, SimPingPairPointer> SimPingPairMap;
  const SimPingPairMap &GetClientPingPairs() const { return m_simPingClientPairs; }
  const SimPingPairMap &GetServerPingPairs() const { return m_simPingServerPairs; }

protected:
  typedef std::map<uint32_t, SimObsvFlowPointer> SimObsvFlowMap;
  SimObsvFlowMap m_simFlows;

  typedef std::map<uint32_t, SimPathPointer> SimPathMap;
  SimPathMap m_simPaths;

  SimPingPairMap m_simPingClientPairs;
  SimPingPairMap m_simPingServerPairs;

private:
};


}  // namespace simdata


#endif  // SIM_VANTAGE_POINT_H