#include "reconstruct.h"

#include <thread>

GenerateModelsBatch reconstruct_batch(std::span<Points> points_one_batch) {
  std::vector<Points> reconstructed_models_one_laz{};

  for (const auto& points_one_building : points_one_batch) {
    Points building_model;
    for (const auto& p : points_one_building.x) {
      building_model.x.push_back(p * (float)42.0);
      building_model.y.push_back(p * (float)42.0);
      building_model.z.push_back(p * (float)42.0);
    }
    reconstructed_models_one_laz.push_back(building_model);
    std::this_thread::sleep_for(SLEEP_PER_BUILDING);
  }
  co_return reconstructed_models_one_laz;
}

GenerateModels reconstruct_one_building_coro(Points points_one_building) {
  Points building_model;
  for (const auto& p : points_one_building.x) {
    building_model.x.push_back(p * (float)42.0);
    building_model.y.push_back(p * (float)42.0);
    building_model.z.push_back(p * (float)42.0);
  }
  std::this_thread::sleep_for(SLEEP_PER_BUILDING);
  co_return building_model;
}

Points reconstruct_one_building(Points& points_one_building) {
  Points building_model;
  for (const auto& p : points_one_building.x) {
    building_model.x.push_back(p * (float)42.0);
    building_model.y.push_back(p * (float)42.0);
    building_model.z.push_back(p * (float)42.0);
  }
  std::this_thread::sleep_for(SLEEP_PER_BUILDING);
  return building_model;
}

GenerateModelsBatch reconstruct(
    std::deque<std::vector<Points>>& deque_cropped_laz) {
  const uint EMIT_TRACE_AT = 10;
  auto logger_coro = spdlog::get("coro");
  auto logger_reconstruct = spdlog::get("reconstruct");
  std::vector<Points> vec_reconstructed_one_laz{};
  std::atomic<uint> count_buildings{0};
  if (!deque_cropped_laz.empty()) {
    for (; !deque_cropped_laz.empty(); deque_cropped_laz.pop_front()) {
      logger_coro->debug("popped one item from deque_cropped_laz");
      auto reconstructed_one_laz = reconstruct_batch(deque_cropped_laz.front());
      co_yield reconstructed_one_laz.mCoroHdl.promise()._valueOut;
    }
  }
  co_return {};
}
