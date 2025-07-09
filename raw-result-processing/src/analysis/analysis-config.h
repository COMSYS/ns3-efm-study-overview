#ifndef ANALYSIS_CONFIG_H_
#define ANALYSIS_CONFIG_H_

#include <sim-filter.h>

#include <nlohmann/json.hpp>

#include "classified-path-set.h"
#include "failure-localization.h"

namespace analysis {

struct AnalysisConfig
{
  bool storeMeasurements = true;
  bool performLocalization = true;
  std::string classification_base_id;
  std::vector<ObserverSet> observerSets;
  std::vector<EfmBitSet> efmBitSets;
  std::optional<double> lossRateTh;
  std::optional<uint32_t> delayThMs;
  uint32_t flowLengthTh = 0;
  std::optional<int32_t> autoDelayThOffsetMs;
  std::optional<double> autoLossRateThOffset;
  std::vector<ClassificationMode> classificationModes;
  std::map<LocalizationMethod, LocalizationParams> localizationMethods;
  std::map<FlowSelectionStrategy, FlowSelectionStrategyParams> flowSelectionStrategies;
  simdata::SimFilter simFilter;
  double time_filter_ms;
  bool output_raw_values = false;
};

void from_json(const nlohmann::json &jsn, AnalysisConfig &conf);

}  // namespace analysis


#endif  // ANALYSIS_CONFIG_H_