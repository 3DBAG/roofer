// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of geoflow-roofer (https://github.com/3DBAG/geoflow-roofer)

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
#include <roofer/misc/PointcloudRasteriser.hpp>
#include <string>

namespace roofer::misc {

  struct CandidatePointCloud {
    float nodata_radius;    // radius of the incribed circle in the largest gap
                            // in the point cloud
    float nodata_fraction;  // nodata fraction
    ImageMap& image_bundle;
    int building_yoc;  // year of construction of building. -1 if unknown
    std::string name;  // point cloud name
    int quality;       // point cloud quality score. The lower the better the
                       // quality.
    int date;          // point cloud acquisition date
    int index;         // input point cloud index
  };

  enum PointCloudSelectExplanation {
    NONE,
    PREFERRED_AND_LATEST,                // PL
    PREFERRED_NOT_LATEST,                // P
    LATEST_WITH_MUTATION,                // LM
    _HIGHEST_YET_INSUFFICIENT_COVERAGE,  // _C(M)
    _LATEST                              // _L
  };

  struct PointCloudSelectResult {
    const CandidatePointCloud* selected_pointcloud = nullptr;
    PointCloudSelectExplanation explanation = NONE;
  };

  struct selectPointCloudConfig {
    // Thresholds determined from AHN3 Leiden
    // total fraction of no data area inside footprint
    float threshold_nodata = 0.06;
    // max allowed nodata radius
    float threshold_maxcircle = 0.5;
    // The >=50% change was determined by analyzing the data.
    // See the Leiden, percent_diff_AHN3_2020 plot.
    float threshold_mutation_fraction = 0.5;
    // The threshold is 1.2 meters, because the accuracy of the Kadaster's
    // Dense Image Matching point cloud is about 30cm, so we are at 4 sigma.
    // float threshold_mutation_difference = 1.2;
    float threshold_mutation_difference = 1.2;
  };

  const CandidatePointCloud* getLatestPointCloud(
      const std::vector<CandidatePointCloud>& candidates);

  // return either
  // 1. latest, unless there is coverage issue (case AHN 3/4, both have good
  // quality)
  // 2. best quality, unless there is coverage issue (based on user quality
  // rating, case Kadaster DIM/AHN) In both cases poor coverage candidates are
  // eliminated Also considers mutations, ie. in case best quality candidate is
  // mutated wrt latest latest is selected
  const PointCloudSelectResult selectPointCloud(
      const std::vector<CandidatePointCloud>& candidates,
      const selectPointCloudConfig cfg = selectPointCloudConfig());

}  // namespace roofer::misc
