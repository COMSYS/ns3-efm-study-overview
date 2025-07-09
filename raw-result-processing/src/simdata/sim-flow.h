#ifndef SIM_FLOW_H
#define SIM_FLOW_H

#include "sim-events.h"
#include "sim-filter.h"
#include <list>

namespace simdata {

class SimHostFlow;
class SimObserverFlow;

typedef std::shared_ptr<SimHostFlow> SimHostFlowPointer;
typedef std::shared_ptr<SimObserverFlow> SimObsvFlowPointer;

class SimFlow
{
public:
  SimFlow(uint32_t flowId) : m_flowId(flowId) {}
  virtual ~SimFlow() = default;

  void AddEvent(EventPointer simEvent);

  uint32_t GetEventCount();


  uint32_t GetFlowId();

protected:
  SimEventMap m_simEvents;  // Stores all events for this flow

  uint32_t m_flowId;

private:
};

enum class LossMmntType
{
  EFM_Q,
  EFM_R,
  EFM_L,
  EFM_T_FULL,
  EFM_T_HALF,
  GT_ALL,  // Includes GT_ACK
  GT_ACK   // Only ACK loss
};
const std::array<LossMmntType, 7> lossMmntTypes = {
    LossMmntType::EFM_Q,      LossMmntType::EFM_R,  LossMmntType::EFM_L, LossMmntType::EFM_T_FULL,
    LossMmntType::EFM_T_HALF, LossMmntType::GT_ALL, LossMmntType::GT_ACK};
std::string LossMmntTypeToString(const LossMmntType &lossMmntType);

class SimObserverFlow : public SimFlow
{
public:
  SimObserverFlow(uint32_t flowId) : SimFlow(flowId) {}

  SimObsvFlowPointer ApplyFilter(const SimFilter &filter);

  double GetFlowBegin();


  std::optional<double> GetAvgSpinRTDelay(double time_filter) const;
  std::optional<uint32_t> GetMinSpinRTDelay(double time_filter) const;
  std::optional<uint32_t> GetMaxSpinRTDelay(double time_filter) const;
  std::optional<std::list<double>> GetRawSpinRTValues(double time_filter) const;

  std::optional<double> GetAvgSpinEtEDelay(double time_filter) const;
  std::optional<uint32_t> GetMinSpinEtEDelay(double time_filter) const;
  std::optional<uint32_t> GetMaxSpinEtEDelay(double time_filter) const;
  std::optional<std::list<double>> GetRawSpinEtEValues(double time_filter) const;

  std::optional<double> GetAvgTcpHRTDelay() const;
  std::optional<uint32_t> GetMinTcpHRTDelay() const;
  std::optional<uint32_t> GetMaxTcpHRTDelay() const;
  std::optional<std::list<double>> GetRawTcpHRTValues() const;

  uint32_t GetAbsoluteQBitLoss() const;
  uint32_t GetAbsoluteQBitPacketCount() const;
  uint32_t GetAbsoluteRBitLoss() const;
  uint32_t GetAbsoluteLBitLoss() const;
  uint32_t GetAbsoluteTBitFullLoss() const;
  uint32_t GetAbsoluteTBitHalfLoss() const;
  uint32_t GetAbsoluteSeqLoss() const;
  uint32_t GetAbsoluteAckSeqLoss() const;
  uint32_t GetAbsoluteTcpReordering() const;

  double GetRelativeQBitLoss() const;
  double GetRelativeRBitLoss() const;
  double GetRelativeLBitLoss() const;
  double GetRelativeTBitFullLoss() const;
  double GetRelativeTBitHalfLoss() const;
  double GetRelativeSeqLoss() const;
  double GetRelativeAckSeqLoss() const;
  double GetRelativeTcpReordering() const;

  void GetFinalSeqLoss(uint32_t &loss, uint32_t &pktCount) const;
  void GetFinalAckSeqLoss(uint32_t &loss, uint32_t &pktCount) const;

  uint32_t GetAbsolutePacketCountLossMmnt(LossMmntType lossMmntType) const;
  uint32_t GetAbsoluteLossMmnt(LossMmntType lossMmntType) const;
  double GetRelativeLossMmnt(LossMmntType lossMmntType) const;

protected:

private:
};

class SimHostFlow : public SimFlow
{
public:
  SimHostFlow(uint32_t flowId) : SimFlow(flowId) {}

  SimHostFlowPointer ApplyFilter(const SimFilter &filter);

  uint32_t GetTotalLBitsSent() const;

  std::optional<double> GetAvgGtTransDelay() const;
  std::optional<uint32_t> GetMinGtTransDelay() const;
  std::optional<uint32_t> GetMaxGtTransDelay() const;

  std::optional<double> GetAvgGtAppDelay() const;
  std::optional<uint32_t> GetMinGtAppDelay() const;
  std::optional<uint32_t> GetMaxGtAppDelay() const;

protected:

private:
};

}  // namespace simdata


#endif  // SIM_FLOW_H