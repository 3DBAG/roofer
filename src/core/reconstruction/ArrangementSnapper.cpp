// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of roofer (https://github.com/3DBAG/roofer)

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

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>

#include <roofer/reconstruction/ArrangementBase.hpp>
#include <roofer/reconstruction/ArrangementSnapper.hpp>
#include <roofer/reconstruction/cdt_util.hpp>
// #include <CGAL/Constrained_triangulation_2.h>
#include <CGAL/Arr_walk_along_line_point_location.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_vertex_base_with_info_2.h>

#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <unordered_set>

namespace roofer::reconstruction {

  namespace arragementsnapper {

    typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
    typedef CGAL::Exact_predicates_inexact_constructions_kernel Epeck;
    typedef CGAL::Exact_predicates_tag Tag;

    typedef CGAL::Triangulation_vertex_base_2<K> VertexBase;
    typedef CGAL::Triangulation_vertex_base_with_info_2<bool, K, VertexBase>
        VertexBaseWithInfo;
    typedef CGAL::Constrained_triangulation_face_base_2<K> FaceBase;
    typedef CGAL::Triangulation_data_structure_2<VertexBaseWithInfo, FaceBase>
        TriangulationDataStructure;
    typedef CGAL::Constrained_Delaunay_triangulation_2<
        K, TriangulationDataStructure, Tag>
        T;
    typedef T::Edge_circulator Edge_circulator;
    typedef T::Face_circulator Face_circulator;
    typedef T::Finite_faces_iterator Finite_faces_iterator;
    typedef T::Finite_edges_iterator Finite_edges_iterator;
    typedef T::Vertex_handle Vertex_handle;
    typedef T::Face_handle Face_handle;
    typedef std::pair<Face_handle, int> Edge;

    typedef std::unordered_set<Vertex_handle> ConstraintSet;

    struct ForcedRegionLabel {
      T::Point_2 seed;
      FaceInfo* face_info;
    };

    using SourceFaceIds = std::unordered_map<FaceInfo*, std::size_t>;

    struct IncidentSector {
      Vertex_handle neighbour;
      FaceInfo* face_info;
      double angle;
      double height;
    };

    struct RepairCandidate {
      Vertex_handle vertex;
      std::vector<IncidentSector> sectors;
      double clearance;
      FaceInfo* preferred_merge_face = nullptr;
    };

    double plane_height(const FaceInfo& face_info, const T::Point_2& point) {
      const auto& plane = face_info.plane;
      if (plane.c() == 0) return 0;
      return -(plane.a() * point.x() + plane.b() * point.y() + plane.d()) /
             plane.c();
    }

    bool is_roof_face(const FaceInfo* face_info) {
      return face_info != nullptr && face_info->in_footprint &&
             face_info->segid != 0;
    }

    FaceInfo* source_face_at(
        const T::Point_2& point,
        CGAL::Arr_walk_along_line_point_location<Arrangement_2>& walk_pl,
        Arrangement_2& source_arrangement) {
      auto object =
          walk_pl.locate(Arrangement_2::Point_2(point.x(), point.y()));
      if (auto face = std::get_if<Face_const_handle>(&object)) {
        return &source_arrangement.non_const_handle(*face)->data();
      }
      return nullptr;
    }

    // region grows constrained regions of triangles
    std::vector<std::vector<Face_handle>> constrained_regions(T& tri) {
      std::vector<std::vector<Face_handle>> regions;
      std::unordered_set<Face_handle> visited;

      for (auto face : tri.finite_face_handles()) {
        if (visited.contains(face)) continue;

        std::vector<Face_handle> region;
        std::queue<Face_handle> queue;
        queue.push(face);
        visited.insert(face);
        while (!queue.empty()) {
          auto current = queue.front();
          queue.pop();
          region.push_back(current);

          for (int i = 0; i < 3; ++i) {
            Edge edge(current, i);
            auto neighbour = current->neighbor(i);
            if (!tri.is_constrained(edge) && !tri.is_infinite(neighbour) &&
                visited.insert(neighbour).second) {
              queue.push(neighbour);
            }
          }
        }
        regions.push_back(std::move(region));
      }
      return regions;
    }

    using ForcedFaceLabels = std::vector<std::pair<Face_handle, FaceInfo*>>;
    using TriangleFaceLabels = std::unordered_map<Face_handle, FaceInfo*>;

    struct LabelledRegion {
      T::Point_2 sample;
      FaceInfo* label;
      double area;
    };

    struct RegionLabelling {
      std::vector<LabelledRegion> regions;
      TriangleFaceLabels face_labels;
    };

    ForcedFaceLabels locate_forced_labels(
        T& tri, const std::vector<ForcedRegionLabel>& forced_labels) {
      ForcedFaceLabels forced_face_labels;
      for (const auto& forced : forced_labels) {
        auto located = tri.locate(forced.seed);
        if (!tri.is_infinite(located)) {
          forced_face_labels.emplace_back(located, forced.face_info);
        }
      }
      return forced_face_labels;
    }

    FaceInfo* select_region_label(
        T& tri, const std::vector<Face_handle>& region,
        CGAL::Arr_walk_along_line_point_location<Arrangement_2>& walk_pl,
        Arrangement_2& source_arrangement, const SourceFaceIds& source_ids,
        const ForcedFaceLabels& forced_face_labels) {
      // For this triangle region collect total overlap area for each source
      // arrangement face.
      std::unordered_map<FaceInfo*, double> overlap_area;
      for (auto face : region) {
        auto centroid = CGAL::centroid(tri.triangle(face));
        if (auto* source =
                source_face_at(centroid, walk_pl, source_arrangement)) {
          overlap_area[source] += std::abs(tri.triangle(face).area());
        }
      }

      // Select the arrangement face with the largest overlap area.
      FaceInfo* selected = nullptr;
      double selected_area = -1;
      std::size_t selected_id = std::numeric_limits<std::size_t>::max();
      for (const auto& [source, area] : overlap_area) {
        auto id = source_ids.at(source);
        if (area > selected_area ||
            (area == selected_area && id < selected_id)) {
          selected = source;
          selected_area = area;
          selected_id = id;
        }
      }

      // Apply the forced regions determined by non-manifold repairs.
      for (const auto& [forced_face, forced_label] : forced_face_labels) {
        if (std::find(region.begin(), region.end(), forced_face) !=
            region.end()) {
          selected = forced_label;
          break;
        }
      }
      return selected;
    }

    RegionLabelling compute_region_labelling(
        T& tri,
        CGAL::Arr_walk_along_line_point_location<Arrangement_2>& walk_pl,
        Arrangement_2& source_arrangement, const SourceFaceIds& source_ids,
        const std::vector<ForcedRegionLabel>& forced_labels) {
      RegionLabelling labelling;
      auto forced_face_labels = locate_forced_labels(tri, forced_labels);
      auto regions = constrained_regions(tri);
      for (const auto& region : regions) {
        auto* label =
            select_region_label(tri, region, walk_pl, source_arrangement,
                                source_ids, forced_face_labels);
        if (label == nullptr) continue;

        double area = 0;
        for (auto face : region) {
          labelling.face_labels[face] = label;
          area += std::abs(tri.triangle(face).area());
        }
        labelling.regions.push_back(
            {CGAL::centroid(tri.triangle(region.front())), label, area});
      }
      for (auto face : tri.all_face_handles()) {
        if (tri.is_infinite(face)) {
          labelling.face_labels[face] =
              &source_arrangement.unbounded_face()->data();
        }
      }
      return labelling;
    }

    double squared_distance_to_face(T& tri, Face_handle face,
                                    const T::Point_2& point) {
      auto triangle = tri.triangle(face);
      if (triangle.has_on_boundary(point) ||
          triangle.has_on_bounded_side(point)) {
        return 0;
      }

      double distance_sq = std::numeric_limits<double>::max();
      for (int i = 0; i < 3; ++i) {
        distance_sq =
            std::min(distance_sq, CGAL::to_double(CGAL::squared_distance(
                                      point, tri.segment({face, i}))));
      }
      return distance_sq;
    }

    std::vector<Face_handle> get_repair_region(T& tri, Vertex_handle vertex,
                                               double radius) {
      std::vector<Face_handle> region;
      if (!(radius > 0)) return region;

      const double radius_sq = radius * radius;
      const double tolerance = std::max(1.0, radius_sq) * 1e-12;
      std::unordered_set<Face_handle> visited;
      std::queue<Face_handle> queue;

      auto push_face = [&](Face_handle face) {
        if (tri.is_infinite(face)) return;
        if (visited.contains(face)) return;
        if (squared_distance_to_face(tri, face, vertex->point()) >
            radius_sq + tolerance) {
          return;
        }
        visited.insert(face);
        queue.push(face);
      };

      Face_circulator face = tri.incident_faces(vertex), done(face);
      if (face == nullptr) return region;
      do {
        push_face(face);
      } while (++face != done);

      while (!queue.empty()) {
        auto current = queue.front();
        queue.pop();
        region.push_back(current);

        for (int i = 0; i < 3; ++i) {
          push_face(current->neighbor(i));
        }
      }
      return region;
    }

    bool edge_is_incident_to_vertex(T& tri, const Edge& edge,
                                    Vertex_handle vertex) {
      return edge.first->vertex(tri.cw(edge.second)) == vertex ||
             edge.first->vertex(tri.ccw(edge.second)) == vertex;
    }

    // Compute the real local clearance up to the configured repair radius:
    // grow over all triangulation faces touched by that radius, then measure
    // only constrained edges from the grown patch.
    std::optional<double> calculate_vertex_clearance(T& tri,
                                                     Vertex_handle vertex,
                                                     double repair_radius) {
      if (!(repair_radius > 0)) return std::nullopt;

      const double repair_radius_sq = repair_radius * repair_radius;
      const double tolerance = std::max(1.0, repair_radius_sq) * 1e-12;
      double clearance_sq = std::numeric_limits<double>::max();
      for (auto face : get_repair_region(tri, vertex, repair_radius)) {
        for (int i = 0; i < 3; ++i) {
          Edge edge(face, i);
          if (!tri.is_constrained(edge)) continue;
          if (edge_is_incident_to_vertex(tri, edge, vertex)) continue;

          const double distance_sq = CGAL::to_double(
              CGAL::squared_distance(vertex->point(), tri.segment(edge)));
          if (distance_sq <= repair_radius_sq + tolerance) {
            clearance_sq = std::min(clearance_sq, distance_sq);
          }
        }
      }

      if (clearance_sq == std::numeric_limits<double>::max()) {
        // No constrained edge was found inside the search disk, so clearance
        // should not reduce the configured maximum repair radius.
        return std::numeric_limits<double>::infinity();
      }
      if (!(clearance_sq > 0)) return std::nullopt;
      return std::sqrt(clearance_sq);
    }

    std::size_t constrained_incident_edge_count(T& tri, Vertex_handle vertex) {
      std::size_t count = 0;
      Edge_circulator edge = tri.incident_edges(vertex), done(edge);
      if (edge == nullptr) return count;
      do {
        if (tri.is_constrained(*edge)) ++count;
      } while (++edge != done);
      return count;
    }

    void remove_dangling_constraints_and_vertices(T& tri) {
      // Build the constraint graph and peel its degree-one vertices. Updating
      // the graph as edges are peeled also detects constraint chains that only
      // become dangling after their outermost edge has been removed.
      std::unordered_map<Vertex_handle, ConstraintSet> adjacency;
      for (const auto& edge : tri.constrained_edges()) {
        auto first = edge.first->vertex(tri.cw(edge.second));
        auto second = edge.first->vertex(tri.ccw(edge.second));
        adjacency[first].insert(second);
        adjacency[second].insert(first);
      }

      std::queue<Vertex_handle> leaves;
      for (const auto& [vertex, neighbours] : adjacency) {
        if (neighbours.size() == 1) leaves.push(vertex);
      }

      while (!leaves.empty()) {
        auto vertex = leaves.front();
        leaves.pop();

        auto& neighbours = adjacency[vertex];
        if (neighbours.size() != 1) continue;

        auto neighbour = *neighbours.begin();
        neighbours.clear();

        auto& neighbour_adjacency = adjacency[neighbour];
        neighbour_adjacency.erase(vertex);
        if (neighbour_adjacency.size() == 1) leaves.push(neighbour);

        // Locate the edge from its stable endpoint handles instead of retaining
        // a face handle across triangulation updates.
        Face_handle face;
        int index;
        if (tri.is_edge(vertex, neighbour, face, index) &&
            tri.is_constrained({face, index})) {
          tri.remove_constrained_edge(face, index);
        }
      }

      std::vector<Vertex_handle> vertices_to_remove;
      for (auto vertex = tri.finite_vertices_begin();
           vertex != tri.finite_vertices_end(); ++vertex) {
        if (!tri.are_there_incident_constraints(vertex)) {
          vertices_to_remove.push_back(vertex);
        }
      }
      for (auto vertex : vertices_to_remove) tri.remove(vertex);
    }

    // Calculate azimuth for each incident triangulation edge and sample the
    // corresponding intermediate region label.
    std::vector<IncidentSector> incident_sectors(
        T& tri, Vertex_handle vertex, double clearance,
        const TriangleFaceLabels& face_labels) {
      std::vector<IncidentSector> sectors;
      Edge_circulator edge = tri.incident_edges(vertex), done(edge);
      if (edge == nullptr) return sectors;
      do {
        if (!tri.is_constrained(*edge)) continue;
        auto first = edge->first->vertex(tri.cw(edge->second));
        auto second = edge->first->vertex(tri.ccw(edge->second));
        auto neighbour = first == vertex ? second : first;
        const double dx = neighbour->point().x() - vertex->point().x();
        const double dy = neighbour->point().y() - vertex->point().y();
        sectors.push_back({neighbour, nullptr, std::atan2(dy, dx), 0});
      } while (++edge != done);

      std::sort(sectors.begin(), sectors.end(),
                [](const auto& lhs, const auto& rhs) {
                  return lhs.angle < rhs.angle;
                });
      if (sectors.size() < 2) return sectors;

      const double sample_radius = clearance * 0.25;
      constexpr double two_pi = 2 * CGAL_PI;
      for (std::size_t i = 0; i < sectors.size(); ++i) {
        double next_angle = sectors[(i + 1) % sectors.size()].angle;
        if (next_angle <= sectors[i].angle) next_angle += two_pi;
        const double sample_angle = (sectors[i].angle + next_angle) * 0.5;
        T::Point_2 sample(
            vertex->point().x() + sample_radius * std::cos(sample_angle),
            vertex->point().y() + sample_radius * std::sin(sample_angle));
        auto face = tri.locate(sample);
        auto label = face_labels.find(face);
        if (label == face_labels.end() || label->second == nullptr) return {};
        auto* face_info = label->second;
        sectors[i].face_info = face_info;
        sectors[i].height = plane_height(*face_info, vertex->point());
      }
      return sectors;
    }

    bool has_non_manifold_height_order(
        const std::vector<IncidentSector>& sectors, double tolerance) {
      std::vector<double> heights;
      heights.reserve(sectors.size());
      for (const auto& sector : sectors) heights.push_back(sector.height);
      std::sort(heights.begin(), heights.end());

      std::vector<double> distinct_heights;
      for (double height : heights) {
        if (distinct_heights.empty() ||
            height - distinct_heights.back() >= tolerance) {
          distinct_heights.push_back(height);
        }
      }

      for (std::size_t level = 1; level < distinct_heights.size(); ++level) {
        const double sample_height =
            (distinct_heights[level - 1] + distinct_heights[level]) * 0.5;
        std::size_t crossings = 0;
        for (std::size_t i = 0; i < sectors.size(); ++i) {
          const double lhs =
              sectors[(i + sectors.size() - 1) % sectors.size()].height;
          const double rhs = sectors[i].height;
          if ((lhs < sample_height && rhs > sample_height) ||
              (rhs < sample_height && lhs > sample_height)) {
            ++crossings;
          }
        }
        if (crossings > 2) return true;
      }
      return false;
    }

    std::size_t cyclic_face_run_count(
        const std::vector<IncidentSector>& sectors, const FaceInfo* face_info) {
      if (sectors.empty()) return 0;
      if (std::all_of(sectors.begin(), sectors.end(),
                      [face_info](const auto& sector) {
                        return sector.face_info == face_info;
                      })) {
        return 1;
      }

      std::size_t runs = 0;
      for (std::size_t i = 0; i < sectors.size(); ++i) {
        const auto previous = (i + sectors.size() - 1) % sectors.size();
        if (sectors[i].face_info == face_info &&
            sectors[previous].face_info != face_info) {
          ++runs;
        }
      }
      return runs;
    }

    FaceInfo* repeated_incident_face(const std::vector<IncidentSector>& sectors,
                                     const SourceFaceIds& source_ids) {
      std::unordered_map<FaceInfo*, std::size_t> counts;
      for (const auto& sector : sectors) {
        if (is_roof_face(sector.face_info)) ++counts[sector.face_info];
      }

      FaceInfo* selected = nullptr;
      std::size_t selected_count = 0;
      std::size_t selected_id = std::numeric_limits<std::size_t>::max();
      for (const auto& [face_info, count] : counts) {
        if (count < 2) continue;
        if (cyclic_face_run_count(sectors, face_info) < 2) continue;
        auto id = source_ids.at(face_info);
        if (count > selected_count ||
            (count == selected_count && id < selected_id)) {
          selected = face_info;
          selected_count = count;
          selected_id = id;
        }
      }
      return selected;
    }

    std::optional<RepairCandidate> find_problematic_vertex(
        T& tri, const TriangleFaceLabels& face_labels,
        const SourceFaceIds& source_ids, double repair_radius,
        double height_tolerance) {
      for (auto vertex = tri.finite_vertices_begin();
           vertex != tri.finite_vertices_end(); ++vertex) {
        if (constrained_incident_edge_count(tri, vertex) < 4) continue;
        auto clearance = calculate_vertex_clearance(tri, vertex, repair_radius);
        if (!clearance) continue;
        if (*clearance < repair_radius / 2) continue;
        const double sector_clearance =
            std::isfinite(*clearance) ? *clearance : repair_radius;
        auto sectors =
            incident_sectors(tri, vertex, sector_clearance, face_labels);
        if (sectors.size() < 4) continue;
        if (std::none_of(sectors.begin(), sectors.end(),
                         [](const auto& sector) {
                           return is_roof_face(sector.face_info);
                         })) {
          continue;
        }
        // we check repeated incident faces for self intersection at the vertex
        auto repeated_face = repeated_incident_face(sectors, source_ids);
        if (repeated_face ||
            has_non_manifold_height_order(sectors, height_tolerance)) {
          return RepairCandidate{vertex, std::move(sectors), *clearance,
                                 repeated_face};
        }
      }
      return std::nullopt;
    }

    ForcedRegionLabel repair_vertex(T& tri, const RepairCandidate& candidate,
                                    const SourceFaceIds& source_ids,
                                    double maximum_radius,
                                    double height_tolerance) {
      const auto& sectors = candidate.sectors;
      auto vertex = candidate.vertex;
      if (sectors.size() < 4) {
        throw roofer::rooferException(
            "Unable to compute a safe non-manifold junction repair");
      }

      FaceInfo* selected_face = candidate.preferred_merge_face;
      if (selected_face == nullptr) {
        double selected_height = std::numeric_limits<double>::infinity();
        std::size_t selected_id = std::numeric_limits<std::size_t>::max();

        for (const auto& sector : sectors) {
          if (!is_roof_face(sector.face_info)) continue;

          auto id = source_ids.at(sector.face_info);
          if (sector.height < selected_height ||
              (sector.height == selected_height && id < selected_id)) {
            selected_face = sector.face_info;
            selected_height = sector.height;
            selected_id = id;
          }
        }
        if (selected_face == nullptr) {
          throw roofer::rooferException(
              "Non-manifold junction repair has no roof face to merge into");
        }
      }

      double shortest_edge = std::numeric_limits<double>::max();
      for (const auto& sector : sectors) {
        shortest_edge = std::min(
            shortest_edge, std::sqrt(CGAL::to_double(CGAL::squared_distance(
                               vertex->point(), sector.neighbour->point()))));
      }
      const double radius = std::min(
          {maximum_radius, candidate.clearance * 0.8, shortest_edge * 0.45});
      if (!(radius > std::numeric_limits<double>::epsilon())) {
        throw roofer::rooferException(
            "Non-manifold junction repair radius is degenerate");
      }

      // insert split vertices along each incident edge
      const auto original_point = vertex->point();
      std::vector<Vertex_handle> split_vertices;
      split_vertices.reserve(sectors.size());
      for (const auto& sector : sectors) {
        const double dx = sector.neighbour->point().x() - original_point.x();
        const double dy = sector.neighbour->point().y() - original_point.y();
        const double length = std::sqrt(dx * dx + dy * dy);
        T::Point_2 split_point(original_point.x() + radius * dx / length,
                               original_point.y() + radius * dy / length);
        auto split_vertex = tri.insert(split_point);
        split_vertex->info() = false;
        split_vertices.push_back(split_vertex);
      }

      // remove incident constraints and out main vertex
      tri.remove_incident_constraints(vertex);
      tri.remove(vertex);
      for (std::size_t i = 0; i < sectors.size(); ++i) {
        tri.insert_constraint(split_vertices[i], sectors[i].neighbour);
      }

      //
      std::optional<T::Point_2> forced_seed;
      for (std::size_t i = 0; i < sectors.size(); ++i) {
        auto next = (i + 1) % sectors.size();
        if (sectors[i].face_info == selected_face) {
          if (!forced_seed) {
            forced_seed = T::Point_2(
                (2 * original_point.x() + split_vertices[i]->point().x() +
                 split_vertices[next]->point().x()) /
                    4,
                (2 * original_point.y() + split_vertices[i]->point().y() +
                 split_vertices[next]->point().y()) /
                    4);
          }
          continue;
        }
        tri.insert_constraint(split_vertices[i], split_vertices[next]);
      }

      if (!forced_seed) {
        throw roofer::rooferException(
            "Non-manifold junction repair has no merge sector");
      }
      return {*forced_seed, selected_face};
    }

    void get_incident_constraints(T& tri, Vertex_handle vthis,
                                  Vertex_handle vexcept1,
                                  Vertex_handle vexcept2,
                                  ConstraintSet& constraints_to_restore) {
      // std::cout << "vthis degree=" << tri.degree(vthis) << std::endl;
      Edge_circulator ec = tri.incident_edges(vthis), done(ec);
      if (ec != nullptr) {
        do {
          if (tri.is_constrained(*ec)) {
            auto va = ec->first->vertex(tri.cw(ec->second));
            auto vb = ec->first->vertex(tri.ccw(ec->second));
            Vertex_handle vother;
            if (va == vthis) {
              vother = vb;
            } else {
              vother = va;
            };

            if (vother != vexcept1 && vother != vexcept2) {
              constraints_to_restore.insert(vother);
            }
          }
        } while (++ec != done);
      }
    }

    void restore_constraints(T& tri, T::Point_2& pnew,
                             ConstraintSet& constraints_to_restore) {
      auto vnew = tri.insert(pnew);

      // restore constraints
      // std::cout << "restoring " << constraints_to_restore.size() << "
      // constraints\n";
      for (auto& vh : constraints_to_restore) {
        // std::cout << "reinsert constrained " << *vnew << " - " << *vh <<
        // std::endl;
        tri.insert_constraint(vnew, vh);
      }
    }

    class ArrangementSnapper : public ArrangementSnapperInterface {
      void compute(Arrangement_2& arr, ArrangementSnapperConfig cfg) override {
        typedef CGAL::Arr_walk_along_line_point_location<Arrangement_2> Walk_pl;

        T tri;
        float sq_dist_thres = cfg.dist_thres * cfg.dist_thres;

        SourceFaceIds source_face_ids;
        std::size_t next_source_face_id = 0;
        for (auto face : arr.face_handles()) {
          source_face_ids[&face->data()] = next_source_face_id++;
        }

        // map from arr vertices to tri vertices
        std::unordered_map<Arrangement_2::Vertex_handle, T::Vertex_handle>
            vertex_map;

        // Segment_list_2 seg_list;
        // Polyline_list_2 output_list;
        for (auto arrVertex : arr.vertex_handles()) {
          // auto& p = v->point();

          // check if this vertex is on the footprint
          Arrangement_2::Halfedge_around_vertex_circulator
              ec = arrVertex->incident_halfedges(),
              done(ec);
          bool has_ext_face(false), has_int_face(false);
          if (ec != nullptr) {
            do {
              has_ext_face = !ec->face()->data().in_footprint || has_ext_face;
              has_int_face = ec->face()->data().in_footprint || has_int_face;
            } while (++ec != done);
          }

          auto vtri =
              tri.insert(T::Point_2(CGAL::to_double(arrVertex->point().x()),
                                    CGAL::to_double(arrVertex->point().y())));
          vtri->info() = has_ext_face && has_int_face;
          vertex_map[arrVertex] = vtri;
        }

        Walk_pl walk_pl(arr);
        for (auto& arrEdge : arr.edge_handles()) {
          if (vertex_map[arrEdge->source()] != vertex_map[arrEdge->target()]) {
            tri.insert_constraint(vertex_map[arrEdge->source()],
                                  vertex_map[arrEdge->target()]);
          }
        }

        // Detect triangles with 3 short edges => collapse triangle to point
        // (remove 2 vertices)
        bool found_small_face;
        do {
          found_small_face = false;
          for (Finite_faces_iterator fit = tri.finite_faces_begin();
               fit != tri.finite_faces_end(); ++fit) {
            auto v0 = fit->vertex(0);
            auto v1 = fit->vertex(1);
            auto v2 = fit->vertex(2);
            auto& p0 = v0->point();
            auto& p1 = v1->point();
            auto& p2 = v2->point();
            // auto e0 = std::make_pair(fit, 0);
            // auto e1 = std::make_pair(fit, 1);
            // auto e2 = std::make_pair(fit, 2);
            // do not collapse if all vertices are on footprint boundary
            // if (
            //   v1->info() && v2->info() && v0->info()
            // )
            // continue;
            if ((CGAL::squared_distance(p0, p1) < sq_dist_thres &&
                 CGAL::squared_distance(p1, p2) < sq_dist_thres &&
                 CGAL::squared_distance(p2, p0) < sq_dist_thres)  //&& (
                //   tri.is_constrained( e0 ) &&
                //   tri.is_constrained( e1 ) &&
                //   tri.is_constrained( e2 )
                // )
            ) {
              // std::cout << "small triangle between " << p0 << " and " << p1
              // << "  and  " << p2 << std::endl;
              ConstraintSet constraints_to_restore;

              // collect incident constraint edges
              for (size_t i = 0; i < 3; ++i) {
                auto vthis = fit->vertex(i);
                auto vexcept1 = fit->vertex(tri.cw(i));
                auto vexcept2 = fit->vertex(tri.ccw(i));
                get_incident_constraints(tri, vthis, vexcept1, vexcept2,
                                         constraints_to_restore);
              }

              // but first check points for being on the footprint boundary
              T::Point_2 pnew;
              if (v0->info()) {
                pnew = p0;
              } else if (v1->info()) {
                pnew = p1;
              } else if (v2->info()) {
                pnew = p2;
              } else {
                pnew = CGAL::centroid(p0, p1, p2);
              }
              // collapse the triangle to centroid
              tri.remove_incident_constraints(v0);
              tri.remove(v0);
              tri.remove_incident_constraints(v1);
              tri.remove(v1);
              tri.remove_incident_constraints(v2);
              tri.remove(v2);

              restore_constraints(tri, pnew, constraints_to_restore);

              found_small_face = true;
              break;  // we need to restart the loop because we may have
                      // invalidated the finite faces iterator by modifying the
                      // faces of the triangulation
            }
          }

        } while (found_small_face);

        // Detect triangles with 1 short edge  => collapse the short edge to
        // point (remove one vertex)
        bool found_short_edge;
        do {
          found_short_edge = false;
          for (Finite_edges_iterator ceit = tri.finite_edges_begin();
               ceit != tri.finite_edges_end(); ++ceit) {
            if (tri.is_constrained(*ceit)) {
              auto v1 = ceit->first->vertex(tri.cw(ceit->second));
              auto v2 = ceit->first->vertex(tri.ccw(ceit->second));
              auto& p1 = v1->point();
              auto& p2 = v2->point();

              // do not collapse if this edge is on the footprint boundary
              // if (v1->info() && v2->info()) continue;

              if (CGAL::squared_distance(p1, p2) < sq_dist_thres) {
                // std::cout << "short edge between " << p1 << "  and  " << p2
                // << std::endl;

                // auto vi = ceit->second;
                ConstraintSet constraints_to_restore;
                get_incident_constraints(tri, v1, v1, v2,
                                         constraints_to_restore);
                get_incident_constraints(tri, v2, v1, v2,
                                         constraints_to_restore);

                // insert midpoint and restore constraints
                // first check points for being on the footprint boundary
                T::Point_2 pnew;
                if (v1->info()) {
                  pnew = p1;
                } else if (v2->info()) {
                  pnew = p2;
                } else {
                  pnew = CGAL::midpoint(p1, p2);
                }

                // remove edge
                tri.remove_incident_constraints(v1);
                tri.remove(v1);
                tri.remove_incident_constraints(v2);
                tri.remove(v2);

                restore_constraints(tri, pnew, constraints_to_restore);

                found_short_edge = true;
                break;
              }
            }
          }
        } while (found_short_edge);

        // Detect triangles with 1 vertex close to opposing (longest) edge  =>
        // remove long edge as constraint and ensure both short ones are
        // constrained
        // TODO: move the vertex opposed to long edge to be on the long edge ??
        // TODO: detect if an edge of the triangle is on the footprint boundary

        for (Finite_faces_iterator fit = tri.finite_faces_begin();
             fit != tri.finite_faces_end(); ++fit) {
          auto v0 = fit->vertex(0);
          auto v1 = fit->vertex(1);
          auto v2 = fit->vertex(2);
          auto& p0 = v0->point();
          auto& p1 = v1->point();
          auto& p2 = v2->point();
          auto e0 = std::make_pair(fit, 0);
          auto e1 = std::make_pair(fit, 1);
          auto e2 = std::make_pair(fit, 2);
          auto s0 = tri.segment(e0);
          auto s1 = tri.segment(e1);
          auto s2 = tri.segment(e2);
          if ((CGAL::squared_distance(s0, p0) < sq_dist_thres) &&
              tri.is_constrained(e0)) {
            if (tri.is_constrained(e1) || tri.is_constrained(e2)) {
              // std::cout << "flat triangle between " << s0 << " and " << p0 <<
              // std::endl;
              tri.remove_constrained_edge(fit, 0);
              if (!tri.is_constrained(e2)) tri.insert_constraint(v0, v1);
              if (!tri.is_constrained(e1)) tri.insert_constraint(v0, v2);
            }
          } else if ((CGAL::squared_distance(s1, p1) < sq_dist_thres) &&
                     tri.is_constrained(e1)) {
            if (tri.is_constrained(e0) || tri.is_constrained(e2)) {
              // std::cout << "flat triangle between " << s1 << " and " << p1 <<
              // std::endl;
              tri.remove_constrained_edge(fit, 1);
              if (!tri.is_constrained(e2)) tri.insert_constraint(v1, v0);
              if (!tri.is_constrained(e0)) tri.insert_constraint(v1, v2);
            }
          } else if ((CGAL::squared_distance(s2, p2) < sq_dist_thres) &&
                     tri.is_constrained(e2)) {
            if (tri.is_constrained(e0) || tri.is_constrained(e1)) {
              // std::cout << "flat triangle between " << s2 << " and " << p2 <<
              // std::endl;
              tri.remove_constrained_edge(fit, 2);
              if (!tri.is_constrained(e1)) tri.insert_constraint(v2, v0);
              if (!tri.is_constrained(e0)) tri.insert_constraint(v2, v1);
            }
          }
        }

        // Remove dangling constraint trees and the unconstrained vertices left
        // behind by snapping.
        remove_dangling_constraints_and_vertices(tri);

        // Detect and repair non-manifold vertices (ie. leading to a
        // non-manifold edge during extrusion) and self-intersecting faces.
        // Recompute intermediate labels after every repair so detection uses
        // the same region labels that will be transferred to the arrangement.
        std::vector<ForcedRegionLabel> forced_region_labels;
        RegionLabelling intermediate_labelling;
        while (true) {
          intermediate_labelling = compute_region_labelling(
              tri, walk_pl, arr, source_face_ids, forced_region_labels);
          if (!cfg.repair_non_manifold_vertices) break;

          auto candidate = find_problematic_vertex(
              tri, intermediate_labelling.face_labels, source_face_ids,
              cfg.manifold_repair_radius, cfg.manifold_height_tolerance);
          if (!candidate) break;
          forced_region_labels.push_back(repair_vertex(
              tri, *candidate, source_face_ids, cfg.manifold_repair_radius,
              cfg.manifold_height_tolerance));
        }

        // convert back from triangulation to arrangement
        // 1 recreate vertices and faces
        Arrangement_2 arr_snap;
        std::unordered_map<T::Vertex_handle, Arrangement_2::Vertex_handle>
            vertex2arr_map;

        for (auto vh = tri.finite_vertices_begin();
             vh != tri.finite_vertices_end(); ++vh) {
          // make sure not to add isolated vertices
          if (tri.are_there_incident_constraints(vh)) {
            vertex2arr_map[vh] = insert_point(
                arr_snap,
                Arrangement_2::Point_2(vh->point().x(), vh->point().y()));
          }
        }

        for (auto ce : tri.constrained_edges()) {
          auto v1 = ce.first->vertex(tri.cw(ce.second));
          auto v2 = ce.first->vertex(tri.ccw(ce.second));
          auto& p1_ = v1->point();
          auto& p2_ = v2->point();
          auto p1 = Arrangement_2::Point_2(p1_.x(), p1_.y());
          auto p2 = Arrangement_2::Point_2(p2_.x(), p2_.y());

          // std::cout << p1 << "  --  " << p2 << std::endl;

          // if (vertex2arr_map[v1] != vertex2arr_map[v2]) {
          arr_snap.insert_at_vertices(Segment_2(p1, p2), vertex2arr_map[v1],
                                      vertex2arr_map[v2]);
          // } else {
          //   std::cout << "skipping edge between same vertex\n";
          // }
        }

        // 2 transfer labels from triangulation to new arrangement
        typedef CGAL::Arr_walk_along_line_point_location<Arrangement_2>
            Snap_walk_pl;
        Snap_walk_pl snap_walk_pl(arr_snap);
        std::unordered_map<Arrangement_2::Face_handle,
                           std::unordered_map<FaceInfo*, double>>
            output_face_labels;
        for (const auto& region : intermediate_labelling.regions) {
          if (!(region.area > 0)) continue;

          auto object = snap_walk_pl.locate(
              Arrangement_2::Point_2(region.sample.x(), region.sample.y()));
          if (auto located_face = std::get_if<Face_const_handle>(&object)) {
            auto output_face = arr_snap.non_const_handle(*located_face);
            if (output_face->is_unbounded()) continue;
            output_face_labels[output_face][region.label] += region.area;
          }
        }

        arr_snap.unbounded_face()->data() = arr.unbounded_face()->data();
        for (auto output_face : arr_snap.face_handles()) {
          if (output_face->is_unbounded()) continue;
          const auto labels = output_face_labels.find(output_face);
          if (labels == output_face_labels.end() || labels->second.empty()) {
            throw roofer::rooferException(
                "Unable to transfer snapped arrangement face label");
          }

          auto best =
              std::max_element(labels->second.begin(), labels->second.end(),
                               [&](const auto& lhs, const auto& rhs) {
                                 if (lhs.second != rhs.second)
                                   return lhs.second < rhs.second;
                                 return source_face_ids.at(lhs.first) >
                                        source_face_ids.at(rhs.first);
                               });
          output_face->data() = *best->first;
        }

        // This should not be necessary, unless there is the above code still
        // produces dangling edges (which it shoudldnt)
        // // remove dangling edges if any, eg holes that collapse to a single
        // edge
        // // after snapping
        // {
        //   std::vector<Arrangement_2::Halfedge_handle> to_remove;
        //   for (auto he : arr_snap.edge_handles()) {
        //     if (he->face() == he->twin()->face()) to_remove.push_back(he);
        //   }
        //   for (auto he : to_remove) {
        //     arr_snap.remove_edge(he);
        //   }
        // }

        arr = arr_snap;
      }
    };

  }  // namespace arragementsnapper

  std::unique_ptr<ArrangementSnapperInterface> createArrangementSnapper() {
    return std::make_unique<arragementsnapper::ArrangementSnapper>();
  }

}  // namespace roofer::reconstruction
