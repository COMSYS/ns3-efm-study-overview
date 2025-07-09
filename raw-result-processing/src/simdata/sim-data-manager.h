#ifndef SIM_DATA_MANAGER_H
#define SIM_DATA_MANAGER_H

#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

#include "sim-result-set.h"
#include "simdjson.h"

namespace simdata {


class SimDataManager
{
  using json = nlohmann::json;

public:
  /// @brief Imports a new simulation result from a file
  /// @param parser A simdjson parser (reusing it is more efficient)
  /// @param file The path to a QLOG json file that should be imported
  /// @return true if a new result set was created, false if the data were appended to an existing
  /// one
  bool ImportResult(simdjson::ondemand::parser &parser, std::string file);

  bool GetResultSet(SimId id, SimResultSetPointer &resultSet);

  std::vector<SimId> GetSimIds();

  void ClearResults() { m_simResultMap.clear(); }

  // #####################
  // ##### templates #####
  // #####################

  template <typename T>
  std::vector<T> SelectFromSimResults(std::function<T(SimResultSetPointer)> selector)
  {
    std::vector<T> result;
    for (auto it = m_simResultMap.begin(); it != m_simResultMap.end(); it++)
    {
      result.push_back(selector(it->second));
    }

    return result;
  }

  template <typename T>
  std::vector<T> SelectFromSimResults(std::function<T(SimResultSetPointer)> selector,
                                      std::function<bool(SimResultSetPointer)> filter)
  {
    std::vector<T> result;
    for (auto it = m_simResultMap.begin(); it != m_simResultMap.end(); it++)
    {
      if (filter(it->second))
        result.push_back(selector(it->second));
    }

    return result;
  }

  template <typename T>
  std::vector<T> SelectFromObserverFlows(uint32_t observerId, uint32_t flowId,
                                         std::function<T(SimObsvFlowPointer)> selector)
  {
    std::vector<T> result;
    for (auto it = m_simResultMap.begin(); it != m_simResultMap.end(); it++)
    {
      result.push_back(selector((*it->second).GetObserverVP(observerId)->GetFlow(flowId)));
    }

    return result;
  }

  // #########################
  // ##### templates end #####
  // #########################

protected:
  typedef std::map<SimId, SimResultSetPointer> SimResultMap;
  SimResultMap m_simResultMap;

private:
};

}  // namespace simdata


#endif  // SIM_DATA_MANAGER_H