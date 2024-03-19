#pragma once
#include "datastructures.hpp"
#include <memory>

namespace roofer {

  struct projHelperInterface {

    std::optional<arr3d> data_offset;

    projHelperInterface() {};
    virtual ~projHelperInterface() = default;

    virtual void proj_construct() = 0;
    virtual void proj_clone_from(const projHelperInterface&) = 0;
    virtual void proj_clear() = 0;

    virtual arr3f coord_transform_fwd(const double& x, const double& y, const double& z) = 0;
    virtual arr3d coord_transform_rev(const float& x, const float& y, const float& z) = 0;
    virtual arr3d coord_transform_rev(const arr3f& p) = 0;

    virtual void set_process_crs(const char* crs) = 0;
    virtual void set_fwd_crs_transform(const char* source_crs, bool normalize_for_visualization = false) = 0;
    virtual void set_rev_crs_transform(const char* target_crs, bool normalize_for_visualization = false) = 0;
    virtual std::string get_rev_crs_id_auth_name() = 0;
    virtual std::string get_rev_crs_id_code() = 0;
    virtual std::string get_rev_crs_wkt() = 0;
    virtual void clear_fwd_crs_transform() = 0;
    virtual void clear_rev_crs_transform() = 0;

    virtual void set_data_offset(arr3d& offset) = 0;
  };

  std::unique_ptr<projHelperInterface> createProjHelper();
}
