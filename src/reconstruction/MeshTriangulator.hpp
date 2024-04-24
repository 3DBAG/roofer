#include "../datastructures.hpp"

#include "cgal_shared_definitions.hpp"
#include <memory>

namespace roofer::detection {

  struct MeshTriangulatorConfig{
    int dupe_threshold_exp = 6;
    bool output_all_triangles = false;
    bool output_mtc_for_every_input = false;
  };

  struct MeshTriangulatorInterface {
      TriangleCollection triangles;
      MultiTriangleCollection multitrianglecol;
      vec3f normals;
      vec1i ring_ids;
      vec1f volumes;
    // add_output("triangles_og", typeid(TriangleCollection));
    // add_output("segment_ids_og", typeid(vec1i));
    // add_output("triangles_snapped", typeid(TriangleCollection));
    // add_output("segment_ids_snapped", typeid(vec1i));

    virtual ~MeshTriangulatorInterface() = default;
    virtual void compute(
      const std::vector<Mesh>& meshes,
      MeshTriangulatorConfig config=MeshTriangulatorConfig()
    ) = 0;
    virtual void compute(
      const std::vector<LinearRing>& polygons,
      MeshTriangulatorConfig config=MeshTriangulatorConfig()
    ) = 0;
    virtual void compute(
      const std::unordered_map<int, Mesh>& multisolid, 
      MeshTriangulatorConfig config=MeshTriangulatorConfig()
    ) = 0;
    
  };

  std::unique_ptr<MeshTriangulatorInterface> createMeshTriangulatorLegacy();
//   std::unique_ptr<MeshTriangulatorInterface> createMeshTriangulatorCGALPolyhedron();
}