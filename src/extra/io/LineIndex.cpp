#include <roofer/io/LineIndex.hpp>

#include <gdal_priv.h>
#include <ogrsf_frmts.h>

#include <algorithm>

namespace roofer::io {
  namespace {
    // calls f(x0, y0, x1, y1) for every segment of the lines in `geometry`
    template <typename F>
    void for_each_segment(const OGRGeometry* geometry, F f) {
      const auto type = wkbFlatten(geometry->getGeometryType());
      if (type == wkbLineString) {
        const auto* line = geometry->toLineString();
        for (int i = 1; i < line->getNumPoints(); ++i)
          f(line->getX(i - 1), line->getY(i - 1), line->getX(i), line->getY(i));
      } else if (OGR_GT_IsSubClassOf(type, wkbGeometryCollection)) {
        for (const auto* part : geometry->toGeometryCollection())
          for_each_segment(part, f);
      }
    }
  }  // namespace

  LineIndex::LineIndex(const std::string& source, const std::string& layer_name,
                       const std::string& expected_crs)
      : rtree_(misc::createRTreeGEOS()) {
    if (GDALGetDriverCount() == 0) GDALAllRegister();
    GDALDatasetUniquePtr dataset(
        GDALDataset::Open(source.c_str(), GDAL_OF_VECTOR));
    if (!dataset)
      throw rooferException("[LineIndex] Open failed on " + source + ": " +
                            CPLGetLastErrorMsg());
    auto* layer = layer_name.empty()
                      ? dataset->GetLayer(0)
                      : dataset->GetLayerByName(layer_name.c_str());
    if (!layer)
      throw rooferException("[LineIndex] Layer not found in " + source);
    if (!expected_crs.empty() && layer->GetSpatialRef()) {
      OGRSpatialReference expected;
      if (expected.SetFromUserInput(expected_crs.c_str()) != OGRERR_NONE)
        throw rooferException("[LineIndex] Invalid expected CRS");
      expected.StripVertical();
      std::unique_ptr<OGRSpatialReference> actual(
          layer->GetSpatialRef()->Clone());
      actual->StripVertical();
      expected.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
      actual->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
      if (!expected.IsSame(actual.get()))
        throw rooferException(
            "[LineIndex] Source does not use the footprint CRS");
    }
    for (auto& feature : layer)
      if (const auto* geometry = feature->GetGeometryRef())
        for_each_segment(geometry,
                         [&](double x0, double y0, double x1, double y1) {
                           segments_.push_back({arr2d{x0, y0}, arr2d{x1, y1}});
                         });
    for (auto& s : segments_)
      rtree_->insert(
          {std::min(s[0][0], s[1][0]), std::min(s[0][1], s[1][1]), 0,
           std::max(s[0][0], s[1][0]), std::max(s[0][1], s[1][1]), 0},
          &s);
  }

  SegmentCollection LineIndex::clip(const LinearRing& footprint,
                                    const arr3d& offset) const {
    TBox<double> box;
    for (const auto& p : footprint)
      box.add(arr3d{p[0] + offset[0], p[1] + offset[1], 0});
    auto items = rtree_->query(box);
    // source order, independent of the tree layout
    std::ranges::sort(items, std::less<>());
    OGRPolygon polygon;
    auto add_ring = [&](const vec3f& ring) {
      OGRLinearRing ogr_ring;
      for (const auto& p : ring) ogr_ring.addPoint(p[0], p[1]);
      ogr_ring.closeRings();
      polygon.addRing(&ogr_ring);
    };
    add_ring(footprint);
    for (const auto& hole : footprint.interior_rings()) add_ring(hole);
    SegmentCollection result;
    for (const auto* item : items) {
      const auto& s = *static_cast<const std::array<arr2d, 2>*>(item);
      OGRLineString line;
      line.addPoint(s[0][0] - offset[0], s[0][1] - offset[1]);
      line.addPoint(s[1][0] - offset[0], s[1][1] - offset[1]);
      std::unique_ptr<OGRGeometry> inside(polygon.Intersection(&line));
      if (inside)
        for_each_segment(inside.get(),
                         [&](double x0, double y0, double x1, double y1) {
                           result.push_back({arr3f{float(x0), float(y0), 0},
                                             arr3f{float(x1), float(y1), 0}});
                         });
    }
    return result;
  }
}  // namespace roofer::io
