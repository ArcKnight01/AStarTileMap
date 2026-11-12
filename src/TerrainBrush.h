#pragma once
#include <string>
#include <vector>

// Choices for editing the map with the left mouse button (the PLAYING side panel's "Left click", "Height" and "Brush"
// dropdowns). Board::setTerrainHeight() and Board::updateCostMap() (TerrainBrush.cpp) do the editing.
//
//   Left click: Place start / end -> click a cell, pick Start / End from the list (TilePlacement.cpp)
//   Left click: Set terrain       -> click or drag: every cell under the brush gets the chosen height,
//                                    then the cost map is rebuilt around those cells and the path is planned again
//
// The heights are chosen against the critical values of CostMapParameters (step 0.12 m, slope 0.40 rad, 0.1 m cells):
//   +-0.05 m stays walkable but costly (its edge is a 0.05 m step and a ~0.3 rad slope),
//   +0.15 m and -0.20 m are steeper than a 0.12 m step, so their edges are lethal (and inflated by 0.2 m),
//   +0.50 m is a wall, like the walls of the dungeon map.

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
		{ "Pit   -0.20 m", -0.20f },
		{ "Low   -0.05 m", -0.05f },
		{ "Ground 0.00 m",  0.00f },
		{ "Bump  +0.05 m",  0.05f },
		{ "Ledge +0.15 m",  0.15f },
		{ "Wall  +0.50 m",  0.50f }
	};
}
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
