#pragma once

#include <roofer/reconstruction/ArrangementBase.hpp>
#include <array>

namespace roofer::reconstruction::detail {
  // Profiles run from p to q, with the solid on the left of the directed span.
  // Crossing profiles must be split before adding a span.
  class WallAssembler {
    struct Span {
      Point_2 p, q;
      std::array<float, 2> lower, upper;
      int part, label;
    };
    std::vector<Span> spans_;

   public:
    void add(const Point_2& p, const Point_2& q, std::array<float, 2> lower,
             std::array<float, 2> upper, int part, int label);
    void emit(std::unordered_map<int, Mesh>& meshes) const;
  };

  // Pair vertical boundary subdivisions before removing vertices that are
  // redundant in every incident surface of the solid.
  void conform_vertical_edges(std::unordered_map<int, Mesh>& meshes);
  // Bound the 3D deviation of each degree-two chain by this tolerance. Zero
  // retains exact simplification; junction vertices always remain fixed.
  void remove_redundant_vertices(std::unordered_map<int, Mesh>& meshes,
                                 float tolerance = 0);
}  // namespace roofer::reconstruction::detail
