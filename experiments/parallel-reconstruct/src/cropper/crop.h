#pragma once

#include <queue>

#include "PC.h"
#include "datastructures.h"

void crop_coro(uint nr_points_per_laz, uint nr_laz,
               std::queue<Points>& queue_cropped);

void crop(uint nr_points_per_laz, uint nr_laz,
          std::queue<Points>& queue_cropped);
