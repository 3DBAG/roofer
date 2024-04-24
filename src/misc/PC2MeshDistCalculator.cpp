#include "PC2MeshDistCalculator.hpp"

#include <CGAL/Simple_cartesian.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits.h>

#include <CGAL/Search_traits_3.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>

#include <list>

namespace roofer::detection {

  typedef CGAL::Simple_cartesian<double> SCK;

  // custom triangle type with
  // three pointers to points
  struct My_triangle {
      roofer::Triangle m_t;
      size_t m_face_id;
      My_triangle(roofer::Triangle t, size_t face_id)
          : m_t(t), m_face_id(face_id) {}
  };
  // the custom triangles are stored into a vector
  typedef std::list<My_triangle>::const_iterator Iterator;
  // The following primitive provides the conversion facilities between
  // the custom triangle and point types and the CGAL ones
  struct My_triangle_primitive {
  public:
      // this is the type of data that the queries returns. For this example
      // we imagine that, for some reasons, we do not want to store the iterators
      // of the vector, but raw pointers. This is to show that the Id type
      // does not have to be the same as the one of the input parameter of the 
      // constructor.
      typedef const My_triangle* Id;
      // CGAL types returned
      typedef SCK::Point_3    Point; // CGAL 3D point type
      typedef SCK::Triangle_3 Datum; // CGAL 3D triangle type
  private:
      Id m_mytri; // this is what the AABB tree stores internally
  public:
      My_triangle_primitive() {} // default constructor needed
      // the following constructor is the one that receives the iterators from the 
      // iterator range given as input to the AABB_tree
      My_triangle_primitive(Iterator it)
          : m_mytri(&(*it)) {}
      const Id& id() const { return m_mytri; }
      // on the fly conversion from the internal data to the CGAL types
      Point convert(const roofer::arr3f& p) const {
        return Point(p[0], p[1], p[2]);
      }
      Datum datum() const {
          return Datum(
            convert(m_mytri->m_t[0]),
            convert(m_mytri->m_t[1]),
            convert(m_mytri->m_t[2])
          );
      }
      // returns a reference point which must be on the primitive
      Point reference_point() const { 
        return convert(m_mytri->m_t[0]);
      }
  };
  
  class PC2MeshDistCalculator : public PC2MeshDistCalculatorInterface{

    vec1f compute_mesh2pc_errors(const TriangleCollection& triangles, std::vector<SCK::Point_3> points) {
      // KD tree
      typedef CGAL::Search_traits_3<SCK> TreeTraits;
      typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits> Neighbor_search;
      typedef Neighbor_search::Tree Tree;

      Tree tree(points.begin(), points.end());
      vec1f errors;
      const unsigned int N = 1;
      for(const auto& triangle : triangles) {
        for (const auto& p : triangle) {
          Neighbor_search search(tree, SCK::Point_3(p[0],p[1],p[2]), N);
          for(Neighbor_search::iterator it = search.begin(); it != search.end(); ++it){
            errors.push_back(std::sqrt(it->second));
          }
        }
      }
      return errors;
    }

    vec1f compute_pc2pc_errors(PointCollection points_from, PointCollection points_to) {
      // KD tree
      typedef CGAL::Search_traits_3<SCK> TreeTraits;
      typedef CGAL::Orthogonal_k_neighbor_search<TreeTraits> Neighbor_search;
      typedef Neighbor_search::Tree Tree;

      std::vector<SCK::Point_3> points_to_;
      for (auto& p : points_to) {
        points_to_.push_back(SCK::Point_3(p[0],p[1],p[2]));
      }
      Tree tree(points_to_.begin(), points_to_.end());
      vec1f errors;
      const unsigned int N = 1;
      for(const auto& p : points_from) {
        Neighbor_search search(tree, SCK::Point_3(p[0],p[1],p[2]), N);
        for(Neighbor_search::iterator it = search.begin(); it != search.end(); ++it) {
          errors.push_back(std::sqrt(it->second));
        }
      }
      return errors;
    }

    std::string get_json_histogram(vec1f& values) {
      std::sort(values.begin(), values.end(), [](auto& p1, auto& p2) {
        return p1 < p2;
      });
      const std::vector<float> limits = {0.0,0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0,1.5,2.0,2.5,3.0,4.0,5.0,6.0};
      std::vector<size_t> counts(limits.size(), 0);
      bool do_count;
      for (auto&e : values) {
        for( size_t i = 0; i<limits.size(); ++i) {
          if (i==limits.size()-1) {
            do_count = e>=limits[i];
          } else {
            do_count = e>=limits[i] && e<limits[i+1];
          }
          if (do_count) {
            ++counts[i];
            break;
          }
        }
      }
      std::stringstream json;
      json << "{\"lower_limits\":[" << std::setprecision(2);
      auto sep = "";
      for (auto& l : limits) {
        json << sep << l;
        sep = ",";
      }
      json << "],\"counts\":[";
      sep = "";
      for (auto& c : counts) {
        json << sep << c;
        sep = ",";
      }
      json << "]}";
      return json.str();
    }

    public:
    void compute(
      const std::vector<SCK::Point_3>& points,
      const MultiTriangleCollection& mtcs,
      const roofer::vec1i& face_ids,
      PC2MeshDistCalculatorConfig cfg
    ) {
      typedef CGAL::AABB_traits<SCK, My_triangle_primitive> My_AABB_traits;
      typedef CGAL::AABB_tree<My_AABB_traits> Tree;

      TriangleCollection trin;
      for (size_t j=0; j<mtcs.tri_size(); j++) {
        const auto& tc = mtcs.tri_at(j);
        const auto& labels = mtcs.attr_at(j).at("labels");
        for (size_t i=0; i<tc.size(); ++i) {
          if(std::get<int>(labels[i]) == 1)
            trin.push_back(tc[i]);
        }
      }
      
      // do not run if one of the inputs is empty
      if(points.size()==0 || trin.size()==0) {
        std::cout << "Either input points or triangles are empty. Stopping execution.";
        return;
      }

      std::list<My_triangle> triangles;
      for (size_t i=0; i < trin.size(); ++i) {
        triangles.push_back(My_triangle(trin[i], face_ids[i*3]));
      }

      // constructs AABB tree
      Tree tree(triangles.begin(), triangles.end());
      tree.accelerate_distance_queries();

      std::map<size_t, std::vector<double>> distances;
      vec1f point_errors;
      size_t i = 0;
      for(auto& p : points) {
        auto pt_and_id = tree.closest_point_and_primitive(p);
        auto sqd = CGAL::squared_distance(pt_and_id.first, p);
        auto fid = pt_and_id.second->m_face_id;
        distances[fid].push_back(sqd);
        point_errors.push_back(sqd);
      }

      double sum_total = 0;
      size_t len = 0;
      std::map<size_t, float> face_error_map;
      for (auto& [fid, errors] : distances) {
        double sum_face = 0;
        len += errors.size();
        for(double& error : errors) {
          sum_face += error;
        }
        face_error_map[fid] = float(CGAL::sqrt(sum_face/errors.size()));
        sum_total += sum_face;
      }
      rms_error = float(CGAL::sqrt(sum_total/len));

      vec1f face_errors;
      for (size_t i=0; i < trin.size(); ++i) {
        size_t fid = face_ids[i*3];
        // push zero error if this face has no points/no error defined
        if(face_error_map.find(fid) == face_error_map.end()) {
          face_errors.push_back(0);
          face_errors.push_back(0);
          face_errors.push_back(0);
        } else {
          face_errors.push_back(face_error_map[fid]);
          face_errors.push_back(face_error_map[fid]);
          face_errors.push_back(face_error_map[fid]);
        }
      }

      vec1f mesh_error;
      for (auto& t : triangles) {
        mesh_error.push_back(rms_error);
        mesh_error.push_back(rms_error);
        mesh_error.push_back(rms_error);
      }

      // compute point error stats
      // {
      //   auto json = get_json_histogram(point_errors);
      //   output("error_hist").set(json);
      // }

      // compute mesh to point cloud errors
      // {
      //   auto m2pc_errors = compute_mesh2pc_errors(trin, points);
      //   auto json = get_json_histogram(m2pc_errors);
      //   output("m2pc_error_hist").set(json);
      //   output("m2pc_error_max").set(m2pc_errors.back());
      // }

      // output("point_errors").set(point_errors);
      // output("face_errors").set(face_errors);
      // output("mesh_error_f").set(rms_error);
      // output("mesh_error").set(mesh_error);
    }
    void compute(
      const PointCollection& ipoints,
      const MultiTriangleCollection& mtcs,
      const roofer::vec1i& face_ids,
      PC2MeshDistCalculatorConfig cfg
    ) override {

      std::vector<SCK::Point_3> points;
      for (auto& p : ipoints) {
        points.push_back(SCK::Point_3(p[0], p[1], p[2]));
      }
      compute(
        points,
        mtcs,
        face_ids,
        cfg
      );

    }
    void compute(
      const IndexedPlanesWithPoints& points_per_plane,
      const MultiTriangleCollection& mtcs,
      const roofer::vec1i& face_ids,
      PC2MeshDistCalculatorConfig cfg
    ) override {
      std::vector<SCK::Point_3> points;

      for (auto& [plane_id, plane_pts] : points_per_plane) {
        if(plane_id>0) {
          for(auto& p : plane_pts.second) {
            points.push_back(SCK::Point_3(p.x(), p.y(), p.z()));
          }
        }
      }
      compute(
        points,
        mtcs,
        face_ids,
        cfg
      );
    }
  };


  std::unique_ptr<PC2MeshDistCalculatorInterface> createPC2MeshDistCalculator() {
    return std::make_unique<PC2MeshDistCalculator>();
  }
}