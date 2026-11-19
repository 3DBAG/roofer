#pragma once

#include <roofer/reconstruction/ArrangementBase.hpp>
#include <roofer/reconstruction/ArrangementExtruder.hpp>
#include <functional>
#include "WallAssembler.hpp"

namespace roofer::reconstruction::detail {
  struct VertexColumn {
    std::vector<std::pair<float, Face_const_handle>> heights;
    void snap(float tolerance);
    float height(Face_const_handle face) const;
  };
  using VertexColumns = std::unordered_map<Vertex_const_handle, VertexColumn>;

  struct HeightProfile {
    EPECK::FT source, target;
  };
  std::optional<EPECK::Point_3> profile_crossing(const Point_2& p,
                                                 const Point_2& q,
                                                 const HeightProfile& a,
                                                 const HeightProfile& b);

  // One XY subdivision for roofs, ground and underpass ceilings. Original roof
  // identities and input coverage survive every computational face split.
  class ExtrusionArrangement {
   public:
    void compute(Arrangement_2& arrangement,
                 const ElevationProvider& elevations,
                 std::span<const Underpass> underpasses,
                 const ArrangementExtruderConfig& config,
                 std::vector<UnderpassDiagnostic>& diagnostics,
                 std::unordered_map<int, Mesh>& meshes, WallAssembler& walls);

    // Ordinary extrusion needs no underpass subdivision.
    void emit_ground_and_facades(
        Arrangement_2& roof, const ElevationProvider& elevations,
        const ArrangementExtruderConfig& config,
        const std::function<float(Vertex_handle, Face_handle)>& roof_height,
        std::unordered_map<int, Mesh>& meshes, WallAssembler& walls) const;
  };
}  // namespace roofer::reconstruction::detail
