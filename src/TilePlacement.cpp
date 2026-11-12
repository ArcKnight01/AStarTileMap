#include "Tile.h"

// Placing tiles by selecting them (replaces the old Space / Enter controls in convertTile(), which are kept, commented out,
// in main.cpp).
//   1. click a tile                        -> beginPlacement(): it is selected and a list of types appears next to it
//   2. Up / Down                           -> movePlacementChoice(): the tile previews the highlighted type
//   3. click the same tile again, or Enter -> confirmPlacement()
//   right-click, Backspace or Esc          -> cancelPlacement()

string Board::getTileTypeName(tiletype type) {
	switch (type) {
	case ground:     return "Ground";
	case wall:       return "Wall";
	case startnode:  return "Start";
	case endnode:    return "End";
	case checkpoint: return "Ground"; //the character's breadcrumb is ground underneath
	}
	return "Ground";
}

sf::Sprite& Board::getSpriteFor(tiletype type) {
	switch (type) {
	case wall:       return wallSprite;
	case startnode:  return startSprite;
	case endnode:    return endSprite;
	case checkpoint: return checkPointSprite;
	case ground:     return floorSprite;
	}
	return floorSprite;
}

void Board::beginPlacement(int tile) {
	if (tile < 0 || tile >= tileCount) {
		cancelPlacement();
		return;
	}
	isPlacing = true;
	placingTile = tile;

	//what can go on this tile: always ground and wall; start / end only if there isn't one yet,
	//or if this tile already is it (so it stays an option, and choosing Ground removes it)
	placementOptions.clear();
	placementOptions.push_back(ground);
	placementOptions.push_back(wall);
	if (!startExists || tileTypes[tile] == startnode) {
		placementOptions.push_back(startnode);
	}
	if (!endExists || tileTypes[tile] == endnode) {
		placementOptions.push_back(endnode);
	}

	//start the preview on what the tile already is (the breadcrumb counts as ground)
	tiletype current = (tileTypes[tile] == checkpoint) ? ground : tileTypes[tile];
	placementChoice = 0;
	for (int i = 0; i < (int)placementOptions.size(); i++) {
		if (placementOptions[i] == current) {
			placementChoice = i;
		}
	}
}

void Board::movePlacementChoice(int step) {
	if (!isPlacing || placementOptions.empty()) {
		return;
	}
	int count = (int)placementOptions.size();
	placementChoice = ((placementChoice + step) % count + count) % count; //wraps around in both directions
}

tiletype Board::getPlacementPreview() {
	if (!isPlacing || placementOptions.empty()) {
		return ground;
	}
	return placementOptions[placementChoice];
}

void Board::confirmPlacement() {
	if (!isPlacing) {
		return;
	}
	int tile = placingTile;
	tiletype oldType = tileTypes[tile];
	tiletype newType = getPlacementPreview();

	//the tile stops being the start / end if it was one
	if (oldType == startnode && newType != startnode) {
		startExists = false;
	}
	if (oldType == endnode && newType != endnode) {
		endExists = false;
	}
	//and becomes the start / end if that was chosen
	if (newType == startnode) {
		startPos = tile;
		startExists = true;
	}
	if (newType == endnode) {
		endPos = tile;
		endExists = true;
	}

	tileTypes[tile] = newType;
	navDirty = true; //a tile changed: rebuild the nav graph and re-plan
	cancelPlacement();
}

void Board::cancelPlacement() {
	isPlacing = false;
	placingTile = -1;
	placementOptions.clear();
	placementChoice = 0;
}
