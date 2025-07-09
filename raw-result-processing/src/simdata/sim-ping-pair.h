#ifndef SIM_PING_PAIR
#define SIM_PING_PAIR

#include "sim-events.h"
#include <list>


namespace simdata {

enum class PingPairType
{
  CLIENT,
  SERVER
};

class SimPingPair
{
public:
  SimPingPair(PingPairType ppType, uint32_t targetNodeId)
      : m_ppType(ppType), m_targetNodeId(targetNodeId)
  {
  }

  void AddEvent(EventPointer simEvent);

  uint32_t GetTargetNodeId() const { return m_targetNodeId; }

  PingPairType GetPingPairType() const { return m_ppType; }

  uint32_t GetEventCount() const;

  uint32_t GetAbsoluteLoss() const;
  std::optional<double> GetAvgDelay() const;
  std::optional<std::list<double>> GetRawPingDelayValues() const;

  double GetRelativeLoss() const;


protected:
  SimEventMap m_simEvents;  // Stores all events for this ping pair
  PingPairType m_ppType;
  uint32_t m_targetNodeId;

private:
};
typedef std::shared_ptr<SimPingPair> SimPingPairPointer;

}  // namespace simdata


#endif  // SIM_PING_PAIR