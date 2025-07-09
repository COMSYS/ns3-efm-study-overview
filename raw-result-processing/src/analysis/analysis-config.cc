#include "analysis-config.h"
#include <string>
namespace analysis {
using json = nlohmann::json;

void from_json(const json& jsn, AnalysisConfig& conf)
{
  jsn.at("storeMeasurements").get_to(conf.storeMeasurements);
  jsn.at("performLocalization").get_to(conf.performLocalization);
  jsn.at("efmBitSets").get_to(conf.efmBitSets);
  jsn.at("classificationModes").get_to(conf.classificationModes);
  if (jsn.contains("classification_base_id"))
  {
    jsn.at("classification_base_id").get_to(conf.classification_base_id);
  }
  else 
  {
    conf.classification_base_id = "dummy_id";
  }

  if (jsn.contains("flowLengthTh"))
    jsn.at("flowLengthTh").get_to(conf.flowLengthTh);

  // Observer Sets
  for (const auto& item : jsn.at("observerSets"))
  {
    // Each observer set is either a plane array of observer IDs or an object
    // with an "observers" array and a "metadata" object (for backwards compatibility)
    ObserverSet obsvSet;
    if (item.is_array())
    {
      item.get_to(obsvSet.observers);
    }
    else
    {
      item.get_to(obsvSet);
    }
    conf.observerSets.push_back(obsvSet);
  }


  // Lossrate Thresholds
  if (jsn.contains("lossRateTh"))
    conf.lossRateTh = jsn.at("lossRateTh").get<double>();
  else if (jsn.contains("autoLossRateThOffset"))
    conf.autoLossRateThOffset = jsn.at("autoLossRateThOffset").get<double>();
  else
    throw std::runtime_error("No loss rate threshold specified in analysis config.");


  // Delay Thresholds
  if (jsn.contains("delayThMs"))
    conf.delayThMs = jsn.at("delayThMs").get<uint32_t>();
  else if (jsn.contains("autoDelayThOffsetMs"))
    conf.autoDelayThOffsetMs = jsn.at("autoDelayThOffsetMs").get<int32_t>();
  else
    throw std::runtime_error("No delay threshold specified in analysis config.");

  // Localization methods
  for (auto& [key, val] : jsn.at("localizationMethods").items())
  {
    LocalizationParams params;
    try
    {
      val.get_to(params);
    }
    catch (const json::out_of_range& _)
    {  // We allow params to not be fully specified and just use the default params}
    }
    conf.localizationMethods[LocalizationMethodFromString(key)] = params;
  }

  // Flow Selection Strategies
  for (auto& [key, val] : jsn.at("flowSelectionStrategies").items())
  {
    FlowSelectionStrategyParams params;
    try
    {
      val.get_to(params);
    }
    catch (const json::out_of_range& _)
    {  // We allow params to not be fully specified and just use the default params}
    }
    conf.flowSelectionStrategies[FlowSelectionStrategyFromString(key)] = params;
  }

  jsn.at("simFilter").get_to(conf.simFilter);
  jsn.at("time_filter_ms").get_to(conf.time_filter_ms);
  conf.time_filter_ms *= 1000;
  jsn.at("output_raw_values").get_to(conf.output_raw_values);
}

}  // namespace analysis
