#pragma once
#include "CostMapParameters.h"
#include "GridMap.h"

// Traversability features from the "elevation" layer, in the style of Fankhauser's grid_map filter chain.
// Each looks at a circular window around the cell (radius in metres, from CostMapParameters) and writes one layer.
// A cell whose own elevation is unknown gets an unknown (NaN) feature.

//"slope" (radians): angle between vertical and the normal of the plane z = a*x + b*y + c fitted to the window
void computeSlope(GridMap& map, const CostMapParameters& parameters);
//"roughness" (metres): standard deviation of the heights from that fitted plane
void computeRoughness(GridMap& map, const CostMapParameters& parameters);
//"step" (metres): highest minus lowest elevation in the window
void computeStepHeight(GridMap& map, const CostMapParameters& parameters);
//all three
void computeTerrainFeatures(GridMap& map, const CostMapParameters& parameters);

//least-squares plane z = a*x + b*y + c through the points (needs 3 or more points not all on one line)
bool fitPlane(const std::vector<double>& xs, const std::vector<double>& ys, const std::vector<double>& zs, double& a, double& b, double& c);
