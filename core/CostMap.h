#pragma once
#include "CostMapParameters.h"
#include "GridMap.h"

// The heuristic cost map: Rajput's "Multimodal Locomotion Costmap Generation" (thesis Algorithm 1), ground-only.
//
//   for each cell:
//     traversability < threshold  -> lethal (cost = lethalCost; the M4 would fly here, the Go2 cannot)
//     traversability unknown      -> by UnknownPolicy (lethal, unknownCost, or 0)
//     otherwise                   -> cost = 1 - traversability   (0 = ideal ... 0.5 = worst traversable)
//   inflation: every cell within inflationRadius of a lethal cell becomes lethal too (Rajput's neighbourhood max),
//   so the path keeps the robot's width away from untraversable ground.
//
// The planner only reads the "cost" layer, so a map can also start here (a cost layer made directly) and skip
// the elevation -> features -> traversability stages.

/**
 * Write "cost" from "traversability".
 *
 * @param map         Map with a "traversability" layer; gets a "cost" layer (lethal, 1 - t, or by unknownPolicy).
 * @param parameters  traversabilityThreshold, lethalCost, unknownPolicy and unknownCost.
 */
void buildCostMap(GridMap& map, const CostMapParameters& parameters);
/**
 * Grows the lethal cells of "cost" by inflationRadius.
 *
 * @param map         Map whose "cost" layer is inflated in place.
 * @param parameters  inflationRadius (metres) and lethalCost.
 */
void inflateCostMap(GridMap& map, const CostMapParameters& parameters);
/**
 * The whole pipeline from "elevation": features -> traversability -> cost -> inflation.
 *
 * @param map         Map with an "elevation" layer (metres); gets every derived layer.
 * @param parameters  Thresholds, weights and radii of the pipeline.
 */
void runCostMapPipeline(GridMap& map, const CostMapParameters& parameters);

/**
 * Check whether a cost is lethal (at or above lethalCost; NaN counts as lethal).
 *
 * @param cost        Value of a cell in the "cost" layer.
 * @param parameters  lethalCost.
 * @return            True if the cell must not be entered.
 */
bool isLethal(float cost, const CostMapParameters& parameters);
