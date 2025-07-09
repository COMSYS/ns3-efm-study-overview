#include "sim-events.h"

#include <iostream>

namespace simdata {

namespace {
SimEventType StringToEventType(std::string eventString)
{
  static const std::unordered_map<std::string, SimEventType> mapping = {
      {"efm_host:groundtruth_trans_delay", SimEventType::HOST_GT_TRANS_DELAY},
      {"efm_host:groundtruth_app_delay", SimEventType::HOST_GT_APP_DELAY},
      {"efm_host:spin_bit_update", SimEventType::HOST_SPIN_BIT_UDPATE},
      {"efm_host:l_bit_counter_update", SimEventType::HOST_L_BIT_COUNTER_UPDATE},
      {"efm_host:l_bit_set", SimEventType::HOST_L_BIT_SET},
      {"efm_host:q_bit_update", SimEventType::HOST_Q_BIT_UPDATE},
      {"efm_host:r_bit_update", SimEventType::HOST_R_BIT_UPDATE},
      {"efm_host:r_bit_block_update", SimEventType::HOST_R_BIT_BLOCK_UPDATE},
      {"efm_host:t_bit_set", SimEventType::HOST_T_BIT_SET},
      {"efm_host:t_bit_phase_update", SimEventType::HOST_T_BIT_PHASE_UPDATE},
      {"efm_observer:flow_begin", SimEventType::OBSV_FLOW_BEGIN},
      {"efm_observer:seq_loss", SimEventType::OBSV_SEQ_LOSS},
      {"efm_observer:ack_seq_loss", SimEventType::OBSV_ACK_SEQ_LOSS},
      {"efm_observer:spin_bit_edge", SimEventType::OBSV_SPIN_BIT_EDGE},
      {"efm_observer:spin_bit_delay", SimEventType::OBSV_SPIN_BIT_DELAY},
      {"efm_observer:l_bit_set", SimEventType::OBSV_L_BIT_SET},
      {"efm_observer:q_bit_change", SimEventType::OBSV_Q_BIT_CHANGE},
      {"efm_observer:q_bit_loss", SimEventType::OBSV_Q_BIT_LOSS},
      {"efm_observer:r_bit_change", SimEventType::OBSV_R_BIT_CHANGE},
      {"efm_observer:r_bit_loss", SimEventType::OBSV_R_BIT_LOSS},
      {"efm_observer:t_bit_set", SimEventType::OBSV_T_BIT_SET},
      {"efm_observer:t_bit_phase_update", SimEventType::OBSV_T_BIT_PHASE_UPDATE},
      {"efm_observer:t_bit_loss_full", SimEventType::OBSV_T_BIT_FULL_LOSS},
      {"efm_observer:t_bit_loss_half", SimEventType::OBSV_T_BIT_HALF_LOSS},
      {"efm_observer:p_l_bit_set", SimEventType::OBSV_P_L_BIT_SET},
      {"efm_observer:p_sq_bits_loss", SimEventType::OBSV_P_SQ_BITS_LOSS},
      {"efm_observer:tcp_dart_delay", SimEventType::OBSV_TCP_DART_DELAY},
      {"efm_observer:tcp_reordering", SimEventType::OBSV_TCP_REORDERING},
      {"unknown", SimEventType::UNKNOWN},
      {"ping:rt_delay", SimEventType::PING_RT_DELAY},
      {"ping:ete_delay", SimEventType::PING_ETE_DELAY},
      {"ping:rt_loss", SimEventType::PING_RT_LOSS},
      {"ping:ete_loss", SimEventType::PING_ETE_LOSS}};

  auto it = mapping.find(eventString);
  if (it != mapping.end())
    return it->second;

  return SimEventType::UNKNOWN;
}


// ----- Filter Functions -----

const std::array<SimEventType, 9> nonGroundTruthLossObserverEvents = {
    SimEventType::OBSV_L_BIT_SET,          SimEventType::OBSV_Q_BIT_CHANGE,
    SimEventType::OBSV_Q_BIT_LOSS,         SimEventType::OBSV_R_BIT_CHANGE,
    SimEventType::OBSV_R_BIT_LOSS,         SimEventType::OBSV_T_BIT_SET,
    SimEventType::OBSV_T_BIT_PHASE_UPDATE, SimEventType::OBSV_T_BIT_FULL_LOSS,
    SimEventType::OBSV_T_BIT_HALF_LOSS};

void FilterLBitTriggeredMonitoring(SimEventMap &simEventMap)
{
  auto lbitIt = simEventMap.find(SimEventType::OBSV_L_BIT_SET);

  if (lbitIt == simEventMap.end())
  {
    // Delete all non-groundtruth loss related observer events, since monitoring was never
    // triggered
    for (SimEventType simEvType : nonGroundTruthLossObserverEvents) simEventMap.erase(simEvType);
    return;
  }

  uint32_t packetCountOffset = 0;
  double monitorBeginTime = 0;

  auto lBitSetEvent = std::static_pointer_cast<EfmBitSetPCountEvent>(*lbitIt->second.begin());
  packetCountOffset = lBitSetEvent->pkt_count;
  monitorBeginTime = lBitSetEvent->time;

  packetCountOffset--;  // The first L bit set event counts as the first observed packet

  bool firstQMeasurementDiscarded = false;
  bool firstRMeasurementDiscarded = false;
  bool firstTMeasurementDiscarded = false;

  for (SimEventType simEvType : nonGroundTruthLossObserverEvents)
  {
    auto setIt = simEventMap.find(simEvType);
    if (setIt == simEventMap.end())
      continue;
    SimEventSet &simEventSet = setIt->second;

    std::vector<EventPointer> modifiedEvents;

    for (auto it = simEventSet.begin(); it != simEventSet.end();)
    {
      // Delete all loss-related observer events before the first L bit set event
      // Except for the groundtruth related events
      if ((*it)->time < monitorBeginTime)
      {
        it = simEventSet.erase(it);
        continue;
      }

      switch (simEvType)
      {
        case SimEventType::OBSV_L_BIT_SET:
        {
          auto modifiedEvent = std::make_shared<EfmBitSetPCountEvent>(
              *std::static_pointer_cast<EfmBitSetPCountEvent>(*it));
          modifiedEvent->pkt_count -= packetCountOffset;
          modifiedEvents.push_back(modifiedEvent);
          it = simEventSet.erase(it);
          continue;
        }
        case SimEventType::OBSV_Q_BIT_LOSS:
          if (!firstQMeasurementDiscarded)
          {
            firstQMeasurementDiscarded = true;
            it = simEventSet.erase(it);
            continue;
          }
          else
          {
            auto modifiedEvent = std::make_shared<EfmLossMeasurementEvent>(
                *std::static_pointer_cast<EfmLossMeasurementEvent>(*it));
            modifiedEvent->pkt_count -= packetCountOffset;
            modifiedEvents.push_back(modifiedEvent);
            it = simEventSet.erase(it);
            continue;
          }
          break;
        case SimEventType::OBSV_R_BIT_LOSS:
          if (!firstRMeasurementDiscarded)
          {
            firstRMeasurementDiscarded = true;
            it = simEventSet.erase(it);
            continue;
          }
          else
          {
            auto modifiedEvent = std::make_shared<EfmLossMeasurementEvent>(
                *std::static_pointer_cast<EfmLossMeasurementEvent>(*it));
            modifiedEvent->pkt_count -= packetCountOffset;
            modifiedEvents.push_back(modifiedEvent);
            it = simEventSet.erase(it);
            continue;
          }
          break;
        case SimEventType::OBSV_SEQ_LOSS:
        case SimEventType::OBSV_ACK_SEQ_LOSS:
          break;  // We want to keep the groundtruth independent
        case SimEventType::OBSV_T_BIT_FULL_LOSS:
        case SimEventType::OBSV_T_BIT_HALF_LOSS:
          if (!firstTMeasurementDiscarded)
          {
            firstTMeasurementDiscarded = true;
            it = simEventSet.erase(it);
            continue;
          }
          else
          {
            auto modifiedEvent = std::make_shared<EfmLossMeasurementEvent>(
                *std::static_pointer_cast<EfmLossMeasurementEvent>(*it));
            modifiedEvent->pkt_count -= packetCountOffset;
            modifiedEvents.push_back(modifiedEvent);
            it = simEventSet.erase(it);
            continue;
          }
        default:
          break;
      }
      it++;  // Increment iterator here to allow continue after updating it via erasing
    }
    for (auto &ev : modifiedEvents) simEventSet.insert(ev);
  }
}

void FilterLastSpinTransients(SimEventMap &eventMap, uint32_t transientCount)
{
  auto it = eventMap.find(SimEventType::OBSV_SPIN_BIT_DELAY);

  if (it != eventMap.end())
  {
    // Remove the last transientCount spin bit delay events
    auto &eventSet = it->second;
    if (eventSet.size() < transientCount)
      eventSet.clear();
    else
      eventSet.erase(std::prev(eventSet.end(), transientCount), eventSet.end());
  }

  auto it2 = eventMap.find(SimEventType::OBSV_SPIN_BIT_EDGE);
  if (it2 != eventMap.end())
  {
    // Remove the last transientCount spin bit delay events
    auto &eventSet = it2->second;
    if (eventSet.size() < transientCount)
      eventSet.clear();
    else
      eventSet.erase(std::prev(eventSet.end(), transientCount), eventSet.end());
  }
}

}  // namespace

std::string EventTypeToString(const SimEventType &eventType)
{
  // Enum to string for all event types
  switch (eventType)
  {
    case SimEventType::HOST_GT_TRANS_DELAY:
      return "HOST_GT_TRANS_DELAY";
    case SimEventType::HOST_GT_APP_DELAY:
      return "HOST_GT_APP_DELAY";
    case SimEventType::HOST_SPIN_BIT_UDPATE:
      return "HOST_SPIN_BIT_UDPATE";
    case SimEventType::HOST_L_BIT_COUNTER_UPDATE:
      return "HOST_L_BIT_COUNTER_UPDATE";
    case SimEventType::HOST_L_BIT_SET:
      return "HOST_L_BIT_SET";
    case SimEventType::HOST_Q_BIT_UPDATE:
      return "HOST_Q_BIT_UPDATE";
    case SimEventType::HOST_R_BIT_UPDATE:
      return "HOST_R_BIT_UPDATE";
    case SimEventType::HOST_R_BIT_BLOCK_UPDATE:
      return "HOST_R_BIT_BLOCK_UPDATE";
    case SimEventType::HOST_T_BIT_SET:
      return "HOST_T_BIT_SET";
    case SimEventType::HOST_T_BIT_PHASE_UPDATE:
      return "HOST_T_BIT_PHASE_UPDATE";
    case SimEventType::OBSV_FLOW_BEGIN:
      return "OBSV_FLOW_BEGIN";
    case SimEventType::OBSV_SEQ_LOSS:
      return "OBSV_SEQ_LOSS";
    case SimEventType::OBSV_ACK_SEQ_LOSS:
      return "OBSV_ACK_SEQ_LOSS";
    case SimEventType::OBSV_SPIN_BIT_EDGE:
      return "OBSV_SPIN_BIT_EDGE";
    case SimEventType::OBSV_SPIN_BIT_DELAY:
      return "OBSV_SPIN_BIT_DELAY";
    case SimEventType::OBSV_L_BIT_SET:
      return "OBSV_L_BIT_SET";
    case SimEventType::OBSV_Q_BIT_CHANGE:
      return "OBSV_Q_BIT_CHANGE";
    case SimEventType::OBSV_Q_BIT_LOSS:
      return "OBSV_Q_BIT_LOSS";
    case SimEventType::OBSV_R_BIT_CHANGE:
      return "OBSV_R_BIT_CHANGE";
    case SimEventType::OBSV_R_BIT_LOSS:
      return "OBSV_R_BIT_LOSS";
    case SimEventType::OBSV_T_BIT_SET:
      return "OBSV_T_BIT_SET";
    case SimEventType::OBSV_T_BIT_PHASE_UPDATE:
      return "OBSV_T_BIT_PHASE_UPDATE";
    case SimEventType::OBSV_T_BIT_FULL_LOSS:
      return "OBSV_T_BIT_FULL_LOSS";
    case SimEventType::OBSV_T_BIT_HALF_LOSS:
      return "OBSV_T_BIT_HALF_LOSS";
    case SimEventType::OBSV_P_L_BIT_SET:
      return "OBSV_P_L_BIT_SET";
    case SimEventType::OBSV_P_SQ_BITS_LOSS:
      return "OBSV_P_SQ_BITS_LOSS";
    case SimEventType::OBSV_TCP_DART_DELAY:
      return "OBSV_TCP_DART_DELAY";
    case SimEventType::OBSV_TCP_REORDERING:
      return "OBSV_TCP_REORDERING";
    case SimEventType::PING_RT_LOSS:
      return "PING_RT_LOSS";
    case SimEventType::PING_RT_DELAY:
      return "PING_RT_DELAY";
    case SimEventType::PING_ETE_DELAY:
      return "PING_ETE_DELAY";
    case SimEventType::PING_ETE_LOSS:
      return "PING_ETE_LOSS";
    case SimEventType::UNKNOWN:
      return "UNKNOWN";
    default:
      return "NOT IMPLEMENTED";
  }
}

TBitObserverPhase TBitObserverPhaseFromString(std::string phaseString)
{
  if (phaseString == "gen")
    return TBitObserverPhase::GEN;
  else if (phaseString == "gen_pause_begin")
    return TBitObserverPhase::PAUSE_BEGIN_GEN;
  else if (phaseString == "gen_pause_full")
    return TBitObserverPhase::PAUSE_FULL_GEN;
  else if (phaseString == "ref")
    return TBitObserverPhase::REF;
  else if (phaseString == "ref_pause_begin")
    return TBitObserverPhase::PAUSE_BEGIN_REF;
  else if (phaseString == "ref_pause_full")
    return TBitObserverPhase::PAUSE_FULL_REF;
  else
    return TBitObserverPhase::OP_ERROR;
}


TBitClientPhase TBitClientPhaseFromString(std::string phaseString)
{
  if (phaseString == "gen1")
    return TBitClientPhase::GEN1;
  else if (phaseString == "gen2")
    return TBitClientPhase::GEN2;
  else if (phaseString == "gen_pause")
    return TBitClientPhase::PAUSE_GEN;
  else if (phaseString == "ref1")
    return TBitClientPhase::REF1;
  else if (phaseString == "ref2")
    return TBitClientPhase::REF2;
  else if (phaseString == "ref_pause")
    return TBitClientPhase::PAUSE_REF;
  else
    return TBitClientPhase::CP_ERROR;
}


std::shared_ptr<SimEvent> CreateEvent(simdjson::ondemand::object &event)
{
  using namespace simdjson;
  SimEventType evType = StringToEventType(std::string(event["name"].get_string().value()));

  double evTime = event["time"].get_double();
  uint32_t evFlowId = event["group_id"]["flow_id"].get_uint64();
  ondemand::object evData = event["data"];

  switch (evType)
  {
    case SimEventType::OBSV_FLOW_BEGIN:
      return std::make_shared<SimEvent>(evType, evTime, evFlowId);
      break;
    case SimEventType::HOST_SPIN_BIT_UDPATE:
    case SimEventType::HOST_Q_BIT_UPDATE:
    case SimEventType::HOST_R_BIT_UPDATE:
    case SimEventType::OBSV_SPIN_BIT_EDGE:
    case SimEventType::OBSV_Q_BIT_CHANGE:
    case SimEventType::OBSV_R_BIT_CHANGE:
      return std::make_shared<EfmBitUpdateEvent>(
          evType, evTime, evFlowId, evData["new_state"].get_bool(), evData["seq"].get_uint64());
      break;
    case SimEventType::HOST_L_BIT_SET:
    case SimEventType::HOST_T_BIT_SET:
    case SimEventType::OBSV_T_BIT_SET:
      return std::make_shared<EfmBitSetEvent>(evType, evTime, evFlowId, evData["seq"].get_uint64());
      break;
    case SimEventType::OBSV_L_BIT_SET:
    case SimEventType::OBSV_P_L_BIT_SET:
      return std::make_shared<EfmBitSetPCountEvent>(
          evType, evTime, evFlowId, evData["pkt_count"].get_uint64(), evData["seq"].get_uint64());
      break;
    case SimEventType::HOST_L_BIT_COUNTER_UPDATE:
      return std::make_shared<EfmLBitCounterUpdateEvent>(evType, evTime, evFlowId,
                                                         evData["old_value"].get_uint64(),
                                                         evData["new_value"].get_uint64());
      break;
    case SimEventType::HOST_R_BIT_BLOCK_UPDATE:
      return std::make_shared<EfmRBitBlockLenUpdateEvent>(evType, evTime, evFlowId,
                                                          evData["new_length"].get_uint64());
      break;
    case SimEventType::HOST_T_BIT_PHASE_UPDATE:
      return std::make_shared<EfmTBitHostPhaseUpdateEvent>(
          evType, evTime, evFlowId,
          TBitClientPhaseFromString(std::string(evData["old_phase"].get_string().value())),
          TBitClientPhaseFromString(std::string(evData["new_phase"].get_string().value())));
      break;
    case SimEventType::OBSV_SPIN_BIT_DELAY:
    case SimEventType::HOST_GT_TRANS_DELAY:
    case SimEventType::HOST_GT_APP_DELAY:
    case SimEventType::OBSV_TCP_DART_DELAY:
    case SimEventType::PING_ETE_DELAY:
    case SimEventType::PING_RT_DELAY:
    {
      uint64_t halfDelay;
      if (evData["half_delay_ms"].get(
              halfDelay))  // Evaluates to true on error, i.e., "half_delay_ms" not found
      {
        return std::make_shared<EfmDelayMeasurementEvent>(evType, evTime, evFlowId,
                                                          evData["full_delay_ms"].get_uint64());
      }
      else
      {
        return std::make_shared<EfmDelayMeasurementEvent>(
            evType, evTime, evFlowId, evData["full_delay_ms"].get_uint64(), halfDelay);
      }
      break;
    }

    case SimEventType::OBSV_Q_BIT_LOSS:
    case SimEventType::OBSV_R_BIT_LOSS:
    case SimEventType::OBSV_SEQ_LOSS:
    case SimEventType::OBSV_ACK_SEQ_LOSS:
    case SimEventType::OBSV_T_BIT_FULL_LOSS:
    case SimEventType::OBSV_T_BIT_HALF_LOSS:
    case SimEventType::OBSV_TCP_REORDERING:
    case SimEventType::PING_RT_LOSS:
    case SimEventType::PING_ETE_LOSS:
      return std::make_shared<EfmLossMeasurementEvent>(
          evType, evTime, evFlowId, evData["pkt_count"].get_uint64(), evData["loss"].get_uint64());
      break;
    case SimEventType::OBSV_T_BIT_PHASE_UPDATE:
    {
      auto evPtr = std::make_shared<EfmTBitObserverPhaseUpdateEvent>(
          evType, evTime, evFlowId,
          TBitObserverPhaseFromString(std::string(evData["old_phase"].get_string().value())),
          TBitObserverPhaseFromString(std::string(evData["new_phase"].get_string().value())));
      uint64_t genTrainLength;
      if (!evData["gen_train_length"].get(genTrainLength))  // get returns false => no error
        evPtr->gen_train_length = genTrainLength;
      uint64_t refTrainLength;
      if (!evData["ref_train_length"].get(refTrainLength))  // get returns false => no error
        evPtr->ref_train_length = refTrainLength;
      return evPtr;
      break;
    }
    case SimEventType::OBSV_P_SQ_BITS_LOSS:
      return std::make_shared<EfmSignedLossMeasurementEvent>(
          evType, evTime, evFlowId, evData["pkt_count"].get_uint64(), evData["loss"].get_int64());
    case SimEventType::UNKNOWN:
    default:
      return std::make_shared<SimEvent>(evType, evTime, evFlowId);
      break;
  }
}


void FilterSimEventSet(SimEventMap &eventMap, const SimFilter &filter, bool obsvEvents /*=true*/,
                       bool hostEvents /*=true*/)
{
  if (obsvEvents)
  {
    if (filter.lBitTriggeredMonitoring)
    {
      FilterLBitTriggeredMonitoring(eventMap);
    }

    if (filter.removeLastXSpinTransients > 0)
      FilterLastSpinTransients(eventMap, filter.removeLastXSpinTransients);
  }
}

bool SimEvent::IsPathEvent() const
{
  return eventType == SimEventType::OBSV_P_L_BIT_SET ||
         eventType == SimEventType::OBSV_P_SQ_BITS_LOSS;
}

bool SimEvent::IsPingClientEvent() const
{
  return eventType == SimEventType::PING_RT_LOSS || eventType == SimEventType::PING_RT_DELAY;
}

bool SimEvent::IsPingServerEvent() const
{
  return eventType == SimEventType::PING_ETE_LOSS || eventType == SimEventType::PING_ETE_DELAY;
}

}  // namespace simdata
