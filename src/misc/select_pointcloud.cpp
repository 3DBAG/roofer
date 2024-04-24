#include "select_pointcloud.hpp"

#include <cmath>
#include <numeric>
#include <algorithm>

#include "spdlog/spdlog.h"

namespace roofer {

  // put the one with lowest quality value (meaning highest quality) on top
  bool compareByQuality(const CandidatePointCloud* a,
                        const CandidatePointCloud* b) {
    return a->quality < b->quality;
  }

  // put the one with highest (most recent) date on top
  bool compareByDate(const CandidatePointCloud* a,
                     const CandidatePointCloud* b) {
    return a->date > b->date;
  }

  // put the one with lowest nodata_fraction on top
  bool compareByNoDataFraction(const CandidatePointCloud* a,
                     const CandidatePointCloud* b) {
    return a->nodata_fraction < b->nodata_fraction;
  }

  // Determine if the point cloud has enough point coverage for a good
  // reconstruction. The point cloud has enough coverage if the indicators
  // are below the given thresholds.
  // The 'threshold_nodata' is fraction of the footprint [0.0-1.0] with nodata
  // areas (pixels). The 'threshold_maxcircle' is the fraction of the footprint
  // [0.0-1.0] covered by the maximum inscribed circle of the largest gap in the
  // point cloud.
  bool hasEnoughPointCoverage(const CandidatePointCloud* pc,
                              float threshold_nodata,
                              float threshold_maxcircle);

  // Compute a boolean mask that indicates that the cell has data.
  std::vector<bool> computeMask(const std::vector<float>& image_array,
                                const float& nodataval);

  const CandidatePointCloud* getLatestPointCloud(const std::vector<CandidatePointCloud>& candidates) {
    std::vector<const CandidatePointCloud*> candidates_date;
    for(auto& cand : candidates) {
      candidates_date.push_back(&cand);
    }
    std::sort(candidates_date.begin(), candidates_date.end(),
              roofer::compareByDate);
    return candidates_date[0];
  }  

  const PointCloudSelectResult selectPointCloud(const std::vector<CandidatePointCloud>& candidates,
                                                const selectPointCloudConfig cfg) {
    PointCloudSelectResult result;
    std::vector<const CandidatePointCloud*> candidates_quality;
    std::vector<const CandidatePointCloud*> candidates_date;
    std::vector<const CandidatePointCloud*> candidates_coverage;
    for(auto& cand : candidates) {
      candidates_quality.push_back(&cand);
      candidates_date.push_back(&cand);
      candidates_coverage.push_back(&cand);
    }
    std::sort(candidates_date.begin(), candidates_date.end(),
              roofer::compareByDate);
    std::sort(candidates_quality.begin(), candidates_quality.end(),
              roofer::compareByQuality);
    std::sort(candidates_coverage.begin(), candidates_coverage.end(),
              roofer::compareByNoDataFraction);

    // get the highest quality candidate with sufficient coverage
    const CandidatePointCloud* best_suffcient = nullptr;
    // int candidates_quality_idx(-1);
    for (unsigned i = 0; i < candidates_quality.size(); ++i) {
      // spdlog::debug("quality {}={}", candidates_quality[i]->name, candidates_quality[i]->quality);
      if (roofer::hasEnoughPointCoverage(
              candidates_quality[i], cfg.threshold_nodata, cfg.threshold_maxcircle)) {
        best_suffcient = candidates_quality[i];
        // candidates_quality_idx = i;
        break;
      }
    }

    // no candidate has sufficient coverage
    // find candidate pc with higest coverage that is not mutated wrt latest pc
    if (!best_suffcient) {
      // spdlog::debug("best_suffcient=nullptr");
      result.explanation = PointCloudSelectExplanation::_HIGHEST_YET_INSUFFICIENT_COVERAGE;
      for (unsigned i = 0; i < candidates_coverage.size(); ++i) {
        if (!roofer::isMutated(
          candidates_coverage[i]->image_bundle, 
          candidates_date[0]->image_bundle,
          cfg.threshold_mutation_fraction, 
          cfg.threshold_mutation_difference
        )) {
          result.selected_pointcloud = candidates_coverage[i];
          result.explanation = _HIGHEST_YET_INSUFFICIENT_COVERAGE;
          return result;
        }
      }
      // we should never reach this point (since the above loop will at some point compare latest to itselft and there should be no mutation)
      spdlog::error("Unable to select pointcloud");
      exit(1);
    }
    // spdlog::debug("best_suffcient={}", best_suffcient->name);

    // get the latest candidate with sufficient coverage
    const CandidatePointCloud* latest_suffcient = nullptr;
    // int candidates_latest_idx(-1);
    for (unsigned i = 0; i < candidates_date.size(); ++i) {
      if (roofer::hasEnoughPointCoverage(
              candidates_date[i], cfg.threshold_nodata, cfg.threshold_maxcircle)) {
        latest_suffcient = candidates_date[i];
        // candidates_latest_idx = i;
        break;
      }
    }
    // spdlog::debug("latest_suffcient={}", latest_suffcient->name);

    // highest quality is also latest pointcloud
    if (best_suffcient == latest_suffcient) {
      result.selected_pointcloud = best_suffcient;
      result.explanation = PointCloudSelectExplanation::PREFERRED_AND_LATEST;
      return result;
    // else check for mutations
    } else {
      if (roofer::isMutated(
        best_suffcient->image_bundle, 
        latest_suffcient->image_bundle, 
        cfg.threshold_mutation_fraction, 
        cfg.threshold_mutation_difference
      )) {
        // If the two point clouds are different, that means
        // that the object has changed and the selected point
        // cloud is outdated, therefore, we have to use the
        // latest point cloud, even if it is possibly a lower
        // quality.
        result.selected_pointcloud = latest_suffcient;
        result.explanation = PointCloudSelectExplanation::LATEST_WITH_MUTATION;
        return result;
      } else {
        // return the best point cloud, it seems to be not mutated in the latest_sufficient 
        result.selected_pointcloud = best_suffcient;
        result.explanation = PointCloudSelectExplanation::PREFERRED_NOT_LATEST;
        return result;
      }
    }
  }

  bool hasEnoughPointCoverage(const CandidatePointCloud* pc,
                              float threshold_nodata,
                              float threshold_maxcircle) {
    // float nodata = roofer::computeNoDataFraction(pc->image_bundle);
    // spdlog::debug("pc->nodata_fraction={}, threshold_nodata={}", pc->nodata_fraction, threshold_nodata);
    bool nodata_good = pc->nodata_fraction <= threshold_nodata;
    // float nodata_maxcircle = computeNoDataMaxCircleFraction(pc);
    bool maxcircle_good = pc->nodata_radius <= threshold_maxcircle;
    // spdlog::debug("pc->nodata_radius={}, threshold_maxcircle={}", pc->nodata_radius, threshold_maxcircle);
    // spdlog::debug("nodata_good={}, maxcircle_good={}", nodata_good, maxcircle_good);
    return nodata_good && maxcircle_good;
  }

}  // namespace roofer