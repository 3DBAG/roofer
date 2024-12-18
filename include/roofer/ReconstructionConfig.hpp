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

namespace roofer {
  /**
   * @brief Configuration parameters for the roofer building
   * reconstruction algorithm. Coordinate units are assumed to be in meters.
   */
  struct ReconstructionConfig {
    /**
     * @brief Complexity factor for building model geometry.
     *
     * A number between `0.0` and `1.0`. Higher values lead to
     * more detailed building models, lower values to simpler models.
     */
    float complexity_factor = 0.888;
    /**
     * @brief Set to true to activate the procedure that
     * clips parts from the input footprint wherever patches of ground points
     * are detected. May cause irregular outlines in reconstruction result.
     */
    bool clip_ground = true;
    /**
     * @brief Requested Level of Detail
     * - 12: LoD 1.2
     * - 13: LoD 1.3
     * - 22: LoD 2.2
     */
    int lod = 22;
    /**
     * @brief Step height used for LoD13 generalisation, i.e. roofparts with a
     * height discontinuity that is smaller than this value are merged. Only
     * affects LoD 1.3 reconstructions. Unit: meters.
     */
    float lod13_step_height = 3.;
    /**
     * @brief Floor elevation in case it is not provided by the
     * footprint (API only).
     */
    float floor_elevation = 0.;
    /**
     * @brief Force flat floor instead of using the
     * elevation of the footprint (API only).
     */
    bool override_with_floor_elevation = false;

    /**
     * @brief Number of points used in nearest neighbour queries
     * during plane detection.
     */
    int plane_detect_k = 15;

    /**
     * @brief Minimum number of points required for detecting a plane.
     */
    int plane_detect_min_points = 15;

    /**
     * @brief # Maximum distance from candidate points to plane during
     * plane fitting procedure. Higher values offer more robustness against
     * oversegmentation in plane detection, lower values give more planes that
     * are closer to the point cloud. Unit: meters.
     */
    float plane_detect_epsilon = 0.300000;

    /**
     * @brief Maximum allowed angle between points inside the same
     * detected plane. This value is compared to the dot product between two
     * unit normals. Eg. 0 means 90 degrees (orthogonal normals), and 1.0 means
     * 0 degrees (parallel normals)
     */
    float plane_detect_normal_angle = 0.750000;

    /**
     * @brief Maximum distance from candidate points to line during line
     * fitting procedure. Higher values offer more robustness against irregular
     * lines, lower values give more accurate lines (ie. more closely wrapping
     * around point cloud). Unit: meters.
     */
    float line_detect_epsilon = 1.000000;

    /**
     * @brief Distance used in computing alpha-shape of detected plane
     * segments prior to line detection. Higher values offer more robustness
     * against irregular lines, lower values give more accurate lines (ie. more
     * closely wrapping around point cloud). Unit: meters.
     */
    float thres_alpha = 0.250000;

    /**
     * @brief Maximum distance to merge lines during line regularisation
     * (after line detection). Approximately parallel lines that are closer to
     * each other than this threshold are merged. Higher values yield more
     * regularisation, lower values preserve more finer details. Unit: meters.
     */
    float thres_reg_line_dist = 0.800000;

    /**
     * @brief Extension of regularised lines prior to optimisation. Used
     * to compensate for undetected parts in the roofpart boundaries. Use higher
     * values when the input pointcloud is less dense. However, setting this too
     * high can lead to unrealistic reconstruction results. Unit: meters.
     */
    float thres_reg_line_ext = 3.000000;
    // lod1_extrude_to_max=false

    bool is_valid() {
      return (complexity_factor >= 0 && complexity_factor <= 1.0) &&
                 lod == 12 ||
             lod == 13 || lod == 22 && lod13_step_height > 0;
    }
  };
}  // namespace roofer
