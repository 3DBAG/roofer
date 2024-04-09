#include "../datastructures.hpp"
#include "cgal_shared_definitions.hpp"
#include <memory>

namespace roofer::detection {

struct PlaneDetectorConfig{
    float horiz_min_count = 0.95;
    int metrics_normal_k = 5;
    int metrics_plane_k = 15;
    int metrics_plane_min_points = 20;
    float metrics_plane_epsilon = 0.2;
    float metrics_plane_normal_threshold = 0.75;
    float metrics_is_horizontal_threshold = 0.97;
    float metrics_probability_ransac = 0.05;
    float metrics_cluster_epsilon_ransac = 0.3;
    float metrics_is_wall_threshold = 0.3;
    int n_refit = 5;
    bool use_ransac = false;
    // float roof_percentile=0.5;

    // plane regularisation
    float maximum_angle_ = 25;
    float maximum_offset_ = 0.5;
    bool regularize_parallelism_ = false;
    bool regularize_orthogonality_ = false;
    bool regularize_coplanarity_ = false;
    bool regularize_axis_symmetry_ = false;
};

struct PlaneDetectorInterface {

    vec1i plane_id;
    IndexedPlanesWithPoints pts_per_roofplane;
    std::map<size_t, std::map<size_t, size_t> > plane_adjacencies;

    size_t horiz_roofplane_cnt=0;
    size_t slant_roofplane_cnt=0;
    size_t horiz_pt_cnt=0, total_pt_cnt=0, wall_pt_cnt=0, unsegmented_pt_cnt=0, total_plane_cnt=0;

    std::string roof_type;
    float roof_elevation_70p;
    float roof_elevation_50p;
    float roof_elevation_min;
    float roof_elevation_max;

    virtual ~PlaneDetectorInterface() = default;
    virtual void detect(const PointCollection& points, PlaneDetectorConfig config=PlaneDetectorConfig()) = 0;
};

std::unique_ptr<PlaneDetectorInterface> createPlaneDetector();

struct ShapeDetectorInterface {

  virtual unsigned detectPlanes(
    PointCollection& point_collection, 
    vec3f& normals, 
    vec1i& labels,
    float probability = 0.01,
    int min_points = 15,
    float epsilon = 0.2,
    float cluster_epsilon = 0.5,
    float normal_threshold = 0.8
  ) = 0;

};

std::unique_ptr<ShapeDetectorInterface> createShapeDetector();

}