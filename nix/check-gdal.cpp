#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <cstdio>

int main() {
  GDALAllRegister();
  auto* manager = GetGDALDriverManager();
  for (const char* name :
       {"GPKG", "PostgreSQL", "PGDUMP", "GTiff", "GeoJSON", "ESRI Shapefile"}) {
    if (!manager->GetDriverByName(name)) {
      std::fprintf(stderr, "Missing GDAL driver: %s\n", name);
      return 1;
    }
  }
  // Resource databases must work without environment overrides or network
  // access.
  OGRSpatialReference crs;
  if (crs.importFromEPSG(28992) != OGRERR_NONE) return 2;
  if (!OGRGeometryFactory::haveGEOS()) return 3;
  auto* gpkg = manager->GetDriverByName("GPKG")->Create("test.gpkg", 0, 0, 0,
                                                        GDT_Unknown, nullptr);
  if (!gpkg) return 4;
  auto* layer = gpkg->CreateLayer("footprints", &crs, wkbPoint, nullptr);
  if (!layer) return 5;
  auto* feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
  OGRPoint point(155000, 450000);
  feature->SetGeometry(&point);
  const auto error = layer->CreateFeature(feature);
  OGRFeature::DestroyFeature(feature);
  GDALClose(gpkg);
  if (error != OGRERR_NONE) return 6;
  gpkg = static_cast<GDALDataset*>(
      GDALOpenEx("test.gpkg", GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
  if (!gpkg || gpkg->GetLayer(0)->GetFeatureCount() != 1) return 7;
  GDALClose(gpkg);
  auto* tiff = manager->GetDriverByName("GTiff")->Create("test.tif", 2, 2, 1,
                                                         GDT_Byte, nullptr);
  if (!tiff) return 8;
  if (tiff->SetSpatialRef(&crs) != CE_None) return 9;
  GDALClose(tiff);
  tiff = static_cast<GDALDataset*>(GDALOpen("test.tif", GA_ReadOnly));
  if (!tiff || !tiff->GetSpatialRef()) return 10;
  GDALClose(tiff);
  GDALDestroyDriverManager();
}
