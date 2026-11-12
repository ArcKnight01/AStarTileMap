#include "Tile.h"
#include <algorithm>
#include <cmath>
#include "CostMap.h"

// Editing the "elevation" layer with a square Gaussian brush (the "Set terrain" left-click mode), then rebuilding the cost map
// around the edit. See TerrainBrush.h for the choices offered in the side panel.

void Board::buildBrushKernels() {


	// pseudo-code:
	// for each brush width w:  brushKernels[ w ] = buildGaussianKernel( w )


	for (int width : getBrushWidths()) {
		brushKernels[width] = buildGaussianKernel(width);
	}
}

void Board::setTerrainHeight(int tile, float elevation, int brushWidth, float strength) {


	// pseudo-code:
	// (c0, r0) = cell of tile;  k = ( brushWidth - 1 ) / 2;  K = brushKernels[ brushWidth ]
	// for every cell (c, r) with |c - c0| <= k and |r - r0| <= k inside the map:
	//     w = K[ r - r0 + k ][ c - c0 + k ]
	//     elevation(c, r) = elevation(c, r) + strength * w * ( elevation - elevation(c, r) );  remember (c, r) as edited
	//
	// (was: elevation(c, r) = elevation for every cell of the square, which left a vertical step around the patch;
	//  then z + w * ( elevation - z ), a full step every click)


	if (!map.hasLayer("elevation")) {
		map.addLayer("elevation", 0.f); //a map made from a cost layer alone starts flat
	}
	std::vector<float>& heights        = map.getLayer("elevation");
	const std::vector<float>& kernel   = brushKernels[brushWidth];
	int centerColumn = map.getColumn(tile);
	int centerRow    = map.getRow(tile);
	int halfWidth    = (brushWidth - 1) / 2;


	for (int row = centerRow - halfWidth; row <= centerRow + halfWidth; row++) {
		for (int column = centerColumn - halfWidth; column <= centerColumn + halfWidth; column++) {
			if (!map.isInside(column, row)) continue;
			int index    = map.getIndex(column, row);
			float weight = kernel[(row - centerRow + halfWidth) * brushWidth + (column - centerColumn + halfWidth)];
			//an unknown (NaN) height has nothing to blend from, so it is set to the target
			float current = GridMap::isUnknown(heights[index]) ? elevation : heights[index];
			heights[index] = current + strength * weight * (elevation - current);
			terrainEditedTiles.push_back(index);
		}
	}
}

void Board::updateCostMap() {


	// pseudo-code:
	// rebuilt = runCostMapPipeline( copy of map )        (elevation -> features -> traversability -> cost -> inflation)
	// reach   = feature window reach + inflation reach   (cells; how far one changed height can change the cost)
	// for every cell c within reach of an edited cell:
	//     layer( c ) = rebuilt.layer( c )   for each derived layer this map already has
	//
	// efficient alternative: run the pipeline only on the window around the edit instead of the whole copy


	GridMap rebuilt = map;
	runCostMapPipeline(rebuilt, parameters);


	//a feature looks max(1, round(radius / resolution)) cells around a cell (TerrainFeatures.cpp); inflation then
	//spreads a lethal cell round(inflationRadius / resolution) cells further (CostMap.cpp)
	double resolution    = map.getResolution();
	double featureRadius = std::max({ parameters.slopeRadius, parameters.roughnessRadius, parameters.stepRadius });
	int featureReach     = std::max(1, (int)std::round(featureRadius / resolution));
	int inflationReach   = (int)std::round(parameters.inflationRadius / resolution);
	int reach            = featureReach + inflationReach;


	//mark every cell within reach of an edited cell (a square, which also covers the round inflation)
	std::vector<bool> affected(map.getCellCount(), false);
	for (int tile : terrainEditedTiles) {
		int centerColumn = map.getColumn(tile);
		int centerRow    = map.getRow(tile);
		for (int row = centerRow - reach; row <= centerRow + reach; row++) {
			for (int column = centerColumn - reach; column <= centerColumn + reach; column++) {
				if (map.isInside(column, row)) affected[map.getIndex(column, row)] = true;
			}
		}
	}


	//copy the rebuilt values back into the marked cells. Layers the map doesn't have are not added (the dungeon map
	//has only "elevation" and "cost"), so its saved file keeps the same layers.
	const std::vector<std::string> derivedLayers = { "slope", "roughness", "step", "traversability", "cost" };
	for (const std::string& name : derivedLayers) {
		if (!map.hasLayer(name)) continue;
		std::vector<float>& layer               = map.getLayer(name);
		const std::vector<float>& rebuiltLayer  = rebuilt.getLayer(name);
		for (int index = 0; index < map.getCellCount(); index++) {
			if (affected[index]) layer[index] = rebuiltLayer[index];
		}
	}


	terrainEditedTiles.clear();
	updatePitCells(); //a lethal area may have appeared, grown, or turned from a wall into a pit
	navDirty = true;  //the cost changed: re-plan
}

void Board::drawTerrainBrush(sf::RenderWindow& window, int tile, int brushWidth) {


	// pseudo-code:
	// k = ( brushWidth - 1 ) / 2
	// outline from the top-left corner of cell (c0 - k, r0 - k) to the bottom-right corner of cell (c0 + k, r0 + k),
	// clipped to the board


	if (tile < 0) return;
	int halfWidth    = (brushWidth - 1) / 2;
	int firstColumn  = std::max(0, map.getColumn(tile) - halfWidth);
	int firstRow     = std::max(0, map.getRow(tile) - halfWidth);
	int lastColumn   = std::min(map.getColumns() - 1, map.getColumn(tile) + halfWidth);
	int lastRow      = std::min(map.getRows() - 1, map.getRow(tile) + halfWidth);


	sf::RectangleShape outline(sf::Vector2f((lastColumn - firstColumn + 1) * tileW, (lastRow - firstRow + 1) * tileH));
	outline.setPosition(origin.x + firstColumn * tileW, origin.y + firstRow * tileH);
	outline.setFillColor(sf::Color(255, 255, 255, 40));
	outline.setOutlineColor(sf::Color::Cyan);
	outline.setOutlineThickness(-2.f);
	window.draw(outline);
}
