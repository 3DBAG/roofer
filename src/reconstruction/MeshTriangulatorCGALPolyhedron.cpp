  
  TriangleCollection triangulate_mesh(const Mesh&) {
    typedef SurfaceMesh::Vertex_index       VertexIndex;
    namespace PMP = CGAL::Polygon_mesh_processing;

    auto gfmesh = input("mesh").get<Mesh>();
    
    SurfaceMesh smesh;
    {
      std::map<arr3f, std::size_t> vertex_map;
      std::set<arr3f> vertex_set;
      std::vector<K::Point_3> points;
      for (const auto &ring : gfmesh.get_polygons())
      {
        for (auto &v : ring)
        {
          auto [it, did_insert] = vertex_set.insert(v);
          if (did_insert)
          {
            vertex_map[v] = points.size();
            points.push_back(K::Point_3(v[0],v[1],v[2]));
          }
        }
      }

      // First build a polygon soup
      std::vector<std::vector<std::size_t> > polygons;
      for (auto& ring : gfmesh.get_polygons()) {
        std::vector<std::size_t> rindices;
        rindices.reserve(ring.size());
        for(auto& p : ring) {
          rindices.push_back(vertex_map[p]);
        }
        polygons.push_back(rindices);
      }

      // Do CGAL mesh repair magic, see https://github.com/CGAL/cgal/issues/7529

      // remove all kinds of typical issues in a polygon soup (degenerate polygons, isolated points, etc.)
      CGAL::Polygon_mesh_processing::repair_polygon_soup(points, polygons);

      // duplicate non-manifold edges (but does not re-orient faces)
      CGAL::Polygon_mesh_processing::duplicate_non_manifold_edges_in_polygon_soup(points, polygons);

      CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(points, polygons, smesh);

      if(!CGAL::is_triangle_mesh(smesh)) PMP::triangulate_faces(smesh);

      // this prevents potentially getting stuck in infinite loop (see https://github.com/CGAL/cgal/issues/7529)
      CGAL::Polygon_mesh_processing::duplicate_non_manifold_vertices( smesh );

      // this is not needed since PMP::repair_polygon_soup() will perform this repair
      // CGAL::Polygon_mesh_processing::remove_isolated_vertices( smesh	);
    }
  }

  void Mesh2TriangleCollectionNode::process() { 
    
    typedef SurfaceMesh::Vertex_index       VertexIndex;
    typedef SurfaceMesh::Face_index         FaceIndex;
    namespace PMP = CGAL::Polygon_mesh_processing;

    auto smesh = input("cgal_surface_mesh").get<SurfaceMesh>();
    
    if(!CGAL::is_triangle_mesh(smesh)) PMP::triangulate_faces(smesh);
    
    // normal computation:
    // https://doc.cgal.org/latest/Polygon_mesh_processing/Polygon_mesh_processing_2compute_normals_example_8cpp-example.html#a4
    // auto vnormals = smesh.add_property_map<VertexIndex, K::Vector_3>("v:normals", CGAL::NULL_VECTOR).first;
    auto fnormals = smesh.add_property_map<FaceIndex, K::Vector_3>("f:normals", CGAL::NULL_VECTOR).first;
    PMP::compute_face_normals(smesh, fnormals);
    // PMP::compute_normals(smesh, vnormals, fnormals);
    // std::cout << "Vertex normals :" << std::endl;
    // for(VertexIndex vd: vertices(smesh))
    //   std::cout << vnormals[vd] << std::endl;
    // std::cout << "Face normals :" << std::endl;
    // for(FaceIndex fd: faces(smesh))
    //   std::cout << fnormals[fd] << std::endl;
    
    TriangleCollection triangleCollection;
    vec3f normals;
    for (auto f : smesh.faces()) {
      Triangle t;
      unsigned i = 0;
      
      for(VertexIndex vi : vertices_around_face(smesh.halfedge(f), smesh)) {
        auto& p = smesh.point(vi);
        t[i++] = arr3f{ 
        (float) p.x(),
        (float) p.y(),
        (float) p.z()
        };
      }
      auto& n = fnormals[f];
      normals.push_back(arr3f{ float(n.x()), float(n.y()), float(n.z()) });
      normals.push_back(arr3f{ float(n.x()), float(n.y()), float(n.z()) });
      normals.push_back(arr3f{ float(n.x()), float(n.y()), float(n.z()) });
      triangleCollection.push_back(t);
    }

    output("triangles").set(triangleCollection);
    output("normals").set(normals);
  }