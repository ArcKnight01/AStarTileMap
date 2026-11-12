#include "Tile.h"
#include "CostMap.h"

// Placing the start and end by selecting a cell (costmap branch; editing the cost map itself comes later).
//   1. click a cell                        -> beginPlacement(): it is selected and a list appears next to it
//   2. Up / Down                           -> movePlacementChoice(): the cell previews the highlighted option
//   3. click the same cell again, or Enter -> confirmPlacement()
//   right-click, Backspace or Esc          -> cancelPlacement()
// Start / End are offered only while they don't exist yet and never on a lethal cell; Clear removes them.

string Board::getPlacementName(PlacementOption option) {
	switch (option) {
	case PlacementOption::Start: return "Start";
	case PlacementOption::End:   return "End";
	case PlacementOption::Clear: return "Clear";
	}
	return "Clear";
}

void Board::beginPlacement(int tile) {
	if (tile < 0 || tile >= map.getCellCount()) {
		cancelPlacement();
		return;
	}
	placementOptions.clear();
	bool lethal = isLethal(map.getLayer("cost")[tile], parameters);
	bool isStart = startExists && tile == startPos;
	bool isEnd = endExists && tile == endPos;
	if (!startExists && !lethal) placementOptions.push_back(PlacementOption::Start);
	if (!endExists && !lethal && !isStart) placementOptions.push_back(PlacementOption::End);
	if (isStart || isEnd) placementOptions.push_back(PlacementOption::Clear);
	if (placementOptions.empty()) {
		cancelPlacement(); //nothing can be placed here (a lethal cell, or both already placed elsewhere)
		return;
	}
	isPlacing = true;
	placingTile = tile;
	placementChoice = 0;
}

void Board::movePlacementChoice(int step) {
	if (!isPlacing || placementOptions.empty()) {
		return;
	}
	int count = (int)placementOptions.size();
	placementChoice = ((placementChoice + step) % count + count) % count; //wraps around in both directions
}

void Board::confirmPlacement() {
	if (!isPlacing || placementOptions.empty()) {
		return;
	}
	switch (placementOptions[placementChoice]) {
	case PlacementOption::Start:
		startPos = placingTile;
		startExists = true;
		break;
	case PlacementOption::End:
		endPos = placingTile;
		endExists = true;
		break;
	case PlacementOption::Clear:
		if (startExists && placingTile == startPos) startExists = false;
		if (endExists && placingTile == endPos) endExists = false;
		break;
	}
	navDirty = true; //re-plan
	cancelPlacement();
}

void Board::cancelPlacement() {
	isPlacing = false;
	placingTile = -1;
	placementOptions.clear();
	placementChoice = 0;
}
