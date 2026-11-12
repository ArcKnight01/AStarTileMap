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

//"cost" from "traversability"
void buildCostMap(GridMap& map, const CostMapParameters& parameters);
//grows the lethal cells of "cost" by inflationRadius
void inflateCostMap(GridMap& map, const CostMapParameters& parameters);
//the whole pipeline from "elevation": features -> traversability -> cost -> inflation
void runCostMapPipeline(GridMap& map, const CostMapParameters& parameters);

bool isLethal(float cost, const CostMapParameters& parameters);
