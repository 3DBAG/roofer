#pragma once

#include <roofer/reconstruction/ArrangementBase.hpp>
#include <roofer/reconstruction/ArrangementExtruder.hpp>
#include <functional>
#include "WallAssembler.hpp"

namespace roofer::reconstruction::detail {
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
