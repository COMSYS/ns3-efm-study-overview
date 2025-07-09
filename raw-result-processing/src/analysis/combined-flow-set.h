#ifndef COMBINED_FLOW_SET_H
#define COMBINED_FLOW_SET_H

#include <sim-data-manager.h>
#include "classified-path-set.h"
#include <sstream>
#include <algorithm>

#include <nlohmann/json.hpp>

namespace analysis {



void to_json(nlohmann::json &jsn, const ObserverSet &obsSet);
void from_json(const nlohmann::json &jsn, ObserverSet &obsSet);


struct CombinedFlowSetConfig
{
  ObserverSet observerSet{};
  std::set<uint32_t> flowIds{};
  std::string classification_base_id{};
  uint32_t flowLengthTh{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CombinedFlowSetConfig, observerSet, flowIds, classification_base_id, flowLengthTh)



typedef std::pair<uint32_t, uint32_t> Link;
typedef std::vector<Link> LinkVec;
typedef std::set<EfmBit> EfmBitSet;
typedef std::set<Link> LinkSet;
typedef std::tuple <uint32_t, LinkPath, double> FC_Tuple;
typedef std::tuple <uint32_t, LinkPath, std::pair<uint32_t, uint32_t>> FC_TupleQBit;

typedef std::vector<int> ConnectivityVector;
typedef std::vector<double> MeasurementVector;
typedef std::vector<ConnectivityVector> ConnectivityMatrix;
typedef std::map<Link, uint32_t> LinkIndexMap;
typedef std::map<uint32_t, Link> ReverseLinkIndexMap;
typedef std::pair<ConnectivityMatrix, MeasurementVector> ConnMatrixMeasVecPair;


std::string ConnectivityVectorToString(ConnectivityVector vector);
std::string MeasurementVectorToString(MeasurementVector vector);
std::string ConnectivityVectorToStringWithLinkMapping(ConnectivityVector vector, ReverseLinkIndexMap linkIndexMap);

class CombinedFlowSet
{
public:
  /// @brief Generates a classified path set considering the specified observers and all their flows
  /// @param srs The simresultset to generate paths for
  /// @param observerIds The observers to consider for classification and generation of paths
  /// @param bitCombis The Efm bits to consider for classification and generation of paths, e.g., Q,
  /// R, QR
  /// @param lossRateTh Loss rate to declare path as failed (inclusive)
  /// @param delayTh Average delay to declare path as failed (inclusive)
  /// @param classificationMode The classification mode to use
  static CombinedFlowSet CharacterizeAll(const simdata::SimResultSet &srs,
                                       const std::set<uint32_t> &observerIds,
                                       const std::map<uint32_t, std::set<uint32_t>> flowSelectionMap,
                                       const EfmBitSet &bitCombis,
                                       uint32_t flowLengthTh,
                                       ClassificationMode classificationMode,
                                       std::string classification_base_id,
                                       double time_filter);

  /// @param srs The simresultset to generate paths for
  /// @param observerIds The observers to consider for classification and generation of paths
  /// @param flowIds The flows to generate paths for
  /// @param bitCombis The Efm bits to consider for classification and generation of paths, e.g., Q,
  /// R, QR
  /// @param lossRateTh Loss rate to declare path as failed (inclusive)
  /// @param delayTh Average delay to declare path as failed (inclusive)
  /// @param classificationMode The classification mode to use
  static CombinedFlowSet Characterize(const simdata::SimResultSet &srs,
                                              const std::set<uint32_t> &observerIds,
                                              const std::set<uint32_t> &flowIds,
                                              std::map<uint32_t, std::set<uint32_t>> flowSelectionMap,
                                              const EfmBitSet &bitCombis, 
                                              const LinkIndexMap link_index_map, 
                                              const ReverseLinkIndexMap reverse_link_index_map,
                                              uint32_t flowLengthTh,
                                              std::string classification_base_id,
                                              double time_filter);

  /// @brief Returns the classified paths for the specified observer and bit/bit combination
  /// @param bits A single bit or bit combi, e.g., T, QR, ...
  std::optional<ConnMatrixMeasVecPair> GetConnectivityMatrixMeasurementVector(uint32_t observerId,
                                                                              EfmBit bits) const;

  /// @brief Returns the classified paths for the specified observers and bits/bit combinations
  /// @param observerIds Set of observer IDs
  /// @param bitCombis Vector of bits or bit combis, e.g., {T, QR, ...}
  ConnMatrixMeasVecPair GetConnectivityMatrixMeasurementVector(const std::set<uint32_t> &observerIds,
                                                   const EfmBitSet &bitCombis) const;


  static ConnectivityVector LinkPathToConnectivityVector(const LinkPath &flowPath, const LinkIndexMap &linkIndexMap);


  const CombinedFlowSetConfig &GetConfig() const { return m_config; }
  CombinedFlowSet();

  const ReverseLinkIndexMap &GetReverseLinkIndexMap() const { return m_reverseLinkIndexMapping; }

private:
  CombinedFlowSet(uint32_t flowLengthTh, std::string classification_base_id);

  CombinedFlowSetConfig m_config;

  typedef std::map<uint32_t, std::map<EfmBit, ClassPathVec>> ObsvEfmbitPathMap;
  typedef std::map<uint32_t, std::map<EfmBit, ConnectivityMatrix>> ObsvEfmConnectivityMatrixMap;
  typedef std::map<uint32_t, std::map<EfmBit, MeasurementVector>> ObsvEfmMeasurementVectorMap;


  //ObsvEfmbitPathMap m_classifiedPaths;
  ObsvEfmConnectivityMatrixMap m_connectivityMatrix;
  ObsvEfmMeasurementVectorMap m_measurementVector;
  LinkIndexMap m_linkIndexMapping;
  ReverseLinkIndexMap m_reverseLinkIndexMapping;

  // Generates bit paths for specified observer, flow and bits
  // For a bit combi, only the path resulting from the combination (not from the single bits) are
  // generated E.g., QL only generates the downstream loss path
  LinkPath GeneratePathVisibilityForFlowCombination(uint32_t observerId, EfmBit bits, LinkPath &flowPath,
                                  LinkPath &reverseFlowPath) const;

  LinkPath GetLinkPathDifference(LinkPath &longerPath, LinkPath &shorterPath) const;

  double ExtractFlowMeasurement(const simdata::ObsvVantagePointPointer &observer, uint32_t flowId, EfmBit bits, double time_filter);
  std::pair<uint32_t,uint32_t> ExtractFlowMeasurementPair(const simdata::ObsvVantagePointPointer &observer, uint32_t flowId, EfmBit bits, double time_filter);
};

}  // namespace analysis


#endif  // CLASSIFIED_PATH_SET_H