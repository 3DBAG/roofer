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
#include "ArrangementBase.hpp"
#include <stack>

namespace roofer::detection {

template<typename E, typename P> bool ccb_to_polygon_3(E he, P& polygon, double h=0) {
  auto first = he;

  while(true){
    if (he->is_fictitious()) return false;
      polygon.push_back({
        float(CGAL::to_double(he->source()->point().x())),
        float(CGAL::to_double(he->source()->point().y())),
        float(h)
      });

    he = he->next();
    if (he==first) break;
  // }
  }
  return true;
}
void arrangementface_to_polygon(Face_handle face, vec2f& polygons){
  // if(extract_face){ // ie it is a face on the interior of the footprint
  auto he = face->outer_ccb();
  auto first = he;

  while(true){
    // if (!he->source()- at_infinity())
      polygons.push_back({
        float(CGAL::to_double(he->source()->point().x())),
        float(CGAL::to_double(he->source()->point().y()))
      });

    he = he->next();
    if (he==first) break;
  // }
  }
}
bool arrangementface_to_polygon(Face_handle face, roofer::LinearRing& polygon, double h){
  // if(extract_face){ // ie it is a face on the interior of the footprint
  auto he = face->outer_ccb();
  if (!ccb_to_polygon_3(he, polygon, h)) return false;

  for (auto ccb = face->inner_ccbs_begin(); ccb != face->inner_ccbs_end(); ++ccb) {
    roofer::vec3f ring;
    if (!ccb_to_polygon_3(*ccb, ring, h)) return false;
    polygon.interior_rings().push_back(ring);
  }
  return true;
}

// helper functions
void arr_dissolve_seg_edges(Arrangement_2& arr)
{
  std::vector<Halfedge_handle> to_remove;
  for (auto he : arr.edge_handles()) {
    auto d1 = he->face()->data();
    auto d2 = he->twin()->face()->data();
    if ((d1.segid == d2.segid ) && (d1.in_footprint && d2.in_footprint) && d1.segid != 0)
      to_remove.push_back(he);
  }
  for (auto he : to_remove) {
    arr.remove_edge(he);
  }
}
void arr_remove_redundant_vertices(Arrangement_2& arr)
{
  // cleanup vertices with degree==2
  std::vector<Arrangement_2::Vertex_handle> to_remove;
  for (auto v : arr.vertex_handles()) {
    if(v->degree()==2)
      to_remove.push_back(v);
  }
  for (auto v : to_remove) {
    CGAL::remove_vertex(arr, v);
  }
}

void arr_dissolve_step_edges_naive(Arrangement_2& arr, float step_height_threshold, bool compute_on_edge)
{
  std::vector<Arrangement_2::Halfedge_handle> to_remove;
  for (auto& edge : arr.edge_handles()) {
    auto f1 = edge->face();
    auto f2 = edge->twin()->face();

    if((f1->data().in_footprint && f2->data().in_footprint) && (f1->data().segid!=0 && f2->data().segid!=0)) {
      double d;
      if (compute_on_edge) {
        auto& s = edge->source()->point();
        auto& t = edge->target()->point();
        auto& pl1 = f1->data().plane;
        auto& pl2 = f2->data().plane;
        double h1_pl1 = CGAL::to_double((pl1.a()*s.x() + pl1.b()*s.y() + pl1.d()) / (-pl1.c()));
        double h2_pl1 = CGAL::to_double((pl1.a()*t.x() + pl1.b()*t.y() + pl1.d()) / (-pl1.c()));
        double h1_pl2 = CGAL::to_double((pl2.a()*s.x() + pl2.b()*s.y() + pl2.d()) / (-pl2.c()));
        double h2_pl2 = CGAL::to_double((pl2.a()*t.x() + pl2.b()*t.y() + pl2.d()) / (-pl2.c()));
        d = std::max(std::abs(h1_pl1-h1_pl2), std::abs(h2_pl1-h2_pl2));
      } else {
        d = std::abs(f1->data().elevation_70p - f2->data().elevation_70p);
      }
      if(d < step_height_threshold){
        // Face_merge_observer takes care of data merge
        // if (f2->data().elevation_avg < f1->data().elevation_avg) {
        //   f2->data()= f1->data();
        // } else {
        //   f1->data() = f2->data();
        // }
        to_remove.push_back(edge);
      }
    }
  }
  for (auto edge : to_remove) {
    arr.remove_edge(edge);
  }
}

auto HandleHash = CGAL::Handle_hash_function{};
void arr_dissolve_step_edges(Arrangement_2& arr, float step_height_threshold)
{
  struct FacePair {
      Arrangement_2::Face_handle f_lo;
      Arrangement_2::Face_handle f_hi;

      FacePair(){};
      FacePair(Arrangement_2::Face_handle f1, Arrangement_2::Face_handle f2) {
        if (HandleHash(f1) < HandleHash(f1)) {
          f_lo = f1;
          f_hi = f2;
        } else {
          f_lo = f2;
          f_hi = f1;
        }
      };
  };
    
  struct KeyEqual {
    bool operator()(const FacePair& lhs, const FacePair& rhs) const
    {
      return lhs.f_hi == rhs.f_hi && lhs.f_lo == rhs.f_lo;
    }
  };
  struct KeyHash
  {
    std::size_t operator()(FacePair const& p) const
    {
      std::size_t h1 = HandleHash(p.f_lo);
      std::size_t h2 = HandleHash(p.f_hi);
      return h1 ^ (h2 << 1); // or use boost::hash_combine (see Discussion)
    }
  };
  
  std::unordered_map<
    FacePair, 
    std::vector<Arrangement_2::Halfedge_handle>,
    KeyHash, 
    KeyEqual
  > step_boundaries;

  while (true) {
    double d_min = step_height_threshold;
    step_boundaries.clear();
    for (auto& edge : arr.edge_handles()) {
      auto f1 = edge->face();
      auto f2 = edge->twin()->face();
      if((f1->data().in_footprint && f2->data().in_footprint) && (f1->data().segid!=0 && f2->data().segid!=0)) {
        step_boundaries[FacePair(f1,f2)].push_back(edge);
      }
    }
    FacePair facepair_min;
    for (auto& [faces, edges] : step_boundaries) {
      double d = std::abs(faces.f_hi->data().elevation_50p - faces.f_lo->data().elevation_50p);
      if (d < d_min) {
        d_min = d;
        facepair_min = faces;
      }
    }
    if (d_min == step_height_threshold) break;
    std::vector<Halfedge_handle> to_remove;
    for (auto& edge : step_boundaries[facepair_min]) {
      arr.remove_edge(edge);
    }
  }
}

void arr_snap_duplicates(Arrangement_2& arr, double dupe_threshold) {
  std::vector<Arrangement_2::Halfedge_handle> to_remove;
  double dupe_threshold_sq = dupe_threshold*dupe_threshold;
  for (auto he : arr.edge_handles()) {
    auto source = he->source();
    auto target = he->target();
    if (CGAL::squared_distance(source->point(), target->point()) < dupe_threshold_sq) {
      if ((source->degree()==2 && target->degree()>2) || (target->degree()==2 && source->degree()>2))
        to_remove.push_back(he);
      else
        std::cout << "skipping and edge in duplicate snapping. Degrees are " << target->degree() << " and " << source->degree() << "\n";
    }
  }
  for (auto he : to_remove) {
    Vertex_handle vy, v_other;
    Halfedge_handle he_other;
    auto source = he->source();
    auto target = he->target();
    if (source->degree()==2 && target->degree()>2) {
      vy = target;
      he_other = he->prev();
      v_other = he_other->source();
    } else if (target->degree()==2 && source->degree()>2) {
      vy = source;
      he_other = he->next();
      v_other = he_other->target();
    }
    arr.merge_edge(he, he_other, Segment_2(vy->point(), v_other->point()));
  }
}


// {
//   for (auto& v :  arr.vertex_handles()){
//     auto vhe = v->incident_halfedges();
//     auto vdone = vhe;
//     // check if v is not on the fp boundary
//     bool on_fp = false;
//     do {
//       on_fp |= (!vhe->face()->data().in_footprint) || (!vhe->twin()->face()->data().in_footprint);
//     } while (++vhe!=vdone);
//     if (!on_fp)
//       vertices_to_snap.push_back(v);
//   }
// }

void arr_dissolve_fp(Arrangement_2& arr, bool inside, bool outside) {
  {
    std::vector<Arrangement_2::Halfedge_handle> to_remove;
    for (auto he : arr.edge_handles()) {
      auto d1 = he->face()->data();
      auto d2 = he->twin()->face()->data();
      if(outside)
        if (!d1.in_footprint && !d2.in_footprint)
          to_remove.push_back(he);
      if(inside)
        if (d1.in_footprint && d2.in_footprint)
          to_remove.push_back(he);
    }
    for (auto he : to_remove) {
      arr.remove_edge(he);
    }
  }
}

void arr_filter_biggest_face(Arrangement_2& arr, const float& rel_area_thres) {
  // check number of faces
  typedef std::pair<Polygon_2, double> polyar;
  std::vector<polyar> polygons;
  double total_area=0;
  for (auto& fh : arr.face_handles()) {
    if (fh->data().segid != 0 || fh->data().in_footprint == true) {
      auto poly = arr_cell2polygon(fh);
      double area = CGAL::to_double(CGAL::abs(poly.area()));
      total_area += area;
      polygons.push_back(std::make_pair(poly, area));
    }
  }
  std::sort(polygons.begin(), polygons.end(), [](const polyar& a, const polyar& b) {
    return a.second < b.second;   
  });
  arr.clear();
  for (auto& poly_a : polygons) {
    if (poly_a.second > rel_area_thres * total_area)
      insert_non_intersecting_curves(arr, poly_a.first.edges_begin(), poly_a.first.edges_end());
  }
}

Polygon_2 arr_cell2polygon(const Face_handle& fh) {
  Polygon_2 poly;
  auto he = fh->outer_ccb();
  auto first = he;
  do {
    poly.push_back(he->target()->point());
    he = he->next();
  } while (he!=first);
  return poly;
}

void arr_label_buildingparts(Arrangement_2& arr) {
  std::stack<Face_handle> seeds;

  for (auto& fh : arr.face_handles()) {
    if(fh->data().in_footprint) seeds.push(fh);
  }

  int part_counter = 0;
  while (seeds.size()) {
    auto f_seed = seeds.top(); seeds.pop();
    if (f_seed->data().part_id == -1) {
      f_seed->data().part_id = part_counter;
      std::stack<Face_handle> candidates;
      candidates.push(f_seed);

      while(candidates.size()) {
        auto f_cand = candidates.top(); candidates.pop();
        auto he = f_cand->outer_ccb();
        auto first = he;

        // collect neighbouring faces
        while(true){
          auto f_cand_new = he->twin()->face();
          if (f_cand_new->data().in_footprint && f_cand_new->data().part_id == -1) {
            f_cand_new->data().part_id = part_counter;
            candidates.push(f_cand_new);
          }

          he = he->next();
          if (he==first) break;
        }
        // also look at holes
        for (auto ccb = f_cand->inner_ccbs_begin(); ccb != f_cand->inner_ccbs_end(); ++ccb) {
          auto he = (*ccb);
          auto first = he;

          // walk along entire hole ccb
          while(true){
            auto f_cand_new = he->twin()->face();
            if (f_cand_new->data().in_footprint && f_cand_new->data().part_id == -1) {
              f_cand_new->data().part_id = part_counter;
              candidates.push(f_cand_new);
            }

            he = he->next();
            if (he==first) break;
          }
        }
      }
      ++part_counter;
    }
  }
}

} // namespace roofer::detection