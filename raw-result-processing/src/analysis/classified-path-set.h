#ifndef CLASSIFIED_PATH_SET_H
#define CLASSIFIED_PATH_SET_H

#include <sim-data-manager.h>

#include <nlohmann/json.hpp>
#include <sstream>

namespace analysis {

struct FailureStruct
{
    bool failed_and_medium_failure;
    bool small_failure;
    bool large_failure;
    double measurement;
};

struct ObserverSet
{
  std::set<uint32_t> observers{};
  std::string metadata{};
};

void to_json(nlohmann::json &jsn, const ObserverSet &obsSet);
void from_json(const nlohmann::json &jsn, ObserverSet &obsSet);

enum class EfmBit
{
  Q,
  L,
  R,
  T,
  SPIN,
  QR,
  QL,
  QT,
  LT,
  SEQ,
  TCPRO,    // TCP reordering
  TCPDART,  // TCP DART delay
  PINGDLY,  // Ping delay
  PINGLSS   // Ping loss
};
NLOHMANN_JSON_SERIALIZE_ENUM(EfmBit, {{EfmBit::Q, "Q"},
                                      {EfmBit::L, "L"},
                                      {EfmBit::R, "R"},
                                      {EfmBit::T, "T"},
                                      {EfmBit::SPIN, "SPIN"},
                                      {EfmBit::QR, "QR"},
                                      {EfmBit::QL, "QL"},
                                      {EfmBit::QT, "QT"},
                                      {EfmBit::LT, "LT"},
                                      {EfmBit::SEQ, "SEQ"},
                                      {EfmBit::TCPRO, "TCPRO"},
                                      {EfmBit::TCPDART, "TCPDART"},
                                      {EfmBit::PINGDLY, "PINGDLY"},
                                      {EfmBit::PINGLSS, "PINGLSS"}})

enum class ClassificationMode
{
  STATIC,
  PERFECT
};
NLOHMANN_JSON_SERIALIZE_ENUM(ClassificationMode, {{ClassificationMode::STATIC, "STATIC"},
                                                  {ClassificationMode::PERFECT, "PERFECT"}})


struct ClassificationConfig
{
  ObserverSet observerSet{};
  std::set<uint32_t> flowIds{};
  std::string classification_base_id{};
  double lossRateTh{};
  uint32_t delayTh{};
  uint32_t flowLengthTh{};
  ClassificationMode classificationMode{};
  std::map<uint32_t, std::set<uint32_t>> flowSelectionMap{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ClassificationConfig, observerSet, flowIds, classification_base_id, lossRateTh, delayTh,
                                   flowLengthTh, classificationMode)


struct ClassifiedLinkPath;

typedef std::pair<uint32_t, uint32_t> Link;
typedef std::vector<Link> LinkVec;
typedef std::set<EfmBit> EfmBitSet;
typedef std::vector<ClassifiedLinkPath> ClassPathVec;
typedef std::set<Link> LinkSet;

std::string efmbitset_to_string(EfmBitSet bitset);
std::string efmbit_to_string(EfmBit bit);

struct LinkPath
{
  LinkVec links;
  LinkPath GetUpToX(uint32_t nodeId) const;
  LinkPath GetPathFromXToEnd(uint32_t nodeId) const;
  bool ContainsNode(uint32_t nodeId) const;
  bool ContainsLink(Link check_link) const;
  LinkPath Append(const LinkPath &otherPath) const;
  LinkPath AppendTo(const LinkPath &otherPath) const;
};


std::string LinkPathToString(LinkPath linkPath);
std::string LinkToString(Link link);
std::string ObsvVantagePointPointerVectorToString(std::vector<simdata::ObsvVantagePointPointer> flowPath);

struct ClassifiedLinkPath
{
  LinkPath path;
  bool failed;
  bool small_failure;
  bool medium_failure;
  bool large_failure;
  double measurement;
};

class ClassifiedPathSet
{
public:
  /// @brief Generates a classified path set considering all available observers and all flows
  /// @param srs The simresultset to generate paths for
  /// @param bitCombis The Efm bits to consider for classification and generation of paths, e.g., Q,
  /// R, QR
  /// @param lossRateTh Loss rate to declare path as failed (inclusive)
  /// @param delayTh Average delay to declare path as failed (inclusive)
  /// @param classificationMode The classification mode to use
  /*
  static ClassifiedPathSet ClassifyAll(const simdata::SimResultSet &srs, const EfmBitSet &bitCombis,
                                       double lossRateTh, uint32_t delayTh, uint32_t flowLengthTh,
                                       ClassificationMode classificationMode,
                                       std::string classification_base_id,
                                        double smallFailFactor,
                                        double largeFailFactor,
                                        double time_filter);
*/

  /// @brief Generates a classified path set considering the specified observers and all their flows
  /// @param srs The simresultset to generate paths for
  /// @param observerIds The observers to consider for classification and generation of paths
  /// @param bitCombis The Efm bits to consider for classification and generation of paths, e.g., Q,
  /// R, QR
  /// @param lossRateTh Loss rate to declare path as failed (inclusive)
  /// @param delayTh Average delay to declare path as failed (inclusive)
  /// @param classificationMode The classification mode to use
  static ClassifiedPathSet ClassifyAll(const simdata::SimResultSet &srs,
                                       const std::set<uint32_t> &observerIds,
                                       const std::map<uint32_t, std::set<uint32_t>> flowSelectionMap,
                                       const EfmBitSet &bitCombis, double lossRateTh,
                                       uint32_t delayTh, uint32_t flowLengthTh, 
                                       ClassificationMode classificationMode,
                                       std::string classification_base_id,
                                        double smallFailFactor,
                                        double largeFailFactor,
                                        double time_filter);

  /// @param srs The simresultset to generate paths for
  /// @param observerIds The observers to consider for classification and generation of paths
  /// @param flowIds The flows to generate paths for
  /// @param bitCombis The Efm bits to consider for classification and generation of paths, e.g., Q,
  /// R, QR
  /// @param lossRateTh Loss rate to declare path as failed (inclusive)
  /// @param delayTh Average delay to declare path as failed (inclusive)
  /// @param classificationMode The classification mode to use
  static ClassifiedPathSet Classify(const simdata::SimResultSet &srs,
                                    const std::set<uint32_t> &observerIds,
                                    const std::set<uint32_t> &flowIds, 
                                    std::map<uint32_t, std::set<uint32_t>> flowSelectionMap,
                                    const EfmBitSet &bitCombis,
                                    double lossRateTh, uint32_t delayTh, uint32_t flowLengthTh,
                                    ClassificationMode classificationMode,
                                    std::string classification_base_id,
                                    double smallFailFactor,
                                    double largeFailFactor,
                                    double time_filter);


  void HandleActiveMeasurements(const simdata::SimResultSet &srs,
                                const std::set<uint32_t> &observerIds, const EfmBitSet &bitCombis,
                                double lossRateTh, uint32_t delayTh,
                                ClassificationMode classificationMode,
                                std::string classification_base_id,
                                double smallFailFactor,
                                double largeFailFactor);

  /// @brief Returns the classified paths for the specified observer and bit/bit combination
  /// @param bits A single bit or bit combi, e.g., T, QR, ...
  std::optional<ClassPathVec> GetClassifiedPaths(uint32_t observerId, EfmBit bits) const;

  /// @brief Returns the classified paths for the specified observers and bits/bit combinations
  /// @param observerIds Set of observer IDs
  /// @param bitCombis Vector of bits or bit combis, e.g., {T, QR, ...}
  ClassPathVec GetClassifiedPaths(const std::set<uint32_t> &observerIds,
                                  const EfmBitSet &bitCombis) const;


  const ClassificationConfig &GetConfig() const { return m_config; }

private:
  ClassifiedPathSet(double lossRateTh, uint32_t delayTh, uint32_t flowLengthTh,
                    ClassificationMode classificationMode, std::string classification_base_id);

  ClassificationConfig m_config;

  typedef std::map<uint32_t, std::map<EfmBit, ClassPathVec>> ObsvEfmbitPathMap;
  ObsvEfmbitPathMap m_classifiedPaths;

  // Generates bit paths for specified observer, flow and bits
  // For a bit combi, only the path resulting from the combination (not from the single bits) are
  // generated E.g., QL only generates the downstream loss path
  LinkPath GenerateUnidirBitPaths(uint32_t observerId, EfmBit bits, LinkPath &flowPath,
                                  LinkPath &reverseFlowPath) const;


  // Generates and stores classified bit paths for bit measurements that require a bidirectional
  // observer
  void HandleBidirBitPathClassification(const simdata::SimResultSet &srs,
                                        const simdata::ObsvVantagePointPointer &observer,
                                        EfmBit bits, uint32_t flowId, uint32_t reverseFlowId,
                                        LinkPath &flowPath, LinkPath &reverseFlowPath,
                                        double smallFailFactor,
                                        double largeFailFactor,
                                        double time_filter);

  FailureStruct ClassifyFlow(const simdata::ObsvVantagePointPointer &observer, uint32_t flowId, EfmBit bits,
                                double smallFailFactor,
                                double largeFailFactor,
                                double time_filter);

  bool ClassifyLinkPathViaGT(const simdata::SimResultSet &srs, const LinkPath &path, bool useLoss,
                             bool useDelay) const;
};


std::optional<LinkPath> GenerateLinkPath(std::vector<simdata::ObsvVantagePointPointer> flowPath);
std::optional<LinkPath> GenerateLinkPath(std::vector<uint32_t> flowPath);
bool IsLossBit(EfmBit bit);
bool AreLossBits(EfmBitSet bits);
bool AreSingleCombinationBit(EfmBitSet bits);
bool IsCombinationBit(EfmBit bit);
bool IsActiveMmntBit(EfmBit bit);
//LinkPath LinkPath::GetUpToX(uint32_t nodeId) const;
//LinkPath LinkPath::GetPathFromXToEnd(uint32_t nodeId) const;
//bool LinkPath::ContainsNode(uint32_t nodeId) const;
//LinkPath LinkPath::Append(const LinkPath &otherPath) const;
//LinkPath LinkPath::AppendTo(const LinkPath &otherPath) const;


}  // namespace analysis


#endif  // CLASSIFIED_PATH_SET_H