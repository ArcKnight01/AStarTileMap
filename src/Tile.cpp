#include "Tile.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include "CostMap.h"
#include "GridMapFile.h"

using namespace std;

vector<string> getMapViewNames() {
	return { "Terrain", "Elevation", "Cost", "Traversability", "Slope", "Roughness", "Step" };
}

Board::Board(float height, float width, sf::Vector2f origin) {
	boardH = height;
	boardW = width;
	this->origin = origin;
	loadSprites();
	map = GridMap(newMapSize, newMapSize, 0.1);
	updateGeometry();
}

//brightness of a pixel (0 .. 255)
static float getBrightness(sf::Color color) {
	return 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;
}

//how bright the average ground pixel is in groundTexture (the rest of the range is left for the lighter pixels)
static const float GROUND_LEVEL = 170.f;

void Board::loadSprites() {
	wallTexture.loadFromFile("Textures/wall.png");
	sf::Image floor;
	floor.loadFromFile("Textures/floorTile.png");
	sf::Vector2u size = floor.getSize();

	//groundTexture: the floor as brightness only, scaled so the average pixel is GROUND_LEVEL
	double brightnessSum = 0;
	for (unsigned int y = 0; y < size.y; y++) {
		for (unsigned int x = 0; x < size.x; x++) brightnessSum += getBrightness(floor.getPixel(x, y));
	}
	float averageBrightness = std::max(1.f, (float)(brightnessSum / (size.x * size.y)));
	sf::Image ground;
	ground.create(size.x, size.y);
	for (unsigned int y = 0; y < size.y; y++) {
		for (unsigned int x = 0; x < size.x; x++) {
			float level = std::min(255.f, getBrightness(floor.getPixel(x, y)) / averageBrightness * GROUND_LEVEL);
			ground.setPixel(x, y, sf::Color((sf::Uint8)level, (sf::Uint8)level, (sf::Uint8)level));
		}
	}
	groundTexture.loadFromImage(ground);

	//a light texture: the sprite's pixels that differ from the floor (its lights); everything else transparent
	auto loadLights = [&](sf::Texture& texture, const std::string& file) {
		sf::Image sprite;
		sprite.loadFromFile(file);
		sf::Image lights;
		lights.create(size.x, size.y, sf::Color::Transparent);
		if (sprite.getSize() == size) {
			for (unsigned int y = 0; y < size.y; y++) {
				for (unsigned int x = 0; x < size.x; x++) {
					if (sprite.getPixel(x, y) != floor.getPixel(x, y)) lights.setPixel(x, y, sprite.getPixel(x, y));
				}
			}
		}
		texture.loadFromImage(lights);
	};
	loadLights(startLightTexture, "Textures/startTile.png");
	loadLights(endLightTexture, "Textures/endTile.png");
	loadLights(pathLightTexture, "Textures/light_blue.png");
	loadLights(searchedLightTexture, "Textures/light_purple.png");
}

void Board::setScreenArea(float width, float height, sf::Vector2f newOrigin) {
	boardW = width;
	boardH = height;
	origin = newOrigin;
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
		// No saved map: start a new flat one (cost 0 everywhere) of the size chosen in the menu
		newMapSize = std::max(MIN_MAP_SIZE, std::min(MAX_MAP_SIZE, newMapSize));
		cout << "No map named \"" << name << "\": starting a new " << newMapSize << " x " << newMapSize
			<< " flat map (SAVE & MENU saves it as " << filename << ")" << endl;
		map = GridMap(newMapSize, newMapSize, 0.1);
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

sf::Color Board::getGroundColor(int index) {
	const sf::Color unknown(0, 0, 0);
	auto layerValue = [&](const char* name) -> float {
		return map.hasLayer(name) ? map.getLayer(name)[index] : GridMap::getUnknownValue();
	};
	switch (view) {
	case MapView::Terrain: {
		//the floor sprite's brown on smooth ground; rougher (costlier) ground makes it darker, then slowly grey like the wall
		const sf::Color brown(64, 55, 33), darkBrown(40, 34, 21), grey(62, 62, 62);
		float c = layerValue("cost");
		if (GridMap::isUnknown(c)) return unknown;
		double rough = c / (1.0 - parameters.traversabilityThreshold); //traversable costs run from 0 to 0.5
		if (rough < 0.5) return blend(brown, darkBrown, rough * 2);
		return blend(darkBrown, grey, (rough - 0.5) * 2);
	}
	case MapView::Elevation: {
		float z = layerValue("elevation");
		if (GridMap::isUnknown(z)) return unknown;
		if (elevationHighest - elevationLowest < 1e-6f) return getHeatColor(0); //a flat map is all one colour
		return getHeatColor((z - elevationLowest) / (elevationHighest - elevationLowest));
	}
	case MapView::Cost: {
		float c = layerValue("cost");
		if (GridMap::isUnknown(c)) return unknown;
		return getHeatColor(c / (1.0 - parameters.traversabilityThreshold)); //traversable costs run from 0 to 0.5
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

	//lowest and highest known elevation, for the Elevation view's colour scale (worked out once per frame)
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

	//which lights each cell shows (start / end > path > searched); a selected cell previews the option being chosen
	enum Light { NoLight, SearchedLight, PathLight, StartLight, EndLight };
	std::vector<Light> lights(map.getCellCount(), NoLight);
	for (int i : searchedTiles) lights[i] = SearchedLight;
	for (int i : pathTiles) lights[i] = PathLight;
	bool previewing = isPlacing && placingTile >= 0 && placingTile < map.getCellCount() && !placementOptions.empty();
	PlacementOption preview = previewing ? placementOptions[placementChoice] : PlacementOption::Clear;
	if (startExists) lights[startPos] = StartLight;
	if (endExists) lights[endPos] = EndLight;
	//previewing a move: the old start / end goes dark while the new cell shows it
	if (previewing && preview == PlacementOption::Start && startExists) lights[startPos] = NoLight;
	if (previewing && preview == PlacementOption::End && endExists) lights[endPos] = NoLight;
	if (previewing && preview == PlacementOption::Clear) lights[placingTile] = NoLight;
	if (previewing && preview == PlacementOption::Start) lights[placingTile] = StartLight;
	if (previewing && preview == PlacementOption::End) lights[placingTile] = EndLight;

	//every cell is one sprite at cell size: lethal cells the stone wall; the others the floor, its brown recoloured by
	//the view (getGroundColor), with the cell's lights on top. One vertex array per texture, so 6400 cells stay fast.
	const std::vector<float>& cost = map.getLayer("cost");
	sf::VertexArray wallCells(sf::Quads), groundCells(sf::Quads);
	sf::VertexArray lightCells[5] = { sf::VertexArray(sf::Quads), sf::VertexArray(sf::Quads), sf::VertexArray(sf::Quads),
		sf::VertexArray(sf::Quads), sf::VertexArray(sf::Quads) };
	auto addQuad = [&](sf::VertexArray& cells, int i, sf::Color color, float textureSize) {
		sf::Vector2f p = getTilePos(i);
		cells.append(sf::Vertex(p, color, sf::Vector2f(0, 0)));
		cells.append(sf::Vertex(p + sf::Vector2f(tileW, 0), color, sf::Vector2f(textureSize, 0)));
		cells.append(sf::Vertex(p + sf::Vector2f(tileW, tileH), color, sf::Vector2f(textureSize, textureSize)));
		cells.append(sf::Vertex(p + sf::Vector2f(0, tileH), color, sf::Vector2f(0, textureSize)));
	};
	const float spriteSize = (float)groundTexture.getSize().x; //20
	const float wallSize = (float)wallTexture.getSize().x;     //10
	for (int i = 0; i < map.getCellCount(); i++) {
		if (isLethal(cost[i], parameters) && lights[i] == NoLight) {
			addQuad(wallCells, i, sf::Color::White, wallSize);
			continue;
		}
		//groundTexture's average pixel is GROUND_LEVEL, so this colour makes the average pixel the ground colour
		sf::Color ground = getGroundColor(i);
		auto scale = [](sf::Uint8 value) { return (sf::Uint8)std::min(255.f, value * 255.f / GROUND_LEVEL); };
		addQuad(groundCells, i, sf::Color(scale(ground.r), scale(ground.g), scale(ground.b)), spriteSize);
		if (lights[i] != NoLight) addQuad(lightCells[lights[i]], i, sf::Color::White, spriteSize);
	}
	window.draw(wallCells, &wallTexture);
	window.draw(groundCells, &groundTexture);
	window.draw(lightCells[SearchedLight], &searchedLightTexture);
	window.draw(lightCells[PathLight], &pathLightTexture);
	window.draw(lightCells[StartLight], &startLightTexture);
	window.draw(lightCells[EndLight], &endLightTexture);

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
