// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of roofer (https://github.com/3DBAG/roofer)

// geoflow-roofer was created as part of the 3DBAG project by the TU Delft 3D
// geoinformation group (3d.bk.tudelf.nl) and 3DGI (3dgi.nl)

// geoflow-roofer is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version. geoflow-roofer is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
// Public License for more details. You should have received a copy of the GNU
// General Public License along with geoflow-roofer. If not, see
// <https://www.gnu.org/licenses/>.

// Author(s):
// Ravi Peters

#pragma once

#include <deque>
#include <type_traits>
#include <unordered_map>
#include <chrono>

namespace roofer {

  namespace regiongrower {

    using namespace std;

    class Region {
      size_t region_id;

     public:
      Region(size_t region_id) : region_id(region_id){};

      size_t get_region_id() { return region_id; }
    };

    // typename std::enable_if<std::is_base_of<Implementation, T>::value,
    // bool>::type
    template <typename candidateDS, typename regionType>
    class RegionGrower {
      size_t cur_region_id = 1;

     public:
      vector<size_t> region_ids;
      vector<regionType> regions;
      size_t min_segment_count = 15;
      map<size_t, map<size_t, size_t>>
          adjacencies;  // key: highes plane id, value: vector with adjacent
                        // plane_ids (all lower)

     private:
      template <typename Tester>
      inline bool grow_one_region(candidateDS& cds, Tester& tester,
                                  size_t& seed_handle) {
        deque<size_t> candidates;
        vector<size_t> handles_in_region;
        regions.push_back(regionType(cur_region_id));

        candidates.push_back(seed_handle);
        handles_in_region.push_back(seed_handle);
        region_ids[seed_handle] = cur_region_id;  // regions.size();

        while (candidates.size() > 0) {
          auto candidate = candidates.front();
          candidates.pop_front();
          for (auto neighbour : cds.get_neighbours(candidate)) {
            if (region_ids[neighbour] != 0) {
              if (region_ids[neighbour] != cur_region_id) {
                adjacencies[cur_region_id][region_ids[neighbour]]++;
              }
              continue;
            }
            if (tester.is_valid(cds, candidate, neighbour, regions.back())) {
              candidates.push_back(neighbour);
              handles_in_region.push_back(neighbour);
              region_ids[neighbour] = cur_region_id;  // regions.size();
            }
          }
        }
        // undo region if it doesn't satisfy quality criteria
        if (handles_in_region.size() < min_segment_count) {
          regions.erase(regions.end() - 1);
          adjacencies.erase(cur_region_id);
          for (auto handle : handles_in_region) region_ids[handle] = 0;
          return false;
        }
        return true;
      };

     public:
      template <typename Tester>
      void grow_regions(candidateDS& cds, Tester& tester) {
        std::vector<size_t> new_regions;
        std::deque<size_t> seeds = cds.get_seeds();

        region_ids.resize(cds.size, 0);
        // first region means unsegmented
        regions.push_back(regionType(0));

        // region growing from seed points
        while (seeds.size() > 0) {
          auto idx = seeds.front();
          seeds.erase(seeds.begin());
          if (region_ids[idx] == 0) {
            grow_one_region(cds, tester, idx);
            ++cur_region_id;
          }
        }
      };
      template <typename Tester>
      void grow_regions_with_limits(candidateDS& cds, Tester& tester,
                                    size_t limit_n_regions,
                                    size_t limit_n_milliseconds) {
        std::vector<size_t> new_regions;
        std::deque<size_t> seeds = cds.get_seeds();

        region_ids.resize(cds.size, 0);
        // first region means unsegmented
        regions.push_back(regionType(0));

        // region growing from seed points
        auto t_start = std::chrono::high_resolution_clock::now();
        while (seeds.size() > 0) {
          auto idx = seeds.front();
          seeds.erase(seeds.begin());
          if (region_ids[idx] == 0) {
            grow_one_region(cds, tester, idx);
            ++cur_region_id;
            if (regions.size() >= limit_n_regions ||
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now() - t_start)
                        .count() >= limit_n_milliseconds) {
              throw std::runtime_error("Region growing limit reached.");
            }
          }
        }
      };
    };

  }  // namespace regiongrower

}  // namespace roofer
