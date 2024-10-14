// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of geoflow-roofer (https://github.com/3DBAG/geoflow-roofer)

// geoflow-roofer was created as part of the 3DBAG project by the TU Delft 3D
// geoinformation group (3d.bk.tudelf.nl) and 3DGI (3dgi.nl)

// geoflow-roofer is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version. geoflow-roofer is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
// Public License for more details. You should have received a copy of the GNU
// General Public License along with geoflow-roofer. If not, see
// <https://www.gnu.org/licenses/>.

// Author(s):
// Ravi Peters

// val3dity
#include <roofer/misc/Val3dator.hpp>

#include "val3dity.h"

namespace roofer::misc {

  class Val3dator : public Val3datorInterface {
    void compute(const std::unordered_map<int, Mesh>& meshmap,
                 Val3datorConfig cfg) override {
      for (auto& [sid, mesh] : meshmap) {
        // create a vertex list and vertex IDs, taking care of duplicates
        std::map<arr3f, size_t> vertex_map;
        std::vector<arr3f> vertex_vec;

        size_t v_cntr = 0;
        std::set<arr3f> vertex_set;

        auto& faces = mesh.get_polygons();
        for (auto& face : faces) {
          for (auto& vertex : face) {
            auto [it, did_insert] = vertex_set.insert(vertex);
            if (did_insert) {
              vertex_map[vertex] = v_cntr++;
              vertex_vec.push_back(vertex);
            }
          }
          for (auto& iring : face.interior_rings()) {
            for (auto& vertex : iring) {
              auto [it, did_insert] = vertex_set.insert(vertex);
              if (did_insert) {
                vertex_map[vertex] = v_cntr++;
                vertex_vec.push_back(vertex);
              }
            }
          }
        }

        std::vector<std::array<double, 3>> v3_pts;
        v3_pts.reserve(vertex_vec.size());
        for (auto& v : vertex_vec) {
          v3_pts.push_back({v[0], v[1], v[2]});
        }
        //-- add the facets
        std::vector<std::vector<std::vector<int>>> v3_faces_w_holes;
        for (auto& face : faces) {
          std::vector<std::vector<int>> pgnids;
          std::vector<int> ids;
          // ofs << "f";
          for (auto& vertex : face) {
            ids.push_back(vertex_map[vertex]);
            // ofs << " " << vertex_map[vertex];
          }
          // ofs << std::endl;
          pgnids.push_back(ids);
          for (auto& iring : face.interior_rings()) {
            std::vector<int> ids;
            for (auto& vertex : iring) {
              ids.push_back(vertex_map[vertex]);
            }
            pgnids.push_back(ids);
          }
          v3_faces_w_holes.push_back(pgnids);
        }
        auto j_result = val3dity::validate(
            v3_pts, v3_faces_w_holes,
            {._planarity_d2p_tol = cfg.tol_planarity_d2p_,
             ._planarity_n_tol = cfg.tol_planarity_normals_});
        if (j_result.is_object()) {
          errors.push_back(j_result["all_errors"].dump());
        }
      }
    }
  };

  std::unique_ptr<Val3datorInterface> createVal3dator() {
    return std::make_unique<Val3dator>();
  }
}  // namespace roofer::misc
