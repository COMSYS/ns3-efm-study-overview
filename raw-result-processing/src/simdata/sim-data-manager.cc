#include "sim-data-manager.h"

#include <stdexcept>

namespace simdata {


bool SimDataManager::ImportResult(simdjson::ondemand::parser &parser, std::string file)
{
  using namespace simdjson;

  auto jsonstring = padded_string::load(file);
  ondemand::document doc = parser.iterate(jsonstring);

  ondemand::object root_obj = doc.get_object();

  std::string_view simIdView;
  auto error = root_obj["title"].get(simIdView);
  if (error == NO_SUCH_FIELD)
  {
    if (root_obj["title_ref"].get(simIdView) != SUCCESS)
      throw std::runtime_error("Error while parsing title_ref field.");

    std::string simId(simIdView);
    auto it = m_simResultMap.find(simId);
    if (it == m_simResultMap.end())
      throw std::runtime_error("title_ref field points to non-existing sim id.");

    it->second->ImportAndAppendResult(root_obj);
    return false;
  }
  else if (error != SUCCESS)
    throw std::runtime_error("Error while parsing title field.");
  else
  {
    std::string simId(simIdView);
    auto it = m_simResultMap.find(simId);
    if (it != m_simResultMap.end())
      throw std::runtime_error("Duplicate sim id.");

    SimResultSetPointer srsp = std::make_shared<SimResultSet>(root_obj);
    m_simResultMap.insert(std::pair<SimId, SimResultSetPointer>(simId, srsp));
    return true;
  }
}


bool SimDataManager::GetResultSet(SimId id, SimResultSetPointer &resultSet)
{
  auto it = m_simResultMap.find(id);
  if (it == m_simResultMap.end())
    return false;

  resultSet = it->second;
  return true;
}

std::vector<SimId> SimDataManager::GetSimIds()
{
  std::vector<SimId> result;
  for (auto it = m_simResultMap.begin(); it != m_simResultMap.end(); it++)
  {
    result.push_back(it->first);
  }

  return result;
}

}  // namespace simdata
