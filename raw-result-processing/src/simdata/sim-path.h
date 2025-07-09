#ifndef SIM_PATH_H
#define SIM_PATH_H

#include "sim-events.h"


namespace simdata {


class SimPath
{
public:
  SimPath(uint32_t pathId) : m_pathId(pathId) {}

  void AddEvent(EventPointer simEvent);

  uint32_t GetPathId() const { return m_pathId; }

  uint32_t GetEventCount() const;

  uint32_t GetSQPacketCount() const;

  uint32_t GetAbsoluteLBitLoss() const;
  int32_t GetAbsoluteFinalSQBitsLoss() const;
  double GetAbsoluteAvgSQBitsLoss() const;


  double GetRelativeLBitLoss() const;
  double GetRelativeFinalSQBitsLoss() const;
  double GetRelativeAvgSQBitsLoss() const;


protected:
  SimEventMap m_simEvents;  // Stores all events for this path
  uint32_t m_pathId;

private:
};
typedef std::shared_ptr<SimPath> SimPathPointer;

}  // namespace simdata


#endif  // SIM_PATH_H