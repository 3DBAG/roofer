// The pipe reader releases the first-submitted building only after it can
// read the second one. Both use the production completion queue and writer.
#include "cityjson_output.hpp"

int main() {
  std::cin.tie(nullptr);
  auto tile = std::make_shared<BuildingTile>();
  tile->remaining = 2;
  tile->proj_helper = roofer::misc::createProjHelper();
  tile->attributes.insert_vec<std::string>("source") = {"first", "second"};

  BuildingObject first{};
  first.footprint.push_back({0, 0, 0});
  first.footprint.push_back({1, 0, 0});
  first.footprint.push_back({0, 1, 0});
  auto second = first;
  second.attribute_index = 1;
  second.reconstruction_success = false;
  second.extrusion_mode = FAIL;
  first.reconstruction_success = true;
  first.reconstruction_time = 7;

  CompletedBuildings completed(1);
  std::thread producer([&] {
    completed.push({tile, 1, 1, std::move(second)});
    if (std::cin.get() != EOF) completed.push({tile, 0, 0, std::move(first)});
    completed.close();
  });
  auto srs = roofer::io::createSpatialReferenceSystemOGR();
  std::atomic<size_t> buildings{0}, tiles{0};
  stream_completed_buildings(completed, std::cout, RooferConfig{}, {}, *srs,
                             buildings, tiles);
  producer.join();
  return buildings == 2 && tiles == 1 ? 0 : 1;
}
