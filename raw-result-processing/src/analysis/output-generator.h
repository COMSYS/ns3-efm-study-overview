#ifndef OUTPUT_GENERATOR_H
#define OUTPUT_GENERATOR_H

#include <sim-result-set.h>

#include <nlohmann/json.hpp>

#include "failure-localization.h"

namespace analysis {

enum class ResultType
{
  SEQ_REL_LOSS,
  SEQ_ABS_LOSS,
  ACK_SEQ_REL_LOSS,
  ACK_SEQ_ABS_LOSS,
  Q_REL_LOSS,
  Q_ABS_LOSS,
  R_REL_LOSS,
  R_ABS_LOSS,
  T_REL_FULL_LOSS,
  T_ABS_FULL_LOSS,
  T_REL_HALF_LOSS,
  T_ABS_HALF_LOSS,
  L_REL_LOSS,
  L_ABS_LOSS,
  SPIN_AVG_DELAY,
  SQ_REL_LOSS,
  SQ_ABS_LOSS,
  TCPDART_AVG_DELAY,
  TCPRO_ABS_LOSS,
  TCPRO_REL_LOSS,
  PING_CLNT_ABS_LOSS,
  PING_CLNT_REL_LOSS,
  PING_CLNT_AVG_DELAY,
  PING_SVR_ABS_LOSS,
  PING_SVR_REL_LOSS,
  PING_SVR_AVG_DELAY,
  SPIN_DELAY_RAW,
  TCPDART_DELAY_RAW,
  PING_CLNT_DELAY_RAW,
  PING_SVR_DELAY_RAW
};

const std::array<ResultType, 2> FLOW_RESULT_TYPES_RAW_VALUES = {
    ResultType::SPIN_DELAY_RAW,      
    ResultType::TCPDART_DELAY_RAW};

const std::array<ResultType, 18> FLOW_RESULT_TYPES = {
    ResultType::SEQ_REL_LOSS,      ResultType::SEQ_ABS_LOSS,    ResultType::ACK_SEQ_REL_LOSS,
    ResultType::ACK_SEQ_ABS_LOSS,  ResultType::Q_REL_LOSS,      ResultType::Q_ABS_LOSS,
    ResultType::R_REL_LOSS,        ResultType::R_ABS_LOSS,      ResultType::T_REL_FULL_LOSS,
    ResultType::T_ABS_FULL_LOSS,   ResultType::T_REL_HALF_LOSS, ResultType::T_ABS_HALF_LOSS,
    ResultType::L_REL_LOSS,        ResultType::L_ABS_LOSS,      ResultType::SPIN_AVG_DELAY,
    ResultType::TCPDART_AVG_DELAY, ResultType::TCPRO_ABS_LOSS,  ResultType::TCPRO_REL_LOSS};

const std::array<ResultType, 4> PATH_RESULT_TYPES = {
    ResultType::SQ_REL_LOSS, ResultType::SQ_ABS_LOSS, ResultType::L_REL_LOSS,
    ResultType::L_ABS_LOSS};

const std::array<ResultType, 3> PING_CLIENT_RESULT_TYPES = {ResultType::PING_CLNT_ABS_LOSS,
                                                            ResultType::PING_CLNT_REL_LOSS,
                                                            ResultType::PING_CLNT_AVG_DELAY};

const std::array<ResultType, 1> PING_CLIENT_RESULT_TYPES_RAW = {ResultType::PING_CLNT_DELAY_RAW};                                                      

const std::array<ResultType, 3> PING_SERVER_RESULT_TYPES = {ResultType::PING_SVR_ABS_LOSS, 
                                                            ResultType::PING_SVR_REL_LOSS, 
                                                            ResultType::PING_SVR_AVG_DELAY};
                                                            
const std::array<ResultType, 1> PING_SERVER_RESULT_TYPES_RAW = {ResultType::PING_SVR_DELAY_RAW};

std::string ResultTypeToString(ResultType resultType);


struct LocalizationResultSet
{
  LocalizationResultSet(simdata::SimFilter filter, ClassificationConfig clfcConfig,
                        std::vector<LocalizationResult> results,
                        FlowSelectionStrategyWithParams flowSelectionStrategy)
      : filter(filter), clfcConfig(clfcConfig), results(results), flowSelectionStrategy(flowSelectionStrategy)
  {
  }
  simdata::SimFilter filter;
  ClassificationConfig clfcConfig;
  std::vector<LocalizationResult> results;
  FlowSelectionStrategyWithParams flowSelectionStrategy;
};

class OutputGenerator
{
  using json = nlohmann::json;

public:
  OutputGenerator(simdata::SimResultSetPointer simResultSet, const std::string &m_outputFile)
      : m_outputFile(m_outputFile), m_simResultSet(simResultSet)
  {
  }

  void GenerateOutput(bool pretty = false);

  void AddObserverFlowResult(uint32_t observerId, uint32_t flowId, ResultType resultType,
                             double resultValue);
  void AddObserverFlowResultBulk(uint32_t observerId, uint32_t flowId,
                                 std::map<ResultType, double> resultValues);
  void AddObserverFlowResultList(uint32_t observerId, uint32_t flowId, ResultType resultType, 
                                std::list<double> resultList);
  void AddObserverPathResult(uint32_t observerId, uint32_t pathId, ResultType resultType,
                             double resultValue);
  void AddObserverActiveResult(uint32_t observerId, uint32_t targetId, ResultType resultType,
                               double resultValue);
  void AddObserverActiveResultList(uint32_t observerId, uint32_t targetId, ResultType resultType,
                               std::list<double> resultList);
  void AddLocalizationResults(simdata::SimFilter &filter, ClassificationConfig &clfcConfig,
                              std::vector<LocalizationResult> &result,
                              FlowSelectionStrategyWithParams flowSelectionStrategy);

protected:
  std::string m_outputFile;
  simdata::SimResultSetPointer m_simResultSet;
  // Maps observerId -> flowId -> resultType -> resultValue
  typedef std::map<uint32_t, std::map<uint32_t, std::map<ResultType, double>>>
      ObserverFlowResultMap;
  ObserverFlowResultMap m_observerFlowResults;
  // Maps observerId -> pathId -> resultType -> resultValue
  ObserverFlowResultMap m_observerPathResults;
  // Maps observerId -> targetId -> resultType -> resultValue
  ObserverFlowResultMap m_observerActiveResults;

  typedef std::map<uint32_t, std::map<uint32_t, std::map<ResultType, std::list<double>>>>
      ObserverFlowResultRawValuesMap;
  ObserverFlowResultRawValuesMap m_observerFlowResultsRawValues;
  ObserverFlowResultRawValuesMap m_observerActiveResultsRawValues;
  std::vector<LocalizationResultSet> m_localizationResults;

private:
  /// @brief Creates the path to the output file and dumps the json to it
  /// @param outputJson The json to dump
  /// @param pretty Whether to pretty print the json
  void CreatePathAndDump(const json &outputJson, bool pretty);

  /// @brief Creates a map of flowId -> path for each observed flow and stores it in the output json
  /// @param outputJson The json to store the map in
  void StoreFlowPathMap(json &outputJson);

  /// @brief Stores the list of failed links (per the configuration) in the output json
  /// @param outputJson
  void StoreFailedLinks(json &outputJson);

  void StoreBackboneLinkOverrides(json &outputJson);


  /// @brief Stores the list of all links, core links and edge links in the output json
  /// @param outputJson
  void StoreLinkSets(json &outputJson);

  /// @brief Stores the content of @ref m_observerFlowResults in the output json
  /// @param outputJson The json to store the results in
  void StoreObserverFlowResults(json &outputJson);

  void StoreObserverFlowResultsRawValues(json &outputJson);

  /// @brief Stores the content of @ref m_observerPathResults in the output json
  /// @param outputJson The json to store the results in
  void StoreObserverPathResults(json &outputJson);

  /// @brief Stores the content of @ref m_observerActiveResults in the output json
  /// @param outputJson The json to store the results in
  void StoreObserverActiveResults(json &outputJson);

  void StoreObserverActiveResultsRawValues(json &outputJson);

  void StoreLocalizationResults(json &outputJson);

  void StoreGroundtruthStats(json &outputJson);
};

}  // namespace analysis


#endif  // OUTPUT_GENERATOR_H