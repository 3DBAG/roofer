#pragma once

#include <roofer/reconstruction/ArrangementBase.hpp>
#include <roofer/reconstruction/ArrangementExtruder.hpp>
#include <functional>

namespace roofer::reconstruction::detail {
  struct FloorRegion {
    int part = -1;
    std::optional<float> ceiling;
    std::vector<std::size_t> sources;
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
        std::unordered_map<int, Mesh>& meshes) const;
  };

  // Insert every height at a vertical vertex column into all incident walls.
  // This preserves edge pairing after facade polygons have been assembled.
  void conform_vertical_edges(std::unordered_map<int, Mesh>& meshes);
}  // namespace roofer::reconstruction::detail
