#include "reconstruct.h"

#include <thread>

GenerateModels reconstruct_one_building_coro(Points& points_one_building) {
  Points building_model;
  for (const auto& p : points_one_building.x) {
    building_model.x.push_back(p * (float)42.0);
    building_model.y.push_back(p * (float)42.0);
    building_model.z.push_back(p * (float)42.0);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  co_return building_model;
}

Points reconstruct_one_building(Points& points_one_building) {
  Points building_model;
  for (const auto& p : points_one_building.x) {
    building_model.x.push_back(p * (float)42.0);
    building_model.y.push_back(p * (float)42.0);
    building_model.z.push_back(p * (float)42.0);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  return building_model;
}
