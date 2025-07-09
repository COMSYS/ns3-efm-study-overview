
#include <iostream>
#include <nlohmann/json.hpp>

#include "analysis-config.h"
#include "analysis-manager.h"
#include "failure-localization.h"
#include "helper-templates.h"
#include "sim-data-manager.h"

using json = nlohmann::json;

using namespace simdata;
using namespace analysis;
namespace fs = std::filesystem;


void PrintFlowPath(SimResultSetPointer &srs, uint32_t flowId)
{
  std::vector<ObsvVantagePointPointer> path = srs->CalculateFlowPath(flowId);

  std::cout << "Path of flow " << flowId << ": ";
  bool first = true;
  for (auto it = path.begin(); it != path.end(); it++)
  {
    if (first)
      first = false;
    else
      std::cout << ", ";
    std::cout << (*it)->m_nodeId;
  }
  std::cout << std::endl;
}

void FindRelevantFiles(std::string path, std::string prefix, std::string extension,
                       std::map<std::string, std::set<std::filesystem::path>> &fileMap)
{
  for (const auto &entry : std::filesystem::directory_iterator(path))
  {
    if (entry.path().filename().string().rfind(prefix, 0) == std::string::npos)
      continue;
    if (entry.path().extension().string() != extension)
      continue;

    // We expect file name to be of the form "prefix-<runId>.json" or
    // "prefix-<runId>.fragment.json" and want to group all files with the
    // same prefix and runId together
    std::string fileStem = entry.path().stem().string();
    auto pos = fileStem.rfind('.');
    // This is the case where we do not split the qlog files into multiple files
    if (pos == std::string::npos)
      fileMap[fileStem].insert(entry.path());
    // In this case, we split it. Hence, we need to make sure that we do not include wrong prefixes
    else {
        // Check if we have a valid file that directly follows with a '-'
        if (entry.path().filename().string().find(prefix + "-") != std::string::npos) {
            fileMap[fileStem.substr(0, pos)].insert(entry.path());
        } else{
            continue;
        }
    }
  }
}

bool ImportFileSet(const std::set<std::filesystem::path> &fileSet,
                   simdjson::ondemand::parser &parser, SimDataManager &dataManager)
{
  bool createdSet = false;
  uint32_t count = 0;
  std::string total = std::to_string(fileSet.size());
  for (const auto &filepath : fileSet)
  {
    count++;
    std::cout << "\r[File " << std::to_string(count) << "/" << total << "] Importing file "
              << filepath << "..." << std::flush;
    if (dataManager.ImportResult(parser, filepath))
    {
      if (!createdSet)
        createdSet = true;
      else
        return false;
    }
  }
  std::cout << " Done." << std::endl;
  return true;
}

bool PrepareQlogFiles(fs::path basePath, std::string filePrefix, std::string &pathPrefix,
                      std::map<std::string, std::set<std::filesystem::path>> &fileMap)
{
  static const std::string extension = ".json";

  fs::path path = basePath;
  // If the prefix contains a path, extract it and append it to the base path
  auto pos = filePrefix.rfind('/');
  if (pos != std::string::npos)
  {
    if (pos == 0)
    {
      std::cerr << "Error: Prefix " << filePrefix << " is invalid." << std::endl;
      return false;
    }
    pathPrefix = filePrefix.substr(0, pos);
    // Make sure pathPrefix does not start with a /
    if (pathPrefix.at(0) == '/')
      pathPrefix = pathPrefix.substr(1);
    path = basePath / pathPrefix;

    filePrefix = filePrefix.substr(pos + 1);
  }

  if (!std::filesystem::exists(path))
  {
    std::cerr << "Error: Path " << path << " does not exist." << std::endl;
    return false;
  }

  FindRelevantFiles(path, filePrefix, extension, fileMap);


  if (fileMap.empty())
  {
    std::cerr << "Error: No files with prefix " << filePrefix << " found in path " << path
              << std::endl;
    return false;
  }
  return true;
}

bool LoadAnalysisConfig(std::string filePath, std::vector<AnalysisConfig> &configs)
{
  std::ifstream configFile;
  configFile.open(filePath);
  if (!configFile.is_open())
  {
    std::cerr << "Error: Could not open analysis config file " << filePath << std::endl;
    return false;
  }
  json config_j = json::array();
  configFile >> config_j;  // Parse json
  configFile.close();
  int counter = 0;
  for (auto it = config_j.begin(); it != config_j.end(); it++)
  {
    AnalysisConfig config = it.value().get<AnalysisConfig>();
    if (config.classification_base_id == "dummy_id") {
        config.classification_base_id = "default_id_" + std::to_string(counter);
    }
    counter++;
    configs.push_back(config);
  }

  return true;
}


struct CLIArgs
{
  fs::path simOutputDir = "../ns-3-dev-fork/output/";
  fs::path analysisOutputDir = "./data/analysis-results/";
  std::string qlogFilePrefix = "";
  fs::path analysisConfigFile = "./data/analysis-config.json";
};

void PrintCLIUsage(const std::string &programName)
{
  std::cout << "Usage: " << programName
            << " <qlogFilePrefix> [-c analysisConfigFile] [-s simOutputDir] [-a "
               "analysisOutputDir]\n"
            << "Example: " << programName
            << " download/eq-10-5MB -c ./data/analysis-config.json -s ../ns-3-dev-fork/output/ -a "
               "./data/analysis-results/\n"
            << "This will analyze all files starting with eq-10-5MB in the "
               "directory ../ns-3-dev-fork/output/download using "
               "the config specified in ./data/analysis-config.json and output the results to "
               "./data/analysis-results/download/."
            << std::endl;
}

std::pair<bool, CLIArgs> ParseCli(int arg, char *argv[])
{
  CLIArgs args;

  if (arg < 2)
  {
    PrintCLIUsage(argv[0]);
    return std::make_pair(false, args);
  }

  args.qlogFilePrefix = argv[1];

  if (arg > 2)
  {
    for (int i = 2; i < arg; i++)
    {
      std::string argStr = argv[i];
      if (argStr == "-c")
      {
        if (i + 1 >= arg)
        {
          std::cerr << "Error: Missing argument for -c." << std::endl;
          return std::make_pair(false, args);
        }
        args.analysisConfigFile = argv[i + 1];
        i++;
      }
      else if (argStr == "-s")
      {
        if (i + 1 >= arg)
        {
          std::cerr << "Error: Missing argument for -s." << std::endl;
          return std::make_pair(false, args);
        }
        args.simOutputDir = argv[i + 1];
        i++;
      }
      else if (argStr == "-a")
      {
        if (i + 1 >= arg)
        {
          std::cerr << "Error: Missing argument for -a." << std::endl;
          return std::make_pair(false, args);
        }
        args.analysisOutputDir = argv[i + 1];
        i++;
      }
      else
      {
        std::cerr << "Error: Unknown argument " << argStr << "." << std::endl;
        return std::make_pair(false, args);
      }
    }
  }

  // Validation and cleanup

  if (args.qlogFilePrefix.find('/') == 0)
  {
    if (args.qlogFilePrefix.size() == 1)
    {
      std::cerr << "Error: Prefix " << args.qlogFilePrefix << " is invalid." << std::endl;
      return std::make_pair(false, args);
    }
    args.qlogFilePrefix = args.qlogFilePrefix.substr(1);
  }

  if (!std::filesystem::exists(args.analysisOutputDir))
  {
    std::cerr << "Error: Path " << args.analysisOutputDir << " does not exist." << std::endl;
    return std::make_pair(false, args);
  }

  if (!std::filesystem::exists(args.simOutputDir))
  {
    std::cerr << "Error: Path " << args.simOutputDir << " does not exist." << std::endl;
    return std::make_pair(false, args);
  }

  if (!std::filesystem::exists(args.analysisConfigFile) || !args.analysisConfigFile.has_filename())
  {
    std::cerr << "Error: File " << args.analysisConfigFile << " does not exist." << std::endl;
    return std::make_pair(false, args);
  }

  return std::make_pair(true, args);
}

int main(int arg, char *argv[])
{
  // Handle cli arguments
  auto [success, cliArgs] = ParseCli(arg, argv);
  if (!success)
    return -1;

  // Prepare qlog file information
  std::string pathPrefix;
  std::map<std::string, std::set<std::filesystem::path>> fileMap;
  if (!PrepareQlogFiles(cliArgs.simOutputDir, cliArgs.qlogFilePrefix, pathPrefix, fileMap))
    return -1;

  // Load analysis configs
  std::vector<AnalysisConfig> analysisConfigs;
  if (!LoadAnalysisConfig(cliArgs.analysisConfigFile, analysisConfigs))
    return -1;

  SimDataManager dataManager;
  // It is more efficient to reuse the parser, so create it here and pass it
  // for each call of the ImportResult function
  simdjson::ondemand::parser parser;

  std::string analysisOutputPath = cliArgs.analysisOutputDir.string();
  if (analysisOutputPath.back() != '/')
    analysisOutputPath += "/";
  if (pathPrefix.size() > 0)
    analysisOutputPath += pathPrefix + "/";


  for (const auto &fileSet : fileMap)
  {
    std::cout << "Begin import of files with prefix " << fileSet.first << std::endl;

    if (!ImportFileSet(fileSet.second, parser, dataManager))
    {
      std::cerr << "Error: Multiple result sets found in files with prefix " << fileSet.first
                << std::endl;
      return -1;
    }

    std::cout << "Import for prefix " << fileSet.first << " finished. Starting analysis... "
              << std::flush;

    SimResultSetPointer srs;
    if (!dataManager.GetResultSet(dataManager.GetSimIds().front(), srs))
    {
      std::cerr << "Error accessing result set" << std::endl;
      return -1;
    }

    AnalysisManager::RunAnalyses(srs, analysisOutputPath + "analysis-" + fileSet.first + ".json",
                                 analysisConfigs);

    std::cout << "Done." << std::endl;
    srs.reset();
    dataManager.ClearResults();
  }

}  // main
