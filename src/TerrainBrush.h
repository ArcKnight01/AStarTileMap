#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// Choices for editing the map with the left mouse button (the PLAYING side panel's "Left click", "Height" and "Brush"
// dropdowns). Board::setTerrainHeight() and Board::updateCostMap() (TerrainBrush.cpp) do the editing.
//
//   Left click: Place start / end -> click a cell, pick Start / End from the list (TilePlacement.cpp)
//   Left click: Set terrain       -> click or drag: a tapered Gaussian (below) of the chosen height is added to the
//                                    "elevation" layer inside the brush box (nothing outside it changes), or with a
//                                    height of 0 the box is flattened toward 0 m; then the slope, roughness, step,
//                                    traversability and cost are rebuilt around those cells and the path is planned again
//
// The brush only edits elevation. Slope, roughness and step are computed from it over small windows (TerrainFeatures.h),
// so they (and the cost) also change a couple of cells outside the box, and inflation spreads a lethal patch further.
// Cost of one click on a flat map: see the table measured in tests/test_costmap.cpp ("brush cost table").
// (was: every cell under the brush moved toward a target height, z <- z + strength * w * ( target - z ), with a
// Gaussian that was still 0.14 at the box edge, so it left a small cliff around the box)
// Up / Down change the brush height in brushHeightStep steps (Shift: brushHeightBigStep), so every cost is reachable.

// What a left click on the map does.
enum class ClickMode {
	PlaceStartEnd, //select a cell and place the start / end on it
	SetTerrain     //paint the elevation layer with the brush
};
/**
 * Get the name of every click mode, in ClickMode order (as shown in the dropdown).
 *
 * @return  One name per ClickMode value.
 */
inline std::vector<std::string> getClickModeNames() {
	return { "Place start / end", "Set terrain" };
}

// One entry of the "Height" dropdown.
struct TerrainHeight {
	std::string name; //as shown in the dropdown
	float elevation;  //m: the peak added to the "elevation" layer (negative digs; 0 flattens)
};
/**
 * Get the preset heights of the "Height" dropdown, lowest first.
 *
 * @return  The presets, with heights in metres.
 */
inline std::vector<TerrainHeight> getTerrainHeights() {
	return {
		{ "Hollow -0.05 m", -0.05f }, //digs a hollow 0.05 m deep at the centre (was: moved the cells toward -0.05 m)
		{ "Dip    -0.02 m", -0.02f },
		{ "Flatten 0.00 m",  0.00f }, //flattens the box toward 0 m (was "Ground")
		{ "Bump   +0.02 m",  0.02f },
		{ "Mound  +0.05 m",  0.05f },
		{ "Wall   +0.50 m",  0.50f }  //lethal, like the walls of the dungeon map
	};
}
// Up / Down change the brush height by this much (m); with Shift held, by the big step. 0.01 m is about 0.1 of edge cost.
const float brushHeightStep = 0.01f;
const float brushHeightBigStep = 0.05f;
/**
 * Get the name of every preset height, in getTerrainHeights() order.
 *
 * @return  The dropdown's option names.
 */
inline std::vector<std::string> getTerrainHeightNames() {
	std::vector<std::string> names;
	for (const TerrainHeight& height : getTerrainHeights()) {
		names.push_back(height.name);
	}
	return names;
}

// The "Brush" dropdown: the brush is a square of this many cells per side, centred on the cell under the mouse.
/**
 * Get the brush widths offered in the "Brush" dropdown.
 *
 * @return  Sides of the square brush, in cells (odd).
 */
inline std::vector<int> getBrushWidths() {
	return { 1, 3, 5, 9 };
}
/**
 * Get the name of every brush width, in getBrushWidths() order.
 *
 * @return  The dropdown's option names.
 */
inline std::vector<std::string> getBrushNames() {
	return { "1 cell", "3 x 3 cells", "5 x 5 cells", "9 x 9 cells" };
}

// The brush is a Gaussian centred on the middle of the brush box, tapered so it reaches exactly 0 at the box's border:
//   g( d ) = exp( -d^2 / ( 2 sigma^2 ) ),    sigma = brushSigmaPerWidth * width,    R = width / 2   (centre to edge, cells)
//   w( d ) = max( 0, ( g( d ) - g( R ) ) / ( 1 - g( R ) ) )      1 at the centre, 0 at distance R and beyond
// so the corners of the box and everything outside it get 0. A 1-cell brush is a single weight of 1 (only that cell).
// One application with height h:
//   h != 0:  z  <-  z + h * w              (adds a smooth mound, or digs a hollow if h < 0; repeated clicks add up)
//   h == 0:  z  <-  z * ( 1 - w )          (flattens toward 0 m: the centre reaches 0, the edge blends in)
// It is applied once per click, and once more each time a drag moves the brush centre into another cell.
// The kernels are computed once per brush width (Board::brushKernels) and only looked up while painting.
// (was: z <- z + strength * w * ( target - z ) with an untapered Gaussian and a strength of 10 % .. 100 % on Left / Right)
const float brushSigmaPerWidth = 0.25f;
// heights closer to 0 than this count as 0, so the brush flattens
const float brushFlattenTolerance = 0.0005f;

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
 * Build the square, tapered Gaussian kernel of a brush (0 at the box's border, see above).
 *
 * @param width  Side of the square brush, in cells (odd).
 * @return       width * width weights, row by row (index = row * width + column), centre weight 1, all in [0, 1].
 */
inline std::vector<float> buildGaussianKernel(int width) {


	// pseudo-code:
	// k = ( width - 1 ) / 2,  sigma = brushSigmaPerWidth * width,  R = width / 2,  edge = g( R^2 )
	// K[ r ][ c ] = max( 0, ( g( ( c - k )^2 + ( r - k )^2 ) - edge ) / ( 1 - edge ) )


	int halfWidth = (width - 1) / 2;
	float sigma   = brushSigmaPerWidth * width;
	float radius  = width / 2.f;
	float edge    = computeGaussianWeight(radius * radius, sigma);
	std::vector<float> kernel(width * width);


	for (int row = 0; row < width; row++) {
		for (int column = 0; column < width; column++) {
			float squaredDistance = (float)((column - halfWidth) * (column - halfWidth) + (row - halfWidth) * (row - halfWidth));
			float weight = (computeGaussianWeight(squaredDistance, sigma) - edge) / (1.f - edge);
			kernel[row * width + column] = std::max(0.f, weight);
		}
	}


	return kernel;
}
