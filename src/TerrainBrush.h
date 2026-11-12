#pragma once
#include <cmath>
#include <string>
#include <vector>

// Choices for editing the map with the left mouse button (the PLAYING side panel's "Left click", "Height" and "Brush"
// dropdowns). Board::setTerrainHeight() and Board::updateCostMap() (TerrainBrush.cpp) do the editing.
//
//   Left click: Place start / end -> click a cell, pick Start / End from the list (TilePlacement.cpp)
//   Left click: Set terrain       -> click or drag: every cell under the brush moves toward the chosen height by
//                                    a Gaussian weight (below), then the cost map is rebuilt around those cells
//                                    and the path is planned again
//
// How high a painted patch is sets the cost of its sides. Measured with one click at 100 % strength of the Gaussian brush (below) on a
// flat map, default CostMapParameters, 0.1 m cells (cN = worst walkable cost, L = the patch is lethal):
//   brush          0.02 m   0.05 m   0.08 m   0.10 m   0.15 m   0.50 m
//   1 cell         c0.17    c0.42    L        L        L        L
//   3 x 3          c0.15    c0.39    L        L        L        L
//   5 x 5          c0.11    c0.27    c0.43    L        L        L
//   9 x 9          c0.07    c0.18    c0.28    c0.35    L        L
// A wider brush spreads the same height over a gentler slope, so it can go higher before the slope reaches
// criticalSlope (0.40 rad) and the traversability falls below 0.5 (lethal). The step limit (0.12 m) is never reached first.
// The presets below stay walkable with every brush, except the wall.
// (the first presets were -0.20 / -0.05 / 0 / +0.05 / +0.15 / +0.50 m with a hard-edged square brush: a height
// difference of 0.06 m or more across one cell was lethal, so the pit and the ledge were walls too)
// Up / Down change the brush height in brushHeightStep steps (Shift: brushHeightBigStep), so every cost is reachable.

// What a left click on the map does.
enum class ClickMode {
	PlaceStartEnd, //select a cell and place the start / end on it
	SetTerrain     //paint the elevation layer with the brush
};
inline std::vector<std::string> getClickModeNames() {
	return { "Place start / end", "Set terrain" };
}

// One entry of the "Height" dropdown.
struct TerrainHeight {
	std::string name; //as shown in the dropdown
	float elevation;  //m, written into the "elevation" layer
};
inline std::vector<TerrainHeight> getTerrainHeights() {
	return {
		{ "Hollow -0.05 m", -0.05f }, //side cost ~0.2 - 0.4 (by brush width); was "Pit"
		{ "Dip    -0.02 m", -0.02f }, //side cost ~0.1 - 0.2
		{ "Ground  0.00 m",  0.00f }, //cost 0: smooths a mound or hollow back to flat (drag over it)
		{ "Bump   +0.02 m",  0.02f }, //side cost ~0.1 - 0.2
		{ "Mound  +0.05 m",  0.05f }, //side cost ~0.2 - 0.4; was "Ledge" (a Gaussian patch has no ledge)
		{ "Wall   +0.50 m",  0.50f }  //lethal, like the walls of the dungeon map
	};
}
// Up / Down change the brush height by this much (m); with Shift held, by the big step. 0.01 m is about 0.1 of edge cost.
const float brushHeightStep = 0.01f;
const float brushHeightBigStep = 0.05f;
inline std::vector<std::string> getTerrainHeightNames() {
	std::vector<std::string> names;
	for (const TerrainHeight& height : getTerrainHeights()) {
		names.push_back(height.name);
	}
	return names;
}

// The "Brush" dropdown: the brush is a square of this many cells per side, centred on the cell under the mouse.
inline std::vector<int> getBrushWidths() {
	return { 1, 3, 5, 9 };
}
inline std::vector<std::string> getBrushNames() {
	return { "1 cell", "3 x 3 cells", "5 x 5 cells", "9 x 9 cells" };
}

// The brush is a Gaussian: each brushed cell moves part of the way toward the chosen height,
//   z  <-  z + strength * w * ( target - z ),     w = exp( -d^2 / ( 2 sigma^2 ) ),     sigma = brushSigmaPerWidth * width
// so the height falls off smoothly toward the brush's edge: a patch is a smooth mound or hollow instead of a block with
// a vertical step around it. strength (Left / Right, 10 % .. 100 %) is how far one click goes: at 100 % the centre
// (w = 1) reaches the target at once; at 30 % each click closes 30 % of the gap, so clicking again and again with
// Ground flattens a mound slowly (the centre is within 3 % of flat after 10 clicks: 0.7^10 = 0.03). Moving toward the
// target (instead of adding to the height) means repeated clicks and drags settle on the target instead of piling up.
// (was: z <- z + w * ( target - z ), a full step every click)
// The kernels are computed once per brush width (Board::brushKernels) and only looked up while painting.
// sigma = width / 4 leaves exp( -2 ) ~ 0.14 at the middle of the brush's edge; a 1-cell brush is a single weight of 1.
const float brushSigmaPerWidth = 0.25f;
// brush strength: how far one click moves a cell toward the target, in percent (Left / Right change it by the step)
const int brushStrengthDefault = 30;
const int brushStrengthStep = 10;
const int brushStrengthLowest = 10;
const int brushStrengthHighest = 100;

/**
 * Compute the Gaussian weight at a distance from the brush centre.
 *
 * @param squaredDistance  Squared distance from the centre, in cells^2.
 * @param sigma            Standard deviation of the Gaussian, in cells.
 * @return                 Weight in ( 0, 1 ], 1 at the centre.
 */
inline float computeGaussianWeight(float squaredDistance, float sigma) {


	// pseudo-code:
	// w = exp( -d^2 / ( 2 sigma^2 ) )


	return std::exp(-squaredDistance / (2.f * sigma * sigma));
}

/**
 * Build the square Gaussian kernel of a brush.
 *
 * @param width  Side of the square brush, in cells (odd).
 * @return       width * width weights, row by row (index = row * width + column), centre weight 1.
 */
inline std::vector<float> buildGaussianKernel(int width) {


	// pseudo-code:
	// k = ( width - 1 ) / 2,  sigma = brushSigmaPerWidth * width
	// K[ r ][ c ] = computeGaussianWeight( ( c - k )^2 + ( r - k )^2, sigma )


	int halfWidth = (width - 1) / 2;
	float sigma   = brushSigmaPerWidth * width;
	std::vector<float> kernel(width * width);


	for (int row = 0; row < width; row++) {
		for (int column = 0; column < width; column++) {
			float squaredDistance = (float)((column - halfWidth) * (column - halfWidth) + (row - halfWidth) * (row - halfWidth));
			kernel[row * width + column] = computeGaussianWeight(squaredDistance, sigma);
		}
	}


	return kernel;
}
