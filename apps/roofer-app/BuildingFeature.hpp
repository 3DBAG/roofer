#pragma once

#include "types.hpp"
#include <roofer/common/common.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

/**
 * @brief A unified building feature that encapsulates both geometry and
 * attributes
 *
 * This class represents a GIS-like Feature pattern where a building contains
 * both its geometric properties (inherited from BuildingObject) and its
 * attributes in a single, cohesive object. This eliminates the need for
 * separate AttributeVecMap with index-based coupling.
 */
class BuildingFeature : public BuildingObject {
 public:
  // Attribute storage - similar to AttributeMapRow but owned by the feature
  using AttributeValue =
      std::variant<std::monostate, bool, int, std::string, float, roofer::arr3f,
                   roofer::Date, roofer::Time, roofer::DateTime>;
  using AttributeMap = std::unordered_map<std::string, AttributeValue>;

 private:
  AttributeMap attributes_;

 public:
  BuildingFeature() = default;

  // Constructor from BuildingObject - useful for migration
  explicit BuildingFeature(const BuildingObject& building_obj)
      : BuildingObject(building_obj) {}

  // Copy constructor and assignment
  BuildingFeature(const BuildingFeature& other) = default;
  BuildingFeature& operator=(const BuildingFeature& other) = default;

  // Move constructor and assignment
  BuildingFeature(BuildingFeature&& other) noexcept = default;
  BuildingFeature& operator=(BuildingFeature&& other) noexcept = default;

  // Attribute management methods
  template <typename T>
  void setAttribute(const std::string& name, const T& value) {
    attributes_[name] = value;
  }

  template <typename T>
  void setOptionalAttribute(const std::string& name,
                            const std::optional<T>& opt_value) {
    if (opt_value.has_value()) {
      attributes_[name] = opt_value.value();
    } else {
      attributes_[name] = std::monostate{};
    }
  }

  template <typename T>
  std::optional<T> getAttribute(const std::string& name) const {
    auto it = attributes_.find(name);
    if (it != attributes_.end()) {
      if (auto* value = std::get_if<T>(&it->second)) {
        return *value;
      }
    }
    return std::nullopt;
  }

  bool hasAttribute(const std::string& name) const {
    return attributes_.find(name) != attributes_.end();
  }

  bool isAttributeNull(const std::string& name) const {
    auto it = attributes_.find(name);
    if (it != attributes_.end()) {
      return std::holds_alternative<std::monostate>(it->second);
    }
    return true;  // Missing attribute is considered null
  }

  void setAttributeNull(const std::string& name) {
    attributes_[name] = std::monostate{};
  }

  // Get all attributes (for serialization/debugging)
  const AttributeMap& getAttributes() const { return attributes_; }
  AttributeMap& getAttributes() { return attributes_; }

  // Remove attribute
  void removeAttribute(const std::string& name) { attributes_.erase(name); }

  // Clear all attributes
  void clearAttributes() { attributes_.clear(); }

  // Convenience methods for commonly used attributes (based on RooferConfig)
  void setSuccess(bool success) { setAttribute("rf_success", success); }
  bool getSuccess() const {
    return getAttribute<bool>("rf_success").value_or(false);
  }

  void setReconstructionTime(int time_ms) { setAttribute("rf_t_run", time_ms); }
  int getReconstructionTime() const {
    return getAttribute<int>("rf_t_run").value_or(0);
  }

  void setGroundHeight(float height) { setAttribute("rf_h_ground", height); }
  float getGroundHeight() const {
    return getAttribute<float>("rf_h_ground").value_or(h_ground);
  }

  void setPointcloud98Percentile(float height) {
    setAttribute("rf_h_pc_98p", height);
  }
  float getPointcloud98Percentile() const {
    return getAttribute<float>("rf_h_pc_98p").value_or(h_pc_98p);
  }

  void setRoofType(const std::string& type) {
    setAttribute("rf_roof_type", type);
  }
  std::string getRoofType() const {
    return getAttribute<std::string>("rf_roof_type").value_or(roof_type);
  }

  void setIsGlassRoof(bool is_glass) {
    setAttribute("rf_is_glass_roof", is_glass);
  }
  bool getIsGlassRoof() const {
    return getAttribute<bool>("rf_is_glass_roof").value_or(is_glass_roof);
  }

  void setPointcloudUnusable(bool unusable) {
    setAttribute("rf_pointcloud_unusable", unusable);
  }
  bool getPointcloudUnusable() const {
    return getAttribute<bool>("rf_pointcloud_unusable")
        .value_or(pointcloud_insufficient);
  }

  void setExtrusionMode(const std::string& mode) {
    setAttribute("rf_extrusion_mode", mode);
  }
  std::string getExtrusionMode() const {
    auto mode = getAttribute<std::string>("rf_extrusion_mode");
    if (mode.has_value()) return mode.value();

    // Convert enum to string as fallback
    switch (extrusion_mode) {
      case STANDARD:
        return "standard";
      case LOD11_FALLBACK:
        return "lod11_fallback";
      case SKIP:
        return "skip";
      default:
        return "unknown";
    }
  }

  // LOD-specific convenience methods
  void setRmseLod12(float rmse) {
    setOptionalAttribute("rf_rmse_lod12", rmse_lod12);
  }
  void setRmseLod13(float rmse) {
    setOptionalAttribute("rf_rmse_lod13", rmse_lod13);
  }
  void setRmseLod22(float rmse) {
    setOptionalAttribute("rf_rmse_lod22", rmse_lod22);
  }

  void setVolumeLod12(float volume) {
    setOptionalAttribute("rf_volume_lod12", volume_lod12);
  }
  void setVolumeLod13(float volume) {
    setOptionalAttribute("rf_volume_lod13", volume_lod13);
  }
  void setVolumeLod22(float volume) {
    setOptionalAttribute("rf_volume_lod22", volume_lod22);
  }

#if RF_USE_VAL3DITY
  void setVal3dityLod12(const std::string& result) {
    setOptionalAttribute("rf_val3dity_lod12", val3dity_lod12);
  }
  void setVal3dityLod13(const std::string& result) {
    setOptionalAttribute("rf_val3dity_lod13", val3dity_lod13);
  }
  void setVal3dityLod22(const std::string& result) {
    setOptionalAttribute("rf_val3dity_lod22", val3dity_lod22);
  }
#endif
};

/**
 * @brief Collection of BuildingFeatures with smart pointer management
 *
 * Uses shared_ptr to avoid expensive copies during processing while maintaining
 * memory safety. This addresses the memory efficiency concern for large
 * BuildingFeature objects.
 */
class BuildingFeatureCollection {
 public:
  using BuildingFeaturePtr = std::shared_ptr<BuildingFeature>;
  using Container = std::vector<BuildingFeaturePtr>;
  using iterator = Container::iterator;
  using const_iterator = Container::const_iterator;

 private:
  Container features_;

 public:
  BuildingFeatureCollection() = default;

  // Reserve capacity for efficiency
  void reserve(size_t capacity) { features_.reserve(capacity); }

  // Add feature (creates shared_ptr)
  void addFeature(const BuildingFeature& feature) {
    features_.emplace_back(std::make_shared<BuildingFeature>(feature));
  }

  void addFeature(BuildingFeature&& feature) {
    features_.emplace_back(
        std::make_shared<BuildingFeature>(std::move(feature)));
  }

  void addFeature(BuildingFeaturePtr feature) {
    features_.emplace_back(feature);
  }

  // Create feature in-place
  template <typename... Args>
  BuildingFeaturePtr emplaceFeature(Args&&... args) {
    auto feature =
        std::make_shared<BuildingFeature>(std::forward<Args>(args)...);
    features_.emplace_back(feature);
    return feature;
  }

  // Access methods
  BuildingFeaturePtr operator[](size_t index) { return features_[index]; }
  const BuildingFeaturePtr operator[](size_t index) const {
    return features_[index];
  }

  BuildingFeaturePtr at(size_t index) { return features_.at(index); }
  const BuildingFeaturePtr at(size_t index) const {
    return features_.at(index);
  }

  // Container interface
  size_t size() const { return features_.size(); }
  bool empty() const { return features_.empty(); }
  void clear() { features_.clear(); }

  iterator begin() { return features_.begin(); }
  iterator end() { return features_.end(); }
  const_iterator begin() const { return features_.begin(); }
  const_iterator end() const { return features_.end(); }
  const_iterator cbegin() const { return features_.cbegin(); }
  const_iterator cend() const { return features_.cend(); }

  // Remove feature
  void erase(iterator it) { features_.erase(it); }
  void erase(size_t index) { features_.erase(features_.begin() + index); }

  // Get raw container (for compatibility with existing code)
  Container& getContainer() { return features_; }
  const Container& getContainer() const { return features_; }
};
