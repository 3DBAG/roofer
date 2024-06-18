
struct PointcloudSourceConfig {
  std::string path;
  std::string name;
  int quality;
  int date = 0;
  int bld_class = 6;
  int grnd_class = 2;
  bool force_low_lod = false;
  bool select_only_for_date = false;
};

struct FootprintSourceConfig {
  std::string path;
  std::string id_attribute;
  std::string low_lod_attribute;
  std::string year_of_construction_attribute;
};

struct CropConfig {
  float max_point_density = 20;
  float cellsize = 0.5;
  int low_lod_area = 69000;
  float max_point_density_low_lod = 5;
};

struct AppConfig {
  // crop 
  std::vector<PointcloudSourceConfig> PointcloudSources;
  FootprintSourceConfig FootprintSource;
  CropConfig CropParameters;

  //reconstruct
  //...
};



struct CropBatchData {

}

struct BuildingData {
  roofer::PointCollection pointcloud;
  roofer::LinearRing footprint;

  float nodata_radius;
  float nodata_fraction;
  float pt_density;
  bool is_mutated;
  roofer::LinearRing nodata_circle;
  roofer::ImageMap building_raster;
  float ground_elevation;
  int acquisition_year;
};

struct Data {
  std::list<BuildingData> buildings;
};
