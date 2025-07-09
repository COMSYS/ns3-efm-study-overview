#ifndef SIM_FILTER_H
#define SIM_FILTER_H

#include <cstdint>
#include <nlohmann/json.hpp>
#include <set>

namespace simdata {

struct SimFilter
{
  bool lBitTriggeredMonitoring = false;
  uint32_t removeLastXSpinTransients = 0;
  bool IsDefault() const { return !lBitTriggeredMonitoring && removeLastXSpinTransients == 0; }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SimFilter, lBitTriggeredMonitoring, removeLastXSpinTransients)


}  // namespace simdata

#endif  // SIM_FILTER_H