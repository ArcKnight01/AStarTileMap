#include "Tile.h"
#include "CostMap.h"

// Placing the start and end by selecting a cell (costmap branch; editing the cost map itself comes later).
//   1. click a cell                        -> beginPlacement(): it is selected and a list appears next to it
//   2. Up / Down                           -> movePlacementChoice(): the cell previews the highlighted option
//   3. click the same cell again, or Enter -> confirmPlacement()
//   right-click, Backspace or Esc          -> cancelPlacement()
// Start / End are offered on every walkable cell (moving them there if they already exist), never on a lethal cell;
// Clear removes the start / end from the cell they are on.

string Board::getPlacementName(PlacementOption option) {
	switch (option) {
	case PlacementOption::Start: return "Start";
	case PlacementOption::End:   return "End";
	case PlacementOption::Clear: return "Clear";
	}
	return "Clear";
}

string Board::getPlacementLabel(PlacementOption option) const {
	if (option == PlacementOption::Start && startExists) return "Move start";
	if (option == PlacementOption::End && endExists) return "Move end";
	return getPlacementName(option);
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
	//OLD: Start / End only while they didn't exist yet, so on a map that has both, only their own cells could be selected
	//if (!startExists && !lethal) placementOptions.push_back(PlacementOption::Start);
	//if (!endExists && !lethal && !isStart) placementOptions.push_back(PlacementOption::End);
	if (!lethal && !isStart && !isEnd) placementOptions.push_back(PlacementOption::Start);
	if (!lethal && !isStart && !isEnd) placementOptions.push_back(PlacementOption::End);
	if (isStart || isEnd) placementOptions.push_back(PlacementOption::Clear);
	if (placementOptions.empty()) {
		cancelPlacement(); //nothing can be placed on a lethal cell
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
	case PlacementOption::Start: //places it, or moves it here
		startPos = placingTile;
		startExists = true;
		break;
	case PlacementOption::End: //places it, or moves it here
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
