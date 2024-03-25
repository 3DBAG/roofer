
#include "../datastructures.hpp"
#include <memory>

namespace roofer {
  struct Vector2DOpsInterface {

    virtual ~Vector2DOpsInterface() = default;

    virtual void simplify_polygons(
      std::vector<LinearRing>& polygons,
      float tolerance = 0.01,
      // bool output_failures = true,
      bool orient_after_simplify = true
    ) = 0;

    virtual void buffer_polygons(
      std::vector<LinearRing>& polygons,
      float offset = 4
    ) = 0;
  };

  std::unique_ptr<Vector2DOpsInterface> createVector2DOpsGEOS();
}