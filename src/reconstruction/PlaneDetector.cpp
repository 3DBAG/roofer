// This file is part of gfp-building-reconstruction
// Copyright (C) 2018-2022 Ravi Peters

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// #include "point_edge.h"
// #include "stepedge_nodes.hpp"

#include "PlaneDetectorBase.hpp"
#include "PlaneDetector.hpp"

#include <CGAL/Shape_detection/Efficient_RANSAC.h>

#include <CGAL/property_map.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Search_traits_3.h>
#include <CGAL/Search_traits_adapter.h>
#include <CGAL/Shape_regularization/regularize_planes.h>
#include <boost/container_hash/hash_fwd.hpp>
#include <cstddef>
#include <utility>
#include <functional>

// #include <CGAL/number_utils.h>
// #include <CGAL/Cartesian.h>
// #include <CGAL/Exact_predicates_exact_constructions_kernel.h>
// #include <CGAL/Exact_rational.h>

#include <CGAL/pca_estimate_normals.h>
#include <CGAL/mst_orient_normals.h>
// #include <CGAL/point_generators_3.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Search_traits_3.h>
#include <CGAL/linear_least_squares_fitting_3.h>

namespace roofer {

// Point with normal vector stored in a std::pair.
typedef boost::tuple<Point, Vector, int, bool, double, int, bool, double, int, bool> PNL;
typedef CGAL::Nth_of_tuple_property_map<0, PNL> Point_map;
typedef CGAL::Nth_of_tuple_property_map<1, PNL> Normal_map;
typedef CGAL::Nth_of_tuple_property_map<2, PNL> Label_map;
typedef CGAL::Nth_of_tuple_property_map<3, PNL> IsWall_map;
typedef CGAL::Nth_of_tuple_property_map<4, PNL> LineFit_map;
typedef CGAL::Nth_of_tuple_property_map<5, PNL> JumpCount_map;
typedef CGAL::Nth_of_tuple_property_map<6, PNL> IsStep_map;
typedef CGAL::Nth_of_tuple_property_map<7, PNL> JumpEle_map;
typedef CGAL::Nth_of_tuple_property_map<8, PNL> Id_map;
typedef CGAL::Nth_of_tuple_property_map<9, PNL> IsHorizontal_map;
typedef std::vector<PNL>                        PNL_vector;

struct AdjacencyFinder {

  typedef CGAL::Search_traits_3<EPICK>                       Traits_base;
  typedef CGAL::Search_traits_adapter<PNL,
  Point_map,
  Traits_base>                                              TreeTraits;
  typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits> Neighbor_search;
  typedef Neighbor_search::Tree Tree;
  

  std::map<size_t, std::map<size_t, size_t>> adjacencies;
  
  AdjacencyFinder(PNL_vector& points, size_t N=15)
  {    
    Tree tree;
    tree.insert(points.begin(), points.end());
    
    for(auto& pi : points){
      auto& p = boost::get<0>(pi);
      auto& l = boost::get<2>(pi);
      if(l==0) continue; // skip unsegmented points
      // std::cout << "pid:" << l << std::endl;
      Neighbor_search search(tree, p, N+1);
      // skip the first point since it is identical to the query point
      for (auto nb = search.begin()+1 ; nb < search.end(); ++nb) {
        // auto& p = boost::get<0>(pi);
        auto l_nb = boost::get<2>(nb->first);
        if(l_nb==0 || l_nb == l) continue; // skip unsegmented neighbours
        // std::cout << "dist:" << nb->second << std::endl;
        // std::cout << "p_x:" << boost::get<2>(nb->first) << std::endl;
        // std::cout << "pid_:" << l_nb << std::endl;
        if(l > l_nb) {
          adjacencies[l][l_nb]++;
        } else {
          adjacencies[l_nb][l]++;
        }
      }
    }

    // for(auto& [idl, value] : adjacencies) {
    //   for(auto& [idh, cnt] : value) {
    //     std::cout<< idl << ":" << idh << "-" << cnt << std::endl;
    //   }
    // }
  };
};

float compute_percentile(std::vector<float>& z_vec, float percentile) {
  assert(percentile<=1.);
  assert(percentile>=0.);
  size_t n = (z_vec.size()-1) * percentile;
  std::nth_element(z_vec.begin(), z_vec.begin()+n, z_vec.end());
  return z_vec[n];
}

namespace detection {

  // Concurrency
  #ifdef CGAL_LINKED_WITH_TBB
  typedef CGAL::Parallel_tag Concurrency_tag;
  #else
  typedef CGAL::Sequential_tag Concurrency_tag;
  #endif
  // search tree
  // typedef boost::tuple<Point_3,int>                           Point_and_int;
  // typedef CGAL::Random_points_in_cube_3<Point_3>              Random_points_iterator;
  typedef CGAL::Search_traits_3<EPICK>                       Traits_base;
  typedef CGAL::Search_traits_adapter<PNL,
    CGAL::Nth_of_tuple_property_map<0, PNL>,
    Traits_base>                                              TreeTraits;
  // typedef CGAL::Search_traits_3<SCK> TreeTraits;
  typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits> Neighbor_search;
  typedef Neighbor_search::Tree Tree;

  typedef CGAL::Shape_detection::Efficient_RANSAC_traits
  <EPICK, PNL_vector, Point_map, Normal_map>             Traits;
  typedef CGAL::Shape_detection::Efficient_RANSAC<Traits> Efficient_ransac;
  typedef CGAL::Shape_detection::Plane<Traits>            RansacPlane;

  struct Custom_plane_map
  {
    using key_type = Plane; // The iterator's value type is an index
    using value_type = Plane;  // The object manipulated by the algorithm is a Plane
    using reference = Plane&;   // The object does not exist in memory, so there's no reference
    using category = boost::read_write_property_map_tag; // The property map is used both
                                                        // for reading and writing data
    // The get() function returns the object expected by the algorithm (here, Point)
    friend Plane get (const Custom_plane_map& map, Plane& plane)
    {
      return plane;
    }
    friend const Plane get (const Custom_plane_map& map, const Plane& plane)
    {
      return plane;
    }
    // The put() function updated the user's data structure from the
    // object handled by the algorithm (here Plane)
    friend void put (const Custom_plane_map& map, Plane& plane_old, const Plane& plane_new)
    {
      plane_old = plane_new;
    }
  };
  struct Custom_plane_index_map
  {
    using key_type = std::size_t; // The iterator's value type is an index
    using value_type = int;  // The object manipulated by the algorithm is a Plane
    using reference = int;   // The object does not exist in memory, so there's no reference
    using category = boost::readable_property_map_tag; // The property map is used both
                                                        // for reading and writing data
    PNL_vector* plane_id;
    Custom_plane_index_map (PNL_vector* plane_id=nullptr)
      : plane_id (plane_id) { }
    // The get() function returns the object expected by the algorithm (here, Plane)
    // return plane based on point idx
    friend int get (const Custom_plane_index_map& map, const std::size_t& idx)
    {
      auto pid = boost::get<2>((*map.plane_id)[idx]);
      if (pid == 0)
        return -1;
      else
        return pid-1;
    }
  };

  // allow us to hash Plane instances
  template <class T>
  inline void hash_combine(std::size_t& seed, const T& v)
  {
      std::hash<T> hasher;
      seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
  }

  struct PlaneHash {
  std::size_t operator()(const Plane& k) const
  {
      size_t seed = std::hash<double>{}(k.a());
      boost::hash_combine(seed, k.b());
      boost::hash_combine(seed, k.c());
      boost::hash_combine(seed, k.d());
      return seed;
  }
  };

class PlaneDetector : public PlaneDetectorInterface {

  public:
  using PlaneDetectorInterface::PlaneDetectorInterface;

  void detect(const PointCollection& points, const PlaneDetectorConfig cfg) override {

    // convert to cgal points with attributes
    PNL_vector pnl_points;
    for (auto& p : points) {
      PNL pv;
      boost::get<0>(pv) = Point(p[0], p[1], p[2]);
      boost::get<2>(pv) = 0;
      boost::get<3>(pv) = 0;
      boost::get<9>(pv) = 0;
      pnl_points.push_back(pv);
    }
    // estimate normals
    if (points.size()) {
      CGAL::pca_estimate_normals<Concurrency_tag>(
        pnl_points, cfg.metrics_normal_k,
        CGAL::parameters::point_map(Point_map()).
        normal_map(Normal_map())
      );
    }
    // orient normals upwards
    auto up = Vector(0,0,1);
    for ( auto& pv : pnl_points) {
      auto &n = boost::get<1>(pv);
      if (n*up<0) 
        boost::get<1>(pv) = -n;
    }

    PointCollection points_vec;
    vec3f normals_vec;
    points_vec.reserve(points.size());

    // IndexedPlanesWithPoints pts_per_roofplane;
    // size_t horiz_roofplane_cnt=0;
    // size_t slant_roofplane_cnt=0;
    // size_t horiz_pt_cnt=0, total_pt_cnt=0, wall_pt_cnt=0, unsegmented_pt_cnt=0, total_plane_cnt=0;
    vec1f roof_elevations;

    std::vector<Plane> planes;

    if (!cfg.use_ransac) {
      // convert to lists required by the planedetector class
      // size_t i=0;

      for (auto &pt : pnl_points) {
        auto& p = boost::get<0>(pt);
        auto& n = boost::get<1>(pt);
        points_vec.push_back(
          {float(CGAL::to_double(p.x())), float(CGAL::to_double(p.y())), float(CGAL::to_double(p.z()))}
        );
        normals_vec.push_back(
          {float(CGAL::to_double(n.x())), float(CGAL::to_double(n.y())), float(CGAL::to_double(n.z()))}
        );
      }
      // perform plane detection
      planedect::PlaneDS PDS(points_vec, normals_vec, cfg.metrics_plane_k);
      planedect::DistAndNormalTester DNTester(
        cfg.metrics_plane_epsilon * cfg.metrics_plane_epsilon,
        cfg.metrics_plane_normal_threshold,
        cfg.n_refit
      );
      regiongrower::RegionGrower<planedect::PlaneDS, planedect::PlaneRegion> R;
      R.min_segment_count = cfg.metrics_plane_min_points;
      if (points.size() > cfg.metrics_plane_min_points)
        R.grow_regions(PDS, DNTester);

      total_plane_cnt = R.regions.size();
      // classify horizontal/vertical planes using plane normals
      unsigned shape_id = 0;
      for(auto region: R.regions){

        if(region.get_region_id() == 0) continue;
        
        auto& plane = region.plane;
        
        Vector n = plane.orthogonal_vector();
        // this dot product is close to 0 for vertical planes
        auto horizontality = CGAL::abs(n*Vector(0,0,1));
        bool is_wall = horizontality < cfg.metrics_is_wall_threshold;
        bool is_horizontal = horizontality > cfg.metrics_is_horizontal_threshold;      

        // put slanted surface points at index -1 if we care only about horzontal surfaces
        if (!is_wall) {
          ++shape_id;
          planes.push_back(plane);
          std::vector<Point> segpts;
          for (auto& i : region.inliers) {
            segpts.push_back(boost::get<0>(pnl_points[i]));
            roof_elevations.push_back(float(boost::get<0>(pnl_points[i]).z()));
            boost::get<2>(pnl_points[i]) = shape_id;
            boost::get<3>(pnl_points[i]) = is_wall;
            boost::get<9>(pnl_points[i]) = is_horizontal;
          }
          total_pt_cnt += segpts.size();
          pts_per_roofplane[shape_id].second = segpts;
          pts_per_roofplane[shape_id].first = plane;

          if (is_horizontal) {
            horiz_pt_cnt += segpts.size();
          }
        } else { // is_wall
          wall_pt_cnt += region.inliers.size();
        }
        if (is_horizontal)
          ++horiz_roofplane_cnt;
        else if (!is_wall && !is_horizontal)
          ++slant_roofplane_cnt;
      }

    } else { // use_ransac == true

      // Instantiate shape detection engine.
      Efficient_ransac ransac;
      // Provide input data.
      ransac.set_input(pnl_points);
      // Register planar shapes via template method.
      ransac.add_shape_factory<RansacPlane>();

      // Set parameters for shape detection.
      Efficient_ransac::Parameters parameters;
      // Set probability to miss the largest primitive at each iteration.
      parameters.probability = cfg.metrics_probability_ransac;
      // Detect shapes with at least 200 points.
      parameters.min_points = cfg.metrics_plane_min_points;
      // Set maximum Euclidean distance between a point and a shape.
      parameters.epsilon = cfg.metrics_plane_epsilon;
      // Set maximum Euclidean distance between points to be clustered.
      parameters.cluster_epsilon = cfg.metrics_cluster_epsilon_ransac;
      // Set maximum normal deviation.
      // 0.9 < dot(surface_normal, point_normal);
      parameters.normal_threshold = cfg.metrics_plane_normal_threshold;
      // Detect shapes.
      ransac.detect(parameters);
      // Print number of detected shapes.
      total_plane_cnt = ransac.shapes().end() - ransac.shapes().begin();

      unsigned shape_id = 0;
      for(auto shape: ransac.shapes()){
        
        RansacPlane* ransac_plane = dynamic_cast<RansacPlane*>(shape.get());
        Plane plane = static_cast<Plane>(*ransac_plane);
        Vector n = plane.orthogonal_vector();
        // this dot product is close to 0 for vertical planes
        auto horizontality = CGAL::abs(n*Vector(0,0,1));
        bool is_wall = horizontality < cfg.metrics_is_wall_threshold;
        bool is_horizontal = horizontality > cfg.metrics_is_horizontal_threshold;
        // put slanted surface points at index -1 if we care only about horzontal surfaces
        if (!is_wall) {
          ++shape_id;
          planes.push_back(plane);
          std::vector<Point> segpts;
          for (auto& i : shape->indices_of_assigned_points()) {
            segpts.push_back(boost::get<0>(pnl_points[i]));
            roof_elevations.push_back(float(boost::get<0>(pnl_points[i]).z()));
            boost::get<2>(pnl_points[i]) = shape_id;
            boost::get<3>(pnl_points[i]) = is_wall;
            boost::get<9>(pnl_points[i]) = is_horizontal;
          }
          total_pt_cnt += segpts.size();
          pts_per_roofplane[shape_id].second = segpts;
          pts_per_roofplane[shape_id].first = plane;

          if (is_horizontal) {
            horiz_pt_cnt += segpts.size();
          }
        } else { // is_wall
          wall_pt_cnt += shape->indices_of_assigned_points().size();
        }
        if (is_horizontal)
          ++horiz_roofplane_cnt;
        else if (!is_wall && !is_horizontal)
          ++slant_roofplane_cnt;

      }
    }
    
    // vec1i plane_id, is_wall, is_horizontal;
    for(auto& p : pnl_points) {
      auto pid = boost::get<2>(p);
      if (pid==0) ++unsegmented_pt_cnt;
      plane_id.push_back(pid);
      // is_wall.push_back(boost::get<3>(p));
      // is_horizontal.push_back(boost::get<9>(p));
    }

    // Plane regularisation

    // START Regularize detected planes.
    if (cfg.regularize_parallelism_ ||
        cfg.regularize_orthogonality_ ||
        cfg.regularize_coplanarity_ ||
        cfg.regularize_axis_symmetry_) {
      std::cout << "\nN planes before: " << pts_per_roofplane.size() << std::endl;
      CGAL::Shape_regularization::Planes::regularize_planes(
        planes,
        pnl_points,
        CGAL::parameters::
        plane_map(Custom_plane_map()).
        point_map(Point_map()).
        plane_index_map(Custom_plane_index_map(&pnl_points)).
        maximum_angle(cfg.maximum_angle_).
        maximum_offset(cfg.maximum_offset_).
        regularize_parallelism(cfg.regularize_parallelism_).
        regularize_orthogonality(cfg.regularize_orthogonality_).
        regularize_coplanarity(cfg.regularize_coplanarity_).
        regularize_axis_symmetry(cfg.regularize_axis_symmetry_)
        // symmetry_direction(symmetry_direction_)
      );

      std::unordered_map<Plane, std::vector<size_t>, PlaneHash> plane_merge_map;
      size_t pt_i=0;
      for (auto& p :pnl_points) {
        // std::cout << "pt_i=" << pt_i << std::endl;
        auto pid = boost::get<2>(p);
        // boost::get<2>(p) = 0;
        // std::cout << "pid=" << pid << std::endl;
        if (pid > 0){
          const auto& pl = planes[pid-1];
          plane_merge_map[pl].push_back( pt_i );
        }
        ++pt_i;
      }
      std::cout << "plane_merge_map.size=" << plane_merge_map.size() << std::endl;
      pts_per_roofplane.clear();

      int plane_cnt = 1;
      for(auto& [plane, pt_i_vec] : plane_merge_map) {
        Vector n = plane.orthogonal_vector();
        // this dot product is close to 0 for vertical planes
        auto horizontality = CGAL::abs(n*Vector(0,0,1));
        bool is_wall = horizontality < cfg.metrics_is_wall_threshold;

        if (!is_wall) {
          std::vector<Point> ptvec;
          ptvec.reserve(pt_i_vec.size());
          for (auto& pt_i : pt_i_vec) {
            boost::get<2>(pnl_points[pt_i]) = plane_cnt;
            plane_id[pt_i] = plane_cnt;
            ptvec.push_back(boost::get<0>(pnl_points[pt_i]));
          }
          pts_per_roofplane[plane_cnt] = std::make_pair(plane, ptvec);
          ++plane_cnt;
        }
      }
      std::cout << "N planes after: " << plane_cnt-1 << std::endl;
    }

    // END Regularize detected planes.

    AdjacencyFinder adj_finder(pnl_points, cfg.metrics_plane_k);
    plane_adjacencies = adj_finder.adjacencies;

    bool b_is_horizontal = float(horiz_pt_cnt)/float(total_pt_cnt) > cfg.horiz_min_count;
    // int roof_type=-2; // as built: -2=undefined; -1=no pts; 0=LOD1, 1=LOD1.3, 2=LOD2
    roof_type = "no planes";
    if (total_plane_cnt==0) {
      // roof_type=-1;
      roof_type = "no points";
    } else if (horiz_roofplane_cnt==1 && slant_roofplane_cnt==0){
      // roof_type=0;
      roof_type = "horizontal";
    } else if (b_is_horizontal){
      // roof_type=1;
      roof_type = "multiple horizontal";
    } else if (slant_roofplane_cnt > 0) {
      // roof_type=2;
      roof_type = "slanted";
    }

    if (roof_elevations.size()) {
      roof_elevation_70p = compute_percentile(roof_elevations, 0.7);
      roof_elevation_50p = compute_percentile(roof_elevations, 0.5);
      roof_elevation_min = compute_percentile(roof_elevations, 0.0);
      roof_elevation_max = compute_percentile(roof_elevations, 1.0);
    }
    
  }
};

std::unique_ptr<PlaneDetectorInterface> createPlaneDetector() {
  return std::make_unique<PlaneDetector>();
};

} // namespace detection

} // namespace roofer