#include "projHelper.hpp"
#include <proj.h>

#include <iostream>

namespace roofer {
  static const char *proj_wkt_options[] = {"MULTILINE=NO", NULL};

  struct projHelper : public projHelperInterface {

    using projHelperInterface::projHelperInterface;

    PJ_CONTEXT *projContext = nullptr;
    PJ *processCRS = nullptr;
    PJ *sCRS = nullptr;
    PJ *tCRS = nullptr;
    PJ *projFwdTransform = nullptr;
    PJ *projRevTransform = nullptr;

    void proj_clear() override {
      data_offset.reset();
      proj_context_destroy(projContext);
      projContext = proj_context_create();
      proj_destroy(processCRS);
      processCRS = nullptr;
      clear_fwd_crs_transform();
      clear_rev_crs_transform();
    };
    void proj_construct() override {
      projContext = proj_context_create();
    };
    void proj_clone_from(const projHelperInterface& other_proj_helper) override {
      const projHelper* other_proj = static_cast<const projHelper*>(&other_proj_helper);
      data_offset = *other_proj->data_offset;
      projContext = proj_context_clone(other_proj->projContext);
      if(other_proj->processCRS) processCRS = proj_clone(projContext, other_proj->processCRS);
      if(other_proj->projFwdTransform) projFwdTransform = proj_clone(projContext, other_proj->projFwdTransform);
      if(other_proj->projRevTransform) projRevTransform = proj_clone(projContext, other_proj->projRevTransform);
    };

    arr3f coord_transform_fwd(const double& x, const double& y, const double& z) override {
      PJ_COORD coord = proj_coord(x, y, z, 0);

      if (projFwdTransform) coord = proj_trans(projFwdTransform, PJ_FWD, coord);

      if(!data_offset.has_value()) {
        data_offset = {coord.xyz.x, coord.xyz.y, coord.xyz.z};
      }
      auto result = arr3f{
        float(coord.xyz.x - (*data_offset)[0]),
        float(coord.xyz.y - (*data_offset)[1]),
        float(coord.xyz.z - (*data_offset)[2])
      };

      return result;
    };
    arr3d coord_transform_rev(const float& x, const float& y, const float& z) override {
      PJ_COORD coord;
      if(!data_offset.has_value()) {
        coord = proj_coord(x, y, z, 0);
      } else {
        coord = proj_coord(
          x + (*data_offset)[0],
          y + (*data_offset)[1],
          z + (*data_offset)[2],
          0
        );
      };

      if (projRevTransform) coord = proj_trans(projRevTransform, PJ_FWD, coord);

      return arr3d{coord.xyz.x, coord.xyz.y, coord.xyz.z};
    }
    arr3d coord_transform_rev(const arr3f& p) override  {
      return coord_transform_rev(p[0], p[1], p[2]);
    };

    void set_process_crs(const char* crs) override{
      // https://proj.org/development/reference/functions.html#c.proj_create
      processCRS = proj_create(projContext, crs);
      processCRS = proj_normalize_for_visualization(projContext, processCRS);
      if (!processCRS)
        throw rooferException("Unable to create CRS from string: " + std::string(crs));
    };
    void set_fwd_crs_transform(const char* source_crs, bool normalize_for_visualization = false) override {
      if(processCRS) {
        sCRS = proj_create(projContext, source_crs);
        if (!sCRS)
          throw rooferException("Unable to create source CRS from string: " + std::string(source_crs));

        if (normalize_for_visualization) sCRS = proj_normalize_for_visualization(projContext, sCRS);

        projFwdTransform = proj_create_crs_to_crs_from_pj(projContext, sCRS, processCRS, 0, 0);

        if (!projFwdTransform)
          throw rooferException("Unable to create forward transformation.");
      } else {
        // std::cout << "Unable to create CRS transform, process CRS is undefined\n";
      }
    };
    void set_rev_crs_transform(const char* target_crs, bool normalize_for_visualization = false) override {
      if (processCRS) {
        tCRS = proj_create(projContext, target_crs);
        if (!tCRS)
          throw rooferException("Unable to create source CRS from string: " + std::string(target_crs));

        if (normalize_for_visualization) tCRS = proj_normalize_for_visualization(projContext, tCRS);

        projRevTransform = proj_create_crs_to_crs_from_pj(projContext, processCRS, tCRS, 0, 0);

        if (!projRevTransform)
          throw rooferException("Unable to create reverse transformation.");
      } else {
        // std::cout << "Unable to create CRS transform, process CRS is undefined\n";
      }
    };
    std::string get_rev_crs_id_auth_name() override {
      std::string auth_name;
      if (tCRS)
        auth_name = proj_get_id_auth_name(tCRS, 0);
      return auth_name;
    };
    std::string get_rev_crs_id_code() override {
      std::string code;
      if (tCRS)
        code = proj_get_id_code(tCRS, 0);
      return code;
    };
    
    std::string get_rev_crs_wkt() override {
      std::string wkt;
      if (tCRS)
        wkt = proj_as_wkt(projContext, tCRS, PJ_WKT1_GDAL, proj_wkt_options);
      return wkt;
    };
    void clear_fwd_crs_transform() override {
      proj_destroy(projFwdTransform);
      proj_destroy(sCRS);
      projFwdTransform = nullptr;
    };
    void clear_rev_crs_transform() override {
      proj_destroy(projRevTransform);
      proj_destroy(tCRS);
      projRevTransform = nullptr;
    };

    void set_data_offset(arr3d& offset) override {
      data_offset = offset;
    }
  };


  std::unique_ptr<projHelperInterface> createProjHelper() {
    return std::make_unique<projHelper>();
  };
};
// std::unique_ptr<projHelperInterface> createProjHelper(projHelperInterface& );