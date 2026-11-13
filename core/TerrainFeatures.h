#pragma once
#include "CostMapParameters.h"
#include "GridMap.h"

// Traversability features from the "elevation" layer, in the style of Fankhauser's grid_map filter chain.
// Each looks at a circular window around the cell (radius in metres, from CostMapParameters) and writes one layer.
// A cell whose own elevation is unknown gets an unknown (NaN) feature.

//"slope" (radians): angle between vertical and the normal of the plane z = a*x + b*y + c fitted to the window
/**
 * Write the "slope" layer (radians) of the map.
 *
 * @param map         Map with an "elevation" layer (metres).
 * @param parameters  slopeRadius (metres).
 */
void computeSlope(GridMap& map, const CostMapParameters& parameters);
//"roughness" (metres): standard deviation of the heights from that fitted plane
/**
 * Write the "roughness" layer (metres) of the map.
 *
 * @param map         Map with an "elevation" layer (metres).
 * @param parameters  roughnessRadius (metres).
 */
void computeRoughness(GridMap& map, const CostMapParameters& parameters);
//"step" (metres): highest minus lowest elevation in the window
/**
 * Write the "step" layer (metres) of the map.
 *
 * @param map         Map with an "elevation" layer (metres).
 * @param parameters  stepRadius (metres).
 */
void computeStepHeight(GridMap& map, const CostMapParameters& parameters);
/**
 * All three: write the "slope", "roughness" and "step" layers.
 *
 * @param map         Map with an "elevation" layer (metres).
 * @param parameters  The three window radii (metres).
 */
void computeTerrainFeatures(GridMap& map, const CostMapParameters& parameters);

/**
 * Least-squares plane z = a*x + b*y + c through the points (needs 3 or more points not all on one line).
 *
 * @param xs  x of each point (metres).
 * @param ys  y of each point (metres).
 * @param zs  Height of each point (metres).
 * @param a   Set to the slope in x.
 * @param b   Set to the slope in y.
 * @param c   Set to the height at (0, 0) (metres).
 * @return    False if there are fewer than 3 points or they lie on one line.
 */
bool fitPlane(const std::vector<double>& xs, const std::vector<double>& ys, const std::vector<double>& zs, double& a, double& b, double& c);
