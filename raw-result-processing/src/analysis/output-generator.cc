#include "output-generator.h"
using json = nlohmann::json;

#include <fstream>

namespace analysis {


void OutputGenerator::GenerateOutput(bool pretty /*= false*/)
{
  json outputJson;

  outputJson["simId"] = m_simResultSet->GetSimId();
  outputJson["config"] = json::parse(m_simResultSet->GetSimConfigJson());

  StoreFlowPathMap(outputJson);

  StoreFailedLinks(outputJson);

  StoreBackboneLinkOverrides(outputJson);

  StoreLinkSets(outputJson);

  StoreGroundtruthStats(outputJson);

  StoreObserverFlowResults(outputJson);

  StoreObserverFlowResultsRawValues(outputJson);

  StoreObserverPathResults(outputJson);

  StoreObserverActiveResults(outputJson);

  StoreObserverActiveResultsRawValues(outputJson);

  StoreLocalizationResults(outputJson);

  CreatePathAndDump(outputJson, pretty);
}

void OutputGenerator::AddObserverFlowResult(uint32_t observerId, uint32_t flowId,
                                            ResultType resultType, double resultValue)
{
  m_observerFlowResults[observerId][flowId][resultType] = resultValue;
}

void OutputGenerator::AddObserverFlowResultBulk(uint32_t observerId, uint32_t flowId,
                                                std::map<ResultType, double> resultValues)
{
  m_observerFlowResults[observerId][flowId] = resultValues;
}

void OutputGenerator::AddObserverFlowResultList(uint32_t observerId, uint32_t flowId,
                                            ResultType resultType, std::list<double> resultList)
{
  m_observerFlowResultsRawValues[observerId][flowId][resultType] = resultList;
}

void OutputGenerator::AddObserverPathResult(uint32_t observerId, uint32_t pathId,
                                            ResultType resultType, double resultValue)
{
  m_observerPathResults[observerId][pathId][resultType] = resultValue;
}

void OutputGenerator::AddObserverActiveResult(uint32_t observerId, uint32_t targetId,
                                              ResultType resultType, double resultValue)
{
  m_observerActiveResults[observerId][targetId][resultType] = resultValue;
}

void OutputGenerator::AddObserverActiveResultList(uint32_t observerId, uint32_t targetId,
                                              ResultType resultType, std::list<double> resultList)
{
  m_observerActiveResultsRawValues[observerId][targetId][resultType] = resultList;
}

void OutputGenerator::AddLocalizationResults(simdata::SimFilter& filter,
                                             ClassificationConfig& clfcConfig,
                                             std::vector<LocalizationResult>& result,
                                             FlowSelectionStrategyWithParams flowSelectionStrategy)
{
  m_localizationResults.emplace_back(filter, clfcConfig, result, flowSelectionStrategy);
}

void OutputGenerator::CreatePathAndDump(const json& outputJson, bool pretty)
{
  std::filesystem::path filePath = m_outputFile;
  if (!filePath.has_filename())
    throw std::runtime_error("Output file path must include a file name");

  std::filesystem::create_directories(filePath.parent_path());

  std::ofstream os(filePath, std::ios::out | std::ios::binary);

  if (!os.is_open())
    throw std::runtime_error("Could not open output file");

  os << outputJson.dump(pretty ? 2 : -1);

  os.close();
}

void OutputGenerator::StoreFlowPathMap(json& outputJson)
{
  json flowPathMap;
  for (auto flowInfo : m_simResultSet->GetObserverFlowInfo())
  {
    std::vector<simdata::ObsvVantagePointPointer> path =
        m_simResultSet->CalculateFlowPath(flowInfo.first);
    std::vector<uint32_t> pathIds;
    for (auto it = path.begin(); it != path.end(); it++)
    {
      pathIds.push_back((*it)->m_nodeId);
    }
    flowPathMap[flowInfo.second.Serialize()] = pathIds;
  }
  outputJson["flowPathMap"] = flowPathMap;
}

void OutputGenerator::StoreFailedLinks(json& outputJson)
{
  std::vector<simdata::FailedLink> failedLinks;
  for (auto failedLink : m_simResultSet->GetFailedLinks())
  {
    failedLinks.push_back(failedLink.second);
  }
  outputJson["failedLinks"] = failedLinks;
}

void OutputGenerator::StoreBackboneLinkOverrides(json& outputJson)
{
  std::vector<simdata::LinkConfig> overrides;
  for (auto linkOv : m_simResultSet->GetBackboneOverrides())
  {
    overrides.push_back(linkOv.second);
  }
  outputJson["backboneOverrides"] = overrides;
}

void OutputGenerator::StoreLinkSets(json& outputJson)
{
  outputJson["allLinks"] = m_simResultSet->GetAllLinks();
  outputJson["edgeLinks"] = m_simResultSet->GetEdgeLinks();
  outputJson["coreLinks"] = m_simResultSet->GetCoreLinks();
}

void OutputGenerator::StoreObserverFlowResults(json& outputJson)
{
  auto flowInfoMap = m_simResultSet->GetObserverFlowInfo();
  json observerFlowResults;
  // Iterate over observerId / (flowId / (result type / value)) pairs
  for (auto observerFlowResult : m_observerFlowResults)
  {
    json flowResults;
    // Iterate over flowId / (result type / value) pairs
    for (auto flowResult : observerFlowResult.second)
    {
      json resultValues;
      // Iterate over result type / value pairs
      for (auto resultValue : flowResult.second)
      {
        resultValues[ResultTypeToString(resultValue.first)] = resultValue.second;
      }
      flowResults[flowInfoMap[flowResult.first].Serialize()] = resultValues;
    }
    observerFlowResults[std::to_string(observerFlowResult.first)] = flowResults;
  }
  outputJson["observerFlowResults"] = observerFlowResults;
}


void OutputGenerator::StoreObserverFlowResultsRawValues(json& outputJson)
{  
  auto flowInfoMap = m_simResultSet->GetObserverFlowInfo();
  json observerFlowResults;
  // Iterate over observerId / (flowId / (result type / value)) pairs
  for (auto observerFlowResultRawValues : m_observerFlowResultsRawValues)
  {
    json flowResults;
    // Iterate over flowId / (result type / value) pairs
    for (auto flowResult : observerFlowResultRawValues.second)
    {
      json resultValues;
      // Iterate over result type / value pairs
      for (auto resultValue : flowResult.second)
      {
        resultValues[ResultTypeToString(resultValue.first)] = resultValue.second;
      }
      flowResults[flowInfoMap[flowResult.first].Serialize()] = resultValues;
    }
    observerFlowResults[std::to_string(observerFlowResultRawValues.first)] = flowResults;
  }
  outputJson["observerFlowResultsRawValues"] = observerFlowResults;
}

void OutputGenerator::StoreObserverPathResults(json& outputJson)
{
  auto pathInfoMap = m_simResultSet->GetObserverPathInfo();
  json observerPathResults;
  // Iterate over observerId / (pathId / (result type / value)) pairs
  for (auto observerPathResult : m_observerPathResults)
  {
    json pathResults;
    // Iterate over pathId / (result type / value) pairs
    for (auto pathResult : observerPathResult.second)
    {
      json resultValues;
      // Iterate over result type / value pairs
      for (auto resultValue : pathResult.second)
      {
        resultValues[ResultTypeToString(resultValue.first)] = resultValue.second;
      }
      pathResults[pathInfoMap[pathResult.first].Serialize()] = resultValues;
    }
    observerPathResults[std::to_string(observerPathResult.first)] = pathResults;
  }
  outputJson["observerPathResults"] = observerPathResults;
}

void OutputGenerator::StoreObserverActiveResults(json& outputJson)
{
  json observerActiveResults;
  // Iterate over observerId / (targetId / (result type / value)) pairs
  for (auto observerActiveResult : m_observerActiveResults)
  {
    json activeResults;
    // Iterate over targetId / (result type / value) pairs
    for (auto activeResult : observerActiveResult.second)
    {
      json resultValues;
      // Iterate over result type / value pairs
      for (auto resultValue : activeResult.second)
      {
        resultValues[ResultTypeToString(resultValue.first)] = resultValue.second;
      }
      activeResults[std::to_string(activeResult.first)] = resultValues;
    }
    observerActiveResults[std::to_string(observerActiveResult.first)] = activeResults;
  }
  outputJson["observerActiveResults"] = observerActiveResults;
}


void OutputGenerator::StoreObserverActiveResultsRawValues(json& outputJson)
{
  json observerActiveResults;
  // Iterate over observerId / (targetId / (result type / value)) pairs
  for (auto observerActiveResult : m_observerActiveResultsRawValues)
  {
    json activeResults;
    // Iterate over targetId / (result type / value) pairs
    for (auto activeResult : observerActiveResult.second)
    {
      json resultValues;
      // Iterate over result type / value pairs
      for (auto resultValue : activeResult.second)
      {
        resultValues[ResultTypeToString(resultValue.first)] = resultValue.second;
      }
      activeResults[std::to_string(activeResult.first)] = resultValues;
    }
    observerActiveResults[std::to_string(observerActiveResult.first)] = activeResults;
  }
  outputJson["observerActiveResultsRawValues"] = observerActiveResults;
}



void OutputGenerator::StoreLocalizationResults(json& outputJson)
{
  auto flowInfoMap = m_simResultSet->GetObserverFlowInfo();

  json localizationResults;

  for (auto locRes : m_localizationResults)
  {
    json locResult;
    locResult["filter"] = locRes.filter;
    locResult["results"] = locRes.results;
    locResult["config"] = {{"delayTh", locRes.clfcConfig.delayTh},
                           {"lossRateTh", locRes.clfcConfig.lossRateTh},
                           {"classification_base_id", locRes.clfcConfig.classification_base_id},
                           {"flowLengthTh", locRes.clfcConfig.flowLengthTh},
                           {"classificationMode", locRes.clfcConfig.classificationMode},
                           {"observerIds", locRes.clfcConfig.observerSet.observers}};
    locResult["flowSelection"] = {  {"selectionStrategy", locRes.flowSelectionStrategy.strategy},
                                    {"params", locRes.flowSelectionStrategy.params},
                                    {"selectionMapping", locRes.clfcConfig.flowSelectionMap}};

    if (!locRes.clfcConfig.observerSet.metadata.empty())
    {
      locResult["config"]["observerSetMetadata"] =
          json::parse(locRes.clfcConfig.observerSet.metadata);
    }

    for (auto fid : locRes.clfcConfig.flowIds)
    {
      locResult["config"]["flowIds"].push_back(flowInfoMap[fid].Serialize());
    }

    localizationResults.push_back(locResult);
  }

  outputJson["localizationResults"] = localizationResults;
}

void OutputGenerator::StoreGroundtruthStats(json& outputJson)
{
  outputJson["linkGroundtruthStats"] = m_simResultSet->GetLinkGroundtruthStats();
}


std::string ResultTypeToString(ResultType resultType)
{
  static const std::unordered_map<ResultType, std::string> mapping = {
      {ResultType::SEQ_REL_LOSS, "seq_rel_loss"},
      {ResultType::SEQ_ABS_LOSS, "seq_abs_loss"},
      {ResultType::ACK_SEQ_ABS_LOSS, "ack_seq_abs_loss"},
      {ResultType::ACK_SEQ_REL_LOSS, "ack_seq_rel_loss"},
      {ResultType::Q_REL_LOSS, "q_rel_loss"},
      {ResultType::Q_ABS_LOSS, "q_abs_loss"},
      {ResultType::R_REL_LOSS, "r_rel_loss"},
      {ResultType::R_ABS_LOSS, "r_abs_loss"},
      {ResultType::T_REL_FULL_LOSS, "t_rel_full_loss"},
      {ResultType::T_ABS_FULL_LOSS, "t_abs_full_loss"},
      {ResultType::T_REL_HALF_LOSS, "t_rel_half_loss"},
      {ResultType::T_ABS_HALF_LOSS, "t_abs_half_loss"},
      {ResultType::L_REL_LOSS, "l_rel_loss"},
      {ResultType::L_ABS_LOSS, "l_abs_loss"},
      {ResultType::SPIN_AVG_DELAY, "spin_avg_delay"},
      {ResultType::SQ_REL_LOSS, "sq_rel_loss"},
      {ResultType::SQ_ABS_LOSS, "sq_abs_loss"},
      {ResultType::TCPDART_AVG_DELAY, "tcpdart_avg_delay"},
      {ResultType::TCPRO_ABS_LOSS, "tcpro_abs_loss"},
      {ResultType::TCPRO_REL_LOSS, "tcpro_rel_loss"},
      {ResultType::PING_CLNT_ABS_LOSS, "ping_clnt_abs_loss"},
      {ResultType::PING_CLNT_REL_LOSS, "ping_clnt_rel_loss"},
      {ResultType::PING_CLNT_AVG_DELAY, "ping_clnt_avg_delay"},
      {ResultType::PING_SVR_ABS_LOSS, "ping_svr_abs_loss"},
      {ResultType::PING_SVR_REL_LOSS, "ping_svr_rel_loss"},
      {ResultType::PING_SVR_AVG_DELAY, "ping_svr_avg_delay"},
      {ResultType::SPIN_DELAY_RAW, "spin_delay_raw"},
      {ResultType::TCPDART_DELAY_RAW, "tcpdart_delay_raw"},
      {ResultType::PING_CLNT_DELAY_RAW, "ping_clnt_delay_raw"},
      {ResultType::PING_SVR_DELAY_RAW, "ping_svr_delay_raw"}};
  return mapping.at(resultType);
}

}  // namespace analysis
