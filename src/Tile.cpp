#include "Tile.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include "CostMap.h"
#include "GridMapFile.h"

using namespace std;

vector<string> getMapViewNames() {
	return { "Terrain", "Cost", "Traversability", "Slope", "Roughness", "Step" };
}

Board::Board(float height, float width, sf::Vector2f origin) {
	boardH = height;
	boardW = width;
	this->origin = origin;
	wallTexture.loadFromFile("Textures/wall.png");
	wallTexture.setRepeated(true);
	map = GridMap(80, 80, 0.1);
	updateGeometry();
}

void Board::updateGeometry() {
	tileW = (map.getColumns() > 0) ? boardW / map.getColumns() : boardW;
	tileH = (map.getRows() > 0) ? boardH / map.getRows() : boardH;
}

// ---------------------------------------------------------------------------------------------------------------
// loading and saving
// ---------------------------------------------------------------------------------------------------------------

vector<string> Board::getSavedMapNames() {
	vector<string> savedMaps;
	std::error_code ec;
	for (const auto& entry : std::filesystem::directory_iterator("maps", ec)) {
		if (entry.path().extension() == ".gridmap") {
			savedMaps.push_back(entry.path().stem().string());
		}
	}
	std::sort(savedMaps.begin(), savedMaps.end());
	return savedMaps;
}

string Board::cleanMapName(string name) {
	// accept "rugged", "rugged.gridmap", "maps/rugged" or "maps/rugged.gridmap"
	while (!name.empty() && isspace((unsigned char)name.back())) name.pop_back();
	while (!name.empty() && isspace((unsigned char)name.front())) name.erase(0, 1);
	if (name.rfind("maps/", 0) == 0) name = name.substr(5);
	const string extension = ".gridmap";
	if (name.size() > extension.size() && name.substr(name.size() - extension.size()) == extension) {
		name.resize(name.size() - extension.size());
	}
	// If they enter no name, use the flat map
	if (name == "") {
		name = "flat";
	}
	return name;
}

void Board::importBoard(string& name) {
	name = cleanMapName(name);
	string filename = "maps/" + name + ".gridmap";
	int start = -1, end = -1;
	if (loadGridMap(filename, map, start, end)) {
		cout << "Loading " << filename << endl;
		mapIsNew = false;
	}
	else {
		// No saved map: start a new flat one (cost 0 everywhere) of the usual size
		cout << "No map named \"" << name << "\": starting a new flat map (SAVE & MENU saves it as " << filename << ")" << endl;
		map = GridMap(80, 80, 0.1);
		map.addLayer("elevation", 0.f);
		mapIsNew = true;
	}
	// The planner starts from the cost layer. A map that only has an elevation layer gets the whole pipeline
	// (elevation -> features -> traversability -> cost -> inflation) run on it first.
	if (!map.hasLayer("cost")) {
		if (!map.hasLayer("elevation")) {
			map.addLayer("elevation", 0.f);
		}
		runCostMapPipeline(map, parameters);
	}
	startPos = start;
	endPos = end;
	startExists = (startPos >= 0);
	endExists = (endPos >= 0);
	updateGeometry();
	navDirty = true;
}

void Board::exportBoard(string name) {
	saveGridMap("maps/" + name + ".gridmap", map, startExists ? startPos : -1, endExists ? endPos : -1);
}

// ---------------------------------------------------------------------------------------------------------------
// planning
// ---------------------------------------------------------------------------------------------------------------

void Board::replan(PathAlgorithm algorithm) {
	navDirty = false;
	if (!(startExists && endExists)) {
		pathTiles.clear();
		searchedTiles.clear();
		pathCost = 0;
		return;
	}
	PathResult result = findPath(map, parameters, algorithm, startPos, endPos);
	pathTiles = result.path;
	searchedTiles = result.searchedCells;
	pathCost = result.cost;
}

// ---------------------------------------------------------------------------------------------------------------
// drawing
// ---------------------------------------------------------------------------------------------------------------

void Board::cycleView() {
	int count = (int)getMapViewNames().size();
	view = (MapView)(((int)view + 1) % count);
}

//linear blend between two colours (amount 0 -> a, 1 -> b)
static sf::Color blend(sf::Color a, sf::Color b, double amount) {
	amount = std::max(0.0, std::min(1.0, amount));
	return sf::Color((sf::Uint8)(a.r + (b.r - a.r) * amount), (sf::Uint8)(a.g + (b.g - a.g) * amount), (sf::Uint8)(a.b + (b.b - a.b) * amount));
}

//green (0) -> yellow (0.5) -> red (1)
static sf::Color getHeatColor(double amount) {
	const sf::Color good(40, 140, 70), middle(225, 200, 60), bad(210, 60, 40);
	if (amount < 0.5) return blend(good, middle, amount * 2);
	return blend(middle, bad, (amount - 0.5) * 2);
}

sf::Color Board::getCellColor(int index) {
	const sf::Color unknown(0, 0, 0);
	auto layerValue = [&](const char* name) -> float {
		return map.hasLayer(name) ? map.getLayer(name)[index] : GridMap::getUnknownValue();
	};
	switch (view) {
	case MapView::Terrain: {
		//elevation from the floor sprite's brown (lowest point of the map) to the wall sprite's grey (highest point),
		//both brightened so height differences are easy to see
		const sf::Color low(96, 80, 48), high(168, 168, 168);
		float z = layerValue("elevation");
		if (GridMap::isUnknown(z)) return unknown;
		if (elevationHighest - elevationLowest < 1e-6f) return low; //a flat map is all brown
		return blend(low, high, (z - elevationLowest) / (elevationHighest - elevationLowest));
	}
	case MapView::Cost: {
		float c = layerValue("cost");
		if (GridMap::isUnknown(c)) return unknown;
		return getHeatColor(c / 0.5); //traversable costs run from 0 to 0.5
	}
	case MapView::Traversability: {
		float t = layerValue("traversability");
		if (GridMap::isUnknown(t)) return unknown;
		return getHeatColor(1.0 - t);
	}
	case MapView::Slope: {
		float s = layerValue("slope");
		return GridMap::isUnknown(s) ? unknown : getHeatColor(s / parameters.criticalSlope);
	}
	case MapView::Roughness: {
		float r = layerValue("roughness");
		return GridMap::isUnknown(r) ? unknown : getHeatColor(r / parameters.criticalRoughness);
	}
	case MapView::Step: {
		float s = layerValue("step");
		return GridMap::isUnknown(s) ? unknown : getHeatColor(s / parameters.criticalStep);
	}
	}
	return unknown;
}

void Board::drawBoard(sf::RenderWindow& window) {
	//outline around the whole map (drawn first; the cells cover its inside)
	sf::RectangleShape outline(sf::Vector2f(boardW, boardH));
	outline.setPosition(origin);
	outline.setFillColor(sf::Color::Transparent);
	outline.setOutlineColor(sf::Color(200, 200, 200));
	outline.setOutlineThickness(4);
	window.draw(outline);

	//lowest and highest known elevation, for the Terrain view's brown-to-grey scale (worked out once per frame)
	elevationLowest = 0.f;
	elevationHighest = 0.f;
	if (map.hasLayer("elevation")) {
		bool first = true;
		for (float e : map.getLayer("elevation")) {
			if (GridMap::isUnknown(e)) continue;
			if (first) { elevationLowest = elevationHighest = e; first = false; }
			elevationLowest = std::min(elevationLowest, e);
			elevationHighest = std::max(elevationHighest, e);
		}
	}

	//1) every cell: coloured by the chosen view, or the stone wall texture if it is lethal (in every view)
	const std::vector<float>& cost = map.getLayer("cost");
	sf::VertexArray cells(sf::Quads);
	sf::VertexArray lethalCells(sf::Quads);
	for (int i = 0; i < map.getCellCount(); i++) {
		sf::Vector2f p = getTilePos(i);
		sf::Vector2f corners[4] = { p, p + sf::Vector2f(tileW, 0), p + sf::Vector2f(tileW, tileH), p + sf::Vector2f(0, tileH) };
		if (isLethal(cost[i], parameters)) {
			//texture coordinates follow the window position, so neighbouring lethal cells form one continuous stone wall
			for (const sf::Vector2f& corner : corners) {
				lethalCells.append(sf::Vertex(corner, sf::Color(205, 205, 205), (corner - origin) * (10.f / 18.f)));
			}
		}
		else {
			sf::Color color = getCellColor(i);
			for (const sf::Vector2f& corner : corners) {
				cells.append(sf::Vertex(corner, color));
			}
		}
	}
	window.draw(cells);
	window.draw(lethalCells, &wallTexture);

	//2) the search result: searched cells tinted purple, then the path in blue on top
	sf::RectangleShape box;
	box.setSize(sf::Vector2f(tileW, tileH));
	box.setFillColor(sf::Color(150, 60, 210, 110));
	for (int i : searchedTiles) {
		box.setPosition(getTilePos(i));
		window.draw(box);
	}
	box.setSize(sf::Vector2f(tileW * 0.6f, tileH * 0.6f));
	box.setFillColor(sf::Color(60, 150, 255));
	for (int i : pathTiles) {
		box.setPosition(getTilePos(i) + sf::Vector2f(tileW * 0.2f, tileH * 0.2f));
		window.draw(box);
	}

	//3) start (green) and end (red); a selected cell shows the option being previewed instead
	auto drawMarker = [&](int i, sf::Color color) {
		sf::CircleShape marker(std::max(tileW, 6.f) * 0.9f);
		marker.setOrigin(marker.getRadius(), marker.getRadius());
		marker.setPosition(getTilePos(i) + sf::Vector2f(tileW / 2, tileH / 2));
		marker.setFillColor(color);
		marker.setOutlineColor(sf::Color::White);
		marker.setOutlineThickness(2);
		window.draw(marker);
	};
	bool previewing = isPlacing && placingTile >= 0 && placingTile < map.getCellCount() && !placementOptions.empty();
	PlacementOption preview = previewing ? placementOptions[placementChoice] : PlacementOption::Clear;
	if (startExists && !(previewing && placingTile == startPos && preview == PlacementOption::Clear)) {
		drawMarker(startPos, sf::Color(40, 200, 80));
	}
	if (endExists && !(previewing && placingTile == endPos && preview == PlacementOption::Clear)) {
		drawMarker(endPos, sf::Color(220, 50, 50));
	}
	if (previewing && preview == PlacementOption::Start) drawMarker(placingTile, sf::Color(40, 200, 80, 170));
	if (previewing && preview == PlacementOption::End) drawMarker(placingTile, sf::Color(220, 50, 50, 170));

	//4) highlights on top: a see-through tint plus an outline drawn inward, so what's underneath stays visible
	auto drawHighlight = [&](int i, sf::Color color, sf::Uint8 tintAlpha, float thickness) {
		if (i < 0 || i >= map.getCellCount()) return; //mouse outside the board
		sf::RectangleShape frame(sf::Vector2f(tileW, tileH));
		frame.setPosition(getTilePos(i));
		frame.setFillColor(sf::Color(color.r, color.g, color.b, tintAlpha));
		frame.setOutlineColor(color);
		frame.setOutlineThickness(-thickness);
		window.draw(frame);
	};
	if (previewing) {
		drawHighlight(placingTile, sf::Color::Cyan, 45, 2.f);
	}
	if (!(previewing && leftMousePos == placingTile)) {
		drawHighlight(leftMousePos, sf::Color::White, 40, 1.5f);
	}
}

int Board::getTileN(float x, float y) {
	//Find the index number starting at the board's top-left corner [index 0] going right then down
	//(-1 when the point is outside the board, e.g. on the side panel)
	x -= origin.x;
	y -= origin.y;
	if (x < 0 || y < 0 || x >= boardW || y >= boardH) {
		return -1;
	}
	int tileX = std::min(map.getColumns() - 1, int(x / tileW));
	int tileY = std::min(map.getRows() - 1, int(y / tileH));
	return map.getIndex(tileX, tileY);
}

sf::Vector2f Board::getTilePos(int N) {
	//Return position of the cell's top-left corner in pixels (in the window, so including the board's margin)
	return sf::Vector2f(origin.x + map.getColumn(N) * tileW, origin.y + map.getRow(N) * tileH);
}
