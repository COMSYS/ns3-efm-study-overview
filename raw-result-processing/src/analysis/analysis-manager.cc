#include "analysis-manager.h"

#include <iostream>
namespace analysis {

// ################################
// ####### Helper functions #######
// ################################

namespace {

bool TryGetFlowResultForResultType(const simdata::SimObserverFlow& flow, ResultType resType,
                                   double& result, double time_filter)
{
  switch (resType)
  {
    case ResultType::SEQ_REL_LOSS:
      result = flow.GetRelativeSeqLoss();
      break;
    case ResultType::SEQ_ABS_LOSS:
      result = flow.GetAbsoluteSeqLoss();
      break;
    case ResultType::ACK_SEQ_REL_LOSS:
      result = flow.GetRelativeAckSeqLoss();
      break;
    case ResultType::ACK_SEQ_ABS_LOSS:
      result = flow.GetAbsoluteAckSeqLoss();
      break;
    case ResultType::Q_REL_LOSS:
      result = flow.GetRelativeQBitLoss();
      break;
    case ResultType::R_REL_LOSS:
      result = flow.GetRelativeRBitLoss();
      break;
    case ResultType::T_REL_FULL_LOSS:
      result = flow.GetRelativeTBitFullLoss();
      break;
    case ResultType::T_REL_HALF_LOSS:
      result = flow.GetRelativeTBitHalfLoss();
      break;
    case ResultType::L_REL_LOSS:
      result = flow.GetRelativeLBitLoss();
      break;
    case ResultType::Q_ABS_LOSS:
      result = flow.GetAbsoluteQBitLoss();
      break;
    case ResultType::R_ABS_LOSS:
      result = flow.GetAbsoluteRBitLoss();
      break;
    case ResultType::T_ABS_FULL_LOSS:
      result = flow.GetAbsoluteTBitFullLoss();
      break;
    case ResultType::T_ABS_HALF_LOSS:
      result = flow.GetAbsoluteTBitHalfLoss();
      break;
    case ResultType::L_ABS_LOSS:
      result = flow.GetAbsoluteLBitLoss();
      break;
    case ResultType::SPIN_AVG_DELAY:
    {
      auto spinDelay = flow.GetAvgSpinRTDelay(time_filter);
      if (spinDelay.has_value())
        result = spinDelay.value();
      else
        return false;
      break;
    }
    case ResultType::TCPDART_AVG_DELAY:
    {
      auto tcpDelay = flow.GetAvgTcpHRTDelay();
      if (tcpDelay.has_value())
        result = tcpDelay.value();
      else
        return false;
      break;
    }
    case ResultType::TCPRO_ABS_LOSS:
      result = flow.GetAbsoluteTcpReordering();
      break;
    case ResultType::TCPRO_REL_LOSS:
      result = flow.GetRelativeTcpReordering();
      break;
    default:
      throw std::runtime_error(
          "TryGetFlowResultForResultType: Unknown or incompatible result type");
  }
  return true;
}

bool TryGetFlowResultForResultTypeRawValues(const simdata::SimObserverFlow& flow, ResultType resType,
                                            std::list<double>& result, double time_filter)
{
  switch (resType)
  {
    case ResultType::SPIN_DELAY_RAW:
    {
      auto spinDelay = flow.GetRawSpinRTValues(time_filter);
      if (spinDelay.has_value())
        result = spinDelay.value();
      else
        return false;
      break;
    }
    case ResultType::TCPDART_DELAY_RAW:
    {
      auto tcpDelay = flow.GetRawTcpHRTValues();
      if (tcpDelay.has_value())
        result = tcpDelay.value();
      else
        return false;
      break;
    }
    default:
      throw std::runtime_error(
          "TryGetFlowResultForResultTypeRawValues: Unknown or incompatible result type");
  }
  return true;
}


bool TryGetPathResultForResultType(const simdata::SimPath& path, ResultType resType, double& result)
{
  switch (resType)
  {
    case ResultType::SQ_REL_LOSS:
      result = path.GetRelativeFinalSQBitsLoss();
      break;
    case ResultType::SQ_ABS_LOSS:
      result = path.GetAbsoluteFinalSQBitsLoss();
      break;
    case ResultType::L_ABS_LOSS:
      result = path.GetAbsoluteLBitLoss();
      break;
    case ResultType::L_REL_LOSS:
      result = path.GetRelativeLBitLoss();
      break;
    default:
      throw std::runtime_error(
          "TryGetPathResultForResultType: Unknown or incompatible result type");
  }
  return true;
}

bool TryGetPingResultForResultType(const simdata::SimPingPair& pp, ResultType resType,
                                   double& result)
{
  switch (resType)
  {
    case ResultType::PING_CLNT_ABS_LOSS:
    case ResultType::PING_SVR_ABS_LOSS:
      result = pp.GetAbsoluteLoss();
      break;
    case ResultType::PING_CLNT_REL_LOSS:
    case ResultType::PING_SVR_REL_LOSS:
      result = pp.GetRelativeLoss();
      break;
    case ResultType::PING_CLNT_AVG_DELAY:
    case ResultType::PING_SVR_AVG_DELAY:
    {
      auto avgDelay = pp.GetAvgDelay();
      if (avgDelay.has_value())
        result = avgDelay.value();
      else
        return false;
      break;
    }
    default:
      throw std::runtime_error(
          "TryGetPingResultForResultType: Unknown or incompatible result type");
  }
  return true;
}


bool TryGetPingResultForResultTypeRawValues(const simdata::SimPingPair& pp, ResultType resType, std::list<double>& result)
{
  switch (resType)
  {
    case ResultType::PING_CLNT_DELAY_RAW:
    {
      auto rawDelayValues = pp.GetRawPingDelayValues();
      if (rawDelayValues.has_value())
        result = rawDelayValues.value();
      else
        return false;
      break;
    }
    case ResultType::PING_SVR_DELAY_RAW:
    {
      auto rawDelayValues = pp.GetRawPingDelayValues();
      if (rawDelayValues.has_value())
        result = rawDelayValues.value();
      else
        return false;
      break;
    }
    default:
      throw std::runtime_error(
          "TryGetPingResultForResultType: Unknown or incompatible result type");
  }
  return true;
}



double CalculateLossThreshold(const simdata::SimResultSet& simResultSet, double offset)
{
  // Set initial value that never occurs
  double lossRateTh = 2;

  for (auto it : simResultSet.GetFailedLinks())
  {
    if (it.second.lossRate > 0 && it.second.lossRate < lossRateTh)
      lossRateTh = it.second.lossRate;
  }

  // If no failed links, set thresholds to 0
  if (lossRateTh == 2)
    lossRateTh = 0;

  // Add offset
  return std::max(lossRateTh + offset, 0.0);
}

uint32_t CalculateDelayThreshold(const simdata::SimResultSet& simResultSet, int32_t offset)
{
  // Set initial value that never occurs
  uint32_t delayTh = UINT32_MAX;

  for (auto it : simResultSet.GetFailedLinks())
  {
    if (it.second.delayMs > 0 && it.second.delayMs < delayTh)
      delayTh = it.second.delayMs;
  }

  // If no failed links, set threshold to 0
  if (delayTh == UINT32_MAX)
    delayTh = 0;

  // Add offset
  if (offset < 0 && delayTh < (uint32_t)(-offset))
    return 0;
  else
    return delayTh + offset;
}

void PrepareConfig(AnalysisConfig& analysisConfig, const simdata::SimResultSet& simResultSet)
{
  // An empty observer set means that all observers should be used
  for (auto it = analysisConfig.observerSets.begin(); it != analysisConfig.observerSets.end(); it++)
  {
    if (it->observers.empty())
    {
      it->observers = simResultSet.GetObserverVPIds(true, true);
    }
  }
}

void GetConfigThresholds(const AnalysisConfig& analysisConfig,
                         const simdata::SimResultSet& simResultSet, uint32_t& delayThMs,
                         double& lossRateTh)
{
  if (analysisConfig.lossRateTh.has_value())
    lossRateTh = analysisConfig.lossRateTh.value();
  else
    lossRateTh = CalculateLossThreshold(simResultSet, *analysisConfig.autoLossRateThOffset);

  if (analysisConfig.delayThMs.has_value())
    delayThMs = analysisConfig.delayThMs.value();
  else
    delayThMs = CalculateDelayThreshold(simResultSet, *analysisConfig.autoDelayThOffsetMs);
}

}  // namespace
// #######################################
// ####### End of helper functions #######
// #######################################


void AnalysisManager::RunAnalyses(simdata::SimResultSetPointer simResultSet,
                                  const std::string& outputFile,
                                  std::vector<AnalysisConfig> analysisConfigs)
{
  OutputGenerator outGen(simResultSet, outputFile);
  bool storedMeasurements = false;
  for (auto& analysisConfig : analysisConfigs)
  {
    // Do not store measurements multiple times
    if (storedMeasurements)
      analysisConfig.storeMeasurements = false;
    else if (analysisConfig.storeMeasurements)
      storedMeasurements = true;
    DoRunAnalysis(simResultSet, outGen, analysisConfig);
  }

  outGen.GenerateOutput();
}

void AnalysisManager::RunAnalysis(simdata::SimResultSetPointer simResultSet,
                                  const std::string& outputFile, AnalysisConfig analysisConfig)
{
  OutputGenerator outGen(simResultSet, outputFile);
  DoRunAnalysis(simResultSet, outGen, analysisConfig);
  outGen.GenerateOutput();
}

void AnalysisManager::DoRunAnalysis(simdata::SimResultSetPointer simResultSet,
                                    OutputGenerator& outGen, AnalysisConfig& analysisConfig)
{
  // Store measurement results for each flow and path per observer
  if (analysisConfig.storeMeasurements)
  {
    for (uint32_t obsvId : simResultSet->GetObserverVPIds(true, true))
    {
      auto obsv = simResultSet->GetObserverVP(obsvId);
      for (uint32_t flowId : obsv->GetFlowIds())
      {
        auto flow = obsv->GetFlow(flowId);
        // These are summarized values
        for (ResultType resType : FLOW_RESULT_TYPES)
        {
          // Maybe use bulk add here with temporary map instead?
          double result;
          if (TryGetFlowResultForResultType(*flow, resType, result, analysisConfig.time_filter_ms))
            outGen.AddObserverFlowResult(obsvId, flowId, resType, result);
        }
        // This is an attempt at lists of values
        if (analysisConfig.output_raw_values){
            for (ResultType resType : FLOW_RESULT_TYPES_RAW_VALUES)
            {
                std::list<double> result;
                if (TryGetFlowResultForResultTypeRawValues(*flow, resType, result, analysisConfig.time_filter_ms)){
                    outGen.AddObserverFlowResultList(obsvId, flowId, resType, result);
                }
            }
        }
      }
      for (uint32_t pathId : obsv->GetPathIds())
      {
        auto path = obsv->GetPath(pathId);
        for (ResultType resType : PATH_RESULT_TYPES)
        {
          // Maybe use bulk add here with temporary map instead?
          double result;
          if (TryGetPathResultForResultType(*path, resType, result))
            outGen.AddObserverPathResult(obsvId, pathId, resType, result);
        }
      }

      for (auto& [targetId, pp] : obsv->GetClientPingPairs())
      {
        for (ResultType resType : PING_CLIENT_RESULT_TYPES)
        {
          double result;
          if (TryGetPingResultForResultType(*pp, resType, result))
            outGen.AddObserverActiveResult(obsvId, targetId, resType, result);
        }

        if (analysisConfig.output_raw_values){
            for (ResultType resType : PING_CLIENT_RESULT_TYPES_RAW)
            {
                std::list<double> result;
                if (TryGetPingResultForResultTypeRawValues(*pp, resType, result)){
                    outGen.AddObserverActiveResultList(obsvId, targetId, resType, result);
                }
            }
        }
      }

      for (auto& [targetId, pp] : obsv->GetServerPingPairs())
      {
        for (ResultType resType : PING_SERVER_RESULT_TYPES)
        {
          double result;
          if (TryGetPingResultForResultType(*pp, resType, result))
            outGen.AddObserverActiveResult(obsvId, targetId, resType, result);
        }

        if (analysisConfig.output_raw_values){
            for (ResultType resType : PING_SERVER_RESULT_TYPES_RAW)
            {
                std::list<double> result;
                if (TryGetPingResultForResultTypeRawValues(*pp, resType, result)){
                    outGen.AddObserverActiveResultList(obsvId, targetId, resType, result);
                }
            }
        }
      }
    }
  }

  if (analysisConfig.performLocalization)
  {
    PrepareConfig(analysisConfig, *simResultSet);
    auto filteredSrs = simResultSet->ApplyFilter(analysisConfig.simFilter);

    uint32_t delayThMs;
    double lossRateTh;
    GetConfigThresholds(analysisConfig, *simResultSet, delayThMs, lossRateTh);

    for (const auto& mode : analysisConfig.classificationModes)
    {
        for (const auto& selectionStrategy : analysisConfig.flowSelectionStrategies)
        {
            auto result = FailureLocalization::LocalizeFailures(
                *filteredSrs, analysisConfig.observerSets, analysisConfig.efmBitSets, lossRateTh,
                delayThMs, analysisConfig.flowLengthTh, mode, analysisConfig.localizationMethods, analysisConfig.classification_base_id, analysisConfig.time_filter_ms, (FlowSelectionStrategyWithParams){selectionStrategy.first, selectionStrategy.second});
            for (auto& [classConf, locResults] : result)
            {
                outGen.AddLocalizationResults(analysisConfig.simFilter, classConf, locResults, (FlowSelectionStrategyWithParams){selectionStrategy.first, selectionStrategy.second});
            }
        }
        
    }
  }
}
}  // namespace analysis
