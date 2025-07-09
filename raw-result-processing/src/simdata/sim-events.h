#ifndef SIM_EVENTS_H
#define SIM_EVENTS_H

#include <memory>
#include <nlohmann/json.hpp>
#include <optional>

#include "sim-filter.h"
#include "simdjson.h"

namespace simdata {

// Allows comparison of shared pointers via a specific comparator of the underlying object
// See https://stackoverflow.com/questions/11774761
template <typename T, typename Pred = std::less<T>>
struct shared_ptr_compare : Pred
{
  shared_ptr_compare(Pred const& p = Pred()) : Pred(p) {}

  bool operator()(std::shared_ptr<T> const p1, std::shared_ptr<T> const p2) const
  {
    return Pred::operator()(*p1, *p2);
  }
};

enum class SimEventType
{
  UNKNOWN = 0,
  HOST_GT_TRANS_DELAY = 1,
  HOST_GT_APP_DELAY = 2,
  HOST_SPIN_BIT_UDPATE = 3,
  HOST_L_BIT_COUNTER_UPDATE = 4,
  HOST_L_BIT_SET = 5,
  HOST_Q_BIT_UPDATE = 6,
  HOST_R_BIT_UPDATE = 7,
  HOST_R_BIT_BLOCK_UPDATE = 8,
  HOST_T_BIT_SET = 9,
  HOST_T_BIT_PHASE_UPDATE = 10,
  OBSV_FLOW_BEGIN = 11,
  OBSV_SEQ_LOSS = 12,
  OBSV_ACK_SEQ_LOSS = 13,
  OBSV_SPIN_BIT_EDGE = 14,
  OBSV_SPIN_BIT_DELAY = 15,
  OBSV_L_BIT_SET = 16,
  OBSV_Q_BIT_CHANGE = 17,
  OBSV_Q_BIT_LOSS = 18,
  OBSV_R_BIT_CHANGE = 19,
  OBSV_R_BIT_LOSS = 20,
  OBSV_T_BIT_SET = 21,
  OBSV_T_BIT_PHASE_UPDATE = 22,
  OBSV_T_BIT_FULL_LOSS = 23,
  OBSV_T_BIT_HALF_LOSS = 24,
  OBSV_P_L_BIT_SET = 25,
  OBSV_P_SQ_BITS_LOSS = 26,
  OBSV_TCP_DART_DELAY = 27,
  OBSV_TCP_REORDERING = 28,
  PING_RT_DELAY = 29,
  PING_ETE_DELAY = 30,
  PING_RT_LOSS = 31,
  PING_ETE_LOSS = 32
};


std::string EventTypeToString(const SimEventType& eventType);


enum class TBitObserverPhase
{
  GEN,  // Generation phase
  PAUSE_BEGIN_GEN,
  PAUSE_FULL_GEN,  // Observed at least one spin period without TBits
  REF,             // Reflection phase
  PAUSE_BEGIN_REF,
  PAUSE_FULL_REF,  // Observed at least one spin period without TBits
  OP_ERROR         // Observer phase error
};

TBitObserverPhase TBitObserverPhaseFromString(std::string phaseString);

NLOHMANN_JSON_SERIALIZE_ENUM(TBitObserverPhase,
                             {{TBitObserverPhase::OP_ERROR, "error"},
                              {TBitObserverPhase::GEN, "gen"},
                              {TBitObserverPhase::PAUSE_BEGIN_GEN, "gen_pause_begin"},
                              {TBitObserverPhase::PAUSE_FULL_GEN, "gen_pause_full"},
                              {TBitObserverPhase::REF, "ref"},
                              {TBitObserverPhase::PAUSE_BEGIN_REF, "ref_pause_begin"},
                              {TBitObserverPhase::PAUSE_FULL_REF, "ref_pause_full"}})

enum class TBitClientPhase
{
  GEN1,
  GEN2,
  PAUSE_GEN,
  REF1,
  REF2,
  PAUSE_REF,
  CP_ERROR  // client phase error
};

TBitClientPhase TBitClientPhaseFromString(std::string phaseString);

NLOHMANN_JSON_SERIALIZE_ENUM(TBitClientPhase, {{TBitClientPhase::CP_ERROR, "error"},
                                               {TBitClientPhase::GEN1, "gen1"},
                                               {TBitClientPhase::GEN2, "gen2"},
                                               {TBitClientPhase::PAUSE_GEN, "gen_pause"},
                                               {TBitClientPhase::REF1, "ref1"},
                                               {TBitClientPhase::REF2, "ref2"},
                                               {TBitClientPhase::PAUSE_REF, "ref_pause"}})

struct SimEvent
{
  SimEvent(SimEventType eventType, double time, uint32_t flowId)
      : eventType(eventType), time(time), flowId(flowId)
  {
  }
  virtual ~SimEvent() = default;

  virtual SimEvent* Clone() const { return new SimEvent(*this); }

  SimEventType eventType;
  double time;
  uint32_t flowId;

  bool IsPathEvent() const;
  bool IsPingClientEvent() const;
  bool IsPingServerEvent() const;


  bool operator<(const SimEvent& other) const { return time < other.time; }
};

typedef std::shared_ptr<SimEvent> EventPointer;
typedef std::multiset<EventPointer, shared_ptr_compare<SimEvent>> SimEventSet;
typedef std::map<SimEventType, SimEventSet> SimEventMap;

void FilterSimEventSet(SimEventMap& eventMap, const SimFilter& filter, bool obsvEvents = true,
                       bool hostEvents = true);


struct EfmBitUpdateEvent : SimEvent
{
  EfmBitUpdateEvent(SimEventType eventType, double time, uint32_t flowId, bool newState,
                    uint32_t seq)
      : SimEvent(eventType, time, flowId), new_state(newState), seq(seq)
  {
  }
  virtual EfmBitUpdateEvent* Clone() const override { return new EfmBitUpdateEvent(*this); }
  bool new_state;
  uint32_t seq;
};

struct EfmLBitCounterUpdateEvent : SimEvent
{
  EfmLBitCounterUpdateEvent(SimEventType eventType, double time, uint32_t flowId,
                            uint32_t old_value, uint32_t new_value)
      : SimEvent(eventType, time, flowId), old_value(old_value), new_value(new_value)
  {
  }
  virtual EfmLBitCounterUpdateEvent* Clone() const override
  {
    return new EfmLBitCounterUpdateEvent(*this);
  }
  uint32_t old_value;
  uint32_t new_value;
};

struct EfmBitSetPCountEvent : SimEvent
{
  EfmBitSetPCountEvent(SimEventType eventType, double time, uint32_t flowId, uint32_t pkt_count,
                       uint32_t seq)
      : SimEvent(eventType, time, flowId), pkt_count(pkt_count), seq(seq)
  {
  }
  virtual EfmBitSetPCountEvent* Clone() const override { return new EfmBitSetPCountEvent(*this); }
  uint32_t pkt_count;
  uint32_t seq;
};

struct EfmBitSetEvent : SimEvent
{
  EfmBitSetEvent(SimEventType eventType, double time, uint32_t flowId, uint32_t seq)
      : SimEvent(eventType, time, flowId), seq(seq)
  {
  }
  virtual EfmBitSetEvent* Clone() const override { return new EfmBitSetEvent(*this); }
  uint32_t seq;
};

struct EfmRBitBlockLenUpdateEvent : SimEvent
{
  EfmRBitBlockLenUpdateEvent(SimEventType eventType, double time, uint32_t flowId,
                             uint32_t new_length)
      : SimEvent(eventType, time, flowId), new_length(new_length)
  {
  }
  virtual EfmRBitBlockLenUpdateEvent* Clone() const override
  {
    return new EfmRBitBlockLenUpdateEvent(*this);
  }
  uint32_t new_length;
};

struct EfmTBitHostPhaseUpdateEvent : SimEvent
{
  EfmTBitHostPhaseUpdateEvent(SimEventType eventType, double time, uint32_t flowId,
                              TBitClientPhase old_phase, TBitClientPhase new_phase)
      : SimEvent(eventType, time, flowId), old_phase(old_phase), new_phase(new_phase)
  {
  }
  virtual EfmTBitHostPhaseUpdateEvent* Clone() const override
  {
    return new EfmTBitHostPhaseUpdateEvent(*this);
  }
  TBitClientPhase old_phase;
  TBitClientPhase new_phase;
};

struct EfmTBitObserverPhaseUpdateEvent : SimEvent
{
  EfmTBitObserverPhaseUpdateEvent(SimEventType eventType, double time, uint32_t flowId,
                                  TBitObserverPhase old_phase, TBitObserverPhase new_phase)
      : SimEvent(eventType, time, flowId), old_phase(old_phase), new_phase(new_phase)
  {
  }
  EfmTBitObserverPhaseUpdateEvent(SimEventType eventType, double time, uint32_t flowId,
                                  TBitObserverPhase old_phase, TBitObserverPhase new_phase,
                                  uint32_t gen_train_length, uint32_t ref_train_length)
      : SimEvent(eventType, time, flowId),
        old_phase(old_phase),
        new_phase(new_phase),
        gen_train_length(gen_train_length),
        ref_train_length(ref_train_length)
  {
  }
  virtual EfmTBitObserverPhaseUpdateEvent* Clone() const override
  {
    return new EfmTBitObserverPhaseUpdateEvent(*this);
  }
  TBitObserverPhase old_phase;
  TBitObserverPhase new_phase;
  std::optional<uint32_t> gen_train_length;
  std::optional<uint32_t> ref_train_length;
};

struct EfmDelayMeasurementEvent : SimEvent
{
  EfmDelayMeasurementEvent(SimEventType eventType, double time, uint32_t flowId,
                           uint32_t full_delay_ms)
      : SimEvent(eventType, time, flowId), full_delay_ms(full_delay_ms)
  {
  }
  EfmDelayMeasurementEvent(SimEventType eventType, double time, uint32_t flowId,
                           uint32_t full_delay_ms, uint32_t half_delay_ms)
      : SimEvent(eventType, time, flowId),
        full_delay_ms(full_delay_ms),
        half_delay_ms(half_delay_ms)
  {
  }
  virtual EfmDelayMeasurementEvent* Clone() const override
  {
    return new EfmDelayMeasurementEvent(*this);
  }
  uint32_t full_delay_ms;
  std::optional<uint32_t> half_delay_ms;
};

struct EfmLossMeasurementEvent : SimEvent
{
  EfmLossMeasurementEvent(SimEventType eventType, double time, uint32_t flowId, uint32_t pkt_count,
                          uint32_t loss)
      : SimEvent(eventType, time, flowId), pkt_count(pkt_count), loss(loss)
  {
  }
  virtual EfmLossMeasurementEvent* Clone() const override
  {
    return new EfmLossMeasurementEvent(*this);
  }
  uint32_t pkt_count;
  uint32_t loss;
};

struct EfmSignedLossMeasurementEvent : SimEvent
{
  EfmSignedLossMeasurementEvent(SimEventType eventType, double time, uint32_t flowId,
                                uint32_t pkt_count, int32_t loss)
      : SimEvent(eventType, time, flowId), pkt_count(pkt_count), loss(loss)
  {
  }
  virtual EfmSignedLossMeasurementEvent* Clone() const override
  {
    return new EfmSignedLossMeasurementEvent(*this);
  }
  uint32_t pkt_count;
  int32_t loss;
};

std::shared_ptr<SimEvent> CreateEvent(simdjson::ondemand::object& event);

}  // namespace simdata


#endif  // SIM_EVENTS_H