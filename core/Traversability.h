#pragma once
#include "CostMapParameters.h"
#include "GridMap.h"

// "traversability" (0 = not traversable, 1 = ideal) from the "slope", "roughness" and "step" layers, following
// Wermelinger et al. (IROS 2016) / ETH's traversability_estimation package:
//   each feature scores  s_k = 1 - value_k / critical_k   below its critical value, and 0 at or above it
//   t = slopeWeight * s_slope + roughnessWeight * s_roughness + stepWeight * s_step     (ETH default: 1/3 each)
// With zeroIfAnyCritical, any feature at or above its critical value makes t = 0 (a stricter variant).
// t is unknown (NaN) if any feature is unknown.
// This geometric score is the project's proxy for loose-terrain (sand) risk.
void computeTraversability(GridMap& map, const CostMapParameters& parameters);

//one feature's score: 1 - value / critical below the critical value, 0 at or above it
double getFeatureScore(double value, double critical);
