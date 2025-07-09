#ifndef ANALYSIS_MANAGER_H
#define ANALYSIS_MANAGER_H

#include <sim-result-set.h>

#include "analysis-config.h"
#include "failure-localization.h"
#include "output-generator.h"

namespace analysis {

class AnalysisManager
{
public:
  static void RunAnalyses(simdata::SimResultSetPointer simResultSet, const std::string &outputFile,
                          std::vector<AnalysisConfig> analysisConfigs);
  static void RunAnalysis(simdata::SimResultSetPointer simResultSet, const std::string &outputFile,
                          AnalysisConfig analysisConfig);

protected:
  static void DoRunAnalysis(simdata::SimResultSetPointer simResultSet, OutputGenerator &outGen,
                            AnalysisConfig &analysisConfig);

private:
};


}  // namespace analysis


#endif  // ANALYSIS_MANAGER_H