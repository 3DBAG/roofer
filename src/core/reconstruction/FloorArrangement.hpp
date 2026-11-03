#pragma once

#include <roofer/reconstruction/ArrangementBase.hpp>
#include <roofer/reconstruction/ArrangementExtruder.hpp>
#include <functional>
#include "WallAssembler.hpp"

namespace roofer::reconstruction::detail {
  struct FloorRegion {
    int part = -1;
    std::optional<float> ceiling;
  };

  // Built only after roof snapping. All underpass interior constraints stay
  // here; only footprint boundary vertices are synchronised with the roof.
  class FloorArrangement {
   public:
    Arrangement_2 floor;
    std::unordered_map<Face_const_handle, FloorRegion> regions;

    void prepare(Arrangement_2& roof, const ElevationProvider& elevations,
                 std::span<const Underpass> underpasses,
                 const ArrangementExtruderConfig& config);

    void emit(
        Arrangement_2& roof, const ElevationProvider& elevations,
        const ArrangementExtruderConfig& config,
        const std::function<float(Vertex_handle, Face_handle)>& roof_height,
        std::unordered_map<int, Mesh>& meshes, WallAssembler& walls) const;
  };

}  // namespace roofer::reconstruction::detail
