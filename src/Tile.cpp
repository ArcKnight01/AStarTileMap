#include "Tile.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <iostream>
#include "CostMap.h"
#include "GridMapFile.h"

using namespace std;

vector<string> getMapViewNames() {
	return { "Terrain", "Elevation", "Cost", "Traversability", "Slope", "Roughness", "Step" };
}

vector<string> getColorMapNames() {
	return { "Red-yellow-green", "Viridis", "Grayscale" };
}

Board::Board(float height, float width, sf::Vector2f origin) {
	boardH = height;
	boardW = width;
	this->origin = origin;
	loadSprites();
	buildBrushKernels(); //the terrain brush's Gaussian kernels, computed once (TerrainBrush.cpp)
	map = GridMap(newMapSize, newMapSize, 0.1);
	updateGeometry();
}

/**
 * Brightness of a pixel (0 .. 255).
 *
 * @param color  The pixel's colour.
 * @return       Perceived brightness (Rec. 601 luma), 0 .. 255.
 */
static float getBrightness(sf::Color color) {
	return 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;
}

//how bright the average ground pixel is in groundTexture (the rest of the range is left for the lighter pixels)
static const float GROUND_LEVEL = 170.f;

void Board::loadSprites() {
	wallTexture.loadFromFile("Textures/wall.png");
	pitTexture.loadFromFile("Textures/pit.png");
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
	loadLights(waypointLightTexture, "Textures/light_yellow.png");
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
	updatePitCells();
	hasUnsavedChanges = false; //just loaded (or a new map not edited yet)
	navDirty = true;
}

bool Board::isExampleMap(const string& name) {


	// pseudo-code:
	// name in { flat, rugged, dungeon, hybrid }   (the maps tools/generate_maps writes)


	const vector<string> exampleMaps = { "flat", "rugged", "dungeon", "hybrid" };


	return std::find(exampleMaps.begin(), exampleMaps.end(), name) != exampleMaps.end();
}

void Board::exportBoard(string name) {
	saveGridMap("maps/" + name + ".gridmap", map, startExists ? startPos : -1, endExists ? endPos : -1);
	hasUnsavedChanges = false;
}

// ---------------------------------------------------------------------------------------------------------------
// planning
// ---------------------------------------------------------------------------------------------------------------

void Board::replan(PathAlgorithm algorithm) {
	navDirty = false;
	if (!(startExists && endExists)) {
		pathTiles.clear();
		searchedTiles.clear();
		prunedTiles.clear();
		waypoints.clear();
		pathCost = 0;
		return;
	}
	PathResult result = findPath(map, parameters, algorithm, startPos, endPos);
	pathTiles = result.path;
	searchedTiles = result.searchedCells;
	pathCost = result.cost;
	//the waypoints a controller would follow (no goal heading in the app, so the last keeps the last stretch's)
	prunedTiles = prunePath(map, parameters, pathTiles);
	waypoints = makeWaypoints(map, prunedTiles, waypointParameters);
}

// ---------------------------------------------------------------------------------------------------------------
// drawing
// ---------------------------------------------------------------------------------------------------------------

void Board::cycleView() {
	int count = (int)getMapViewNames().size();
	view = (MapView)(((int)view + 1) % count);
}

/**
 * Linear blend between two colours (amount 0 -> a, 1 -> b).
 *
 * @param a       Colour at amount 0.
 * @param b       Colour at amount 1.
 * @param amount  Blend fraction, clamped to 0 .. 1.
 * @return        The blended colour.
 */
static sf::Color blend(sf::Color a, sf::Color b, double amount) {
	amount = std::max(0.0, std::min(1.0, amount));
	return sf::Color((sf::Uint8)(a.r + (b.r - a.r) * amount), (sf::Uint8)(a.g + (b.g - a.g) * amount), (sf::Uint8)(a.b + (b.b - a.b) * amount));
}

sf::Color getColorMapColor(ColorMap colorMap, double amount) {


	// pseudo-code:
	// amount = clamp( amount, 0, 1 )
	// the colour map's stops at even spacing; blend the two stops either side of amount


	amount = std::max(0.0, std::min(1.0, amount));
	switch (colorMap) {
	case ColorMap::RedYellowGreen: {
		const sf::Color good(40, 140, 70), middle(225, 200, 60), bad(210, 60, 40);
		if (amount < 0.5) return blend(good, middle, amount * 2);
		return blend(middle, bad, (amount - 0.5) * 2);
	}
	case ColorMap::Viridis: {
		//matplotlib's viridis at 0, 1/8, ..., 1
		static const sf::Color stops[] = { sf::Color(68, 1, 84), sf::Color(72, 40, 120), sf::Color(62, 74, 137),
			sf::Color(49, 104, 142), sf::Color(38, 130, 142), sf::Color(31, 158, 137), sf::Color(53, 183, 121),
			sf::Color(109, 205, 89), sf::Color(253, 231, 37) };
		const int last = 8;
		int stop = std::min(last - 1, (int)(amount * last));
		return blend(stops[stop], stops[stop + 1], amount * last - stop);
	}
	case ColorMap::Grayscale:
		return blend(sf::Color(20, 20, 20), sf::Color(235, 235, 235), amount); //not pure black, which means unknown
	}
	return sf::Color::Black;
}

sf::Color getLethalColor(ColorMap colorMap) {
	//red-yellow-green already ends in red, so lethal is a darker red; the others have no red, so lethal is plain red
	return colorMap == ColorMap::RedYellowGreen ? sf::Color(110, 15, 20) : sf::Color(200, 35, 35);
}

std::string Board::getViewLayer() const {
	switch (view) {
	case MapView::Terrain:
	case MapView::Elevation:      return "elevation";
	case MapView::Cost:           return "cost";
	case MapView::Traversability: return "traversability";
	case MapView::Slope:          return "slope";
	case MapView::Roughness:      return "roughness";
	case MapView::Step:           return "step";
	}
	return "elevation";
}

void Board::getViewRange(float& lowest, float& highest) const {
	lowest = 0.f;
	switch (view) {
	case MapView::Terrain:
	case MapView::Elevation:      lowest = -elevationExtent; highest = elevationExtent; return;
	case MapView::Cost:           highest = (float)(1.0 - parameters.traversabilityThreshold); return; //traversable costs: 0 .. 0.5
	case MapView::Traversability: highest = 1.f; return;
	case MapView::Slope:          highest = (float)parameters.criticalSlope; return;
	case MapView::Roughness:      highest = (float)parameters.criticalRoughness; return;
	case MapView::Step:           highest = (float)parameters.criticalStep; return;
	}
	highest = 1.f;
}

void Board::updateElevationExtent() {
	elevationExtent = 0.01f; //a flat map still gets a scale (and shows the middle colour)
	if (!map.hasLayer("elevation")) return;
	for (float z : map.getLayer("elevation")) {
		if (!GridMap::isUnknown(z)) elevationExtent = std::max(elevationExtent, std::fabs(z));
	}
}

sf::Color Board::getValueColor(float value) {


	// pseudo-code:
	// unknown                  -> black
	// Cost view and lethal     -> the lethal colour
	// amount = ( value - lowest ) / ( highest - lowest )      (the view's range, getViewRange)
	// Traversability: amount = 1 - amount                      (1 is the good end)
	// Terrain: dark brown (0) -> floor brown (0.5, i.e. 0 m) -> grey (1, the highest);  others: the colour map at amount


	if (GridMap::isUnknown(value)) return sf::Color(0, 0, 0);
	if (view == MapView::Cost && isLethal(value, parameters)) return getLethalColor(colorMap);
	float lowest, highest;
	getViewRange(lowest, highest);
	double amount = (value - lowest) / (highest - lowest);
	if (view == MapView::Traversability) amount = 1.0 - amount;
	if (view == MapView::Terrain) {
		const sf::Color darkBrown(28, 24, 14), brown(64, 55, 33), grey(100, 100, 100); //(was lightBrown(122, 106, 68));
		if (amount < 0.5) return blend(darkBrown, brown, amount * 2);
		return blend(brown, grey, (amount - 0.5) * 2);
	}
	return getColorMapColor(colorMap, amount);
}

sf::Color Board::getGroundColor(int index) {
	std::string layer = getViewLayer();
	return getValueColor(map.hasLayer(layer) ? map.getLayer(layer)[index] : GridMap::getUnknownValue());
	//(was: one case per view; Terrain was the cost as brown -> darker -> grey, Elevation ran from the lowest to the
	// highest height, all with a fixed green -> yellow -> red heat colour)
}

void Board::drawLegend(sf::RenderTarget& window, const sf::Font& font, sf::FloatRect area) {


	// pseudo-code:
	// column, left to right: the view's title written upwards | the colour bar | tick labels
	// bar: top = highest value, bottom = lowest; strips coloured with getValueColor( value at the strip )
	// Cost view: a "lethal" swatch above the bar;  every view: an "unknown" (black) swatch below it


	const float titleWidth = 20, barWidth = 14, labelGap = 6, swatchSize = 14, swatchGap = 10;
	const unsigned int labelSize = 13;
	const sf::Color textColor(220, 220, 220), frameColor(150, 150, 150);
	float barLeft = area.left + titleWidth;
	float barTop = area.top + swatchSize + swatchGap;
	float barBottom = area.top + area.height - swatchSize - swatchGap;
	float labelLeft = barLeft + barWidth + labelGap;

	//every view is drawn on the floor sprite, whose average pixel is the colour scaled by 255 / GROUND_LEVEL
	auto shown = [&](sf::Color color) {
		auto scale = [](sf::Uint8 value) { return (sf::Uint8)std::min(255.f, value * 255.f / GROUND_LEVEL); };
		return sf::Color(scale(color.r), scale(color.g), scale(color.b));
	};
	auto drawLabel = [&](const std::string& words, float x, float centerY) {
		sf::Text label(words, font, labelSize);
		label.setFillColor(textColor);
		sf::FloatRect bounds = label.getLocalBounds();
		label.setOrigin(bounds.left, bounds.top + bounds.height / 2);
		label.setPosition(x, centerY);
		window.draw(label);
	};
	auto drawSwatch = [&](sf::Color color, float top, const std::string& words) {
		sf::RectangleShape swatch(sf::Vector2f(barWidth, swatchSize));
		swatch.setPosition(barLeft, top);
		swatch.setFillColor(color);
		swatch.setOutlineColor(frameColor);
		swatch.setOutlineThickness(1);
		window.draw(swatch);
		drawLabel(words, labelLeft, top + swatchSize / 2);
	};

	//the bar, as thin strips from the top (highest) down
	float lowest, highest;
	getViewRange(lowest, highest);
	const int strips = 96;
	float stripHeight = (barBottom - barTop) / strips;
	sf::VertexArray bar(sf::Quads);
	for (int i = 0; i < strips; i++) {
		float value = highest - (highest - lowest) * (i + 0.5f) / strips;
		sf::Color color = shown(getValueColor(value)); //the Cost bar stops at the worst walkable cost; lethal has its own swatch
		float top = barTop + i * stripHeight;
		bar.append(sf::Vertex(sf::Vector2f(barLeft, top), color));
		bar.append(sf::Vertex(sf::Vector2f(barLeft + barWidth, top), color));
		bar.append(sf::Vertex(sf::Vector2f(barLeft + barWidth, top + stripHeight), color));
		bar.append(sf::Vertex(sf::Vector2f(barLeft, top + stripHeight), color));
	}
	window.draw(bar);
	sf::RectangleShape frame(sf::Vector2f(barWidth, barBottom - barTop));
	frame.setPosition(barLeft, barTop);
	frame.setFillColor(sf::Color::Transparent);
	frame.setOutlineColor(frameColor);
	frame.setOutlineThickness(1);
	window.draw(frame);

	//five ticks: highest, 3/4, 1/2, 1/4, lowest; the unit is on the top label only
	std::string unit;
	int decimals = 2;
	bool signedValues = false;
	switch (view) {
	case MapView::Terrain:
	case MapView::Elevation:      unit = " m"; signedValues = true; break;
	case MapView::Cost:           decimals = 3; break;
	case MapView::Traversability: break;
	case MapView::Slope:          unit = " rad"; break;
	case MapView::Roughness:      unit = " m"; decimals = 3; break;
	case MapView::Step:           unit = " m"; break;
	}
	for (int tick = 0; tick <= 4; tick++) {
		float value = highest - (highest - lowest) * tick / 4.f;
		std::ostringstream out;
		if (signedValues && std::fabs(value) > 1e-6f) out << std::showpos;
		out << std::fixed << std::setprecision(decimals) << (std::fabs(value) < 1e-6f ? 0.f : value);
		if (tick == 0) out << unit;
		float y = barTop + (barBottom - barTop) * tick / 4.f;
		sf::RectangleShape mark(sf::Vector2f(4, 1));
		mark.setPosition(barLeft + barWidth, y);
		mark.setFillColor(frameColor);
		window.draw(mark);
		drawLabel(out.str(), labelLeft, y);
	}

	if (view == MapView::Cost) drawSwatch(shown(getLethalColor(colorMap)), area.top, "lethal");
	drawSwatch(sf::Color::Black, area.top + area.height - swatchSize, "unknown");

	//the title, written upwards along the bar
	std::string title;
	switch (view) {
	case MapView::Terrain:        title = "Terrain: elevation (m), dark brown low, grey high"; break;
	case MapView::Elevation:      title = "Elevation (m), symmetric around 0 m"; break;
	case MapView::Cost:           title = "Cost (0 free .. 0.5 worst walkable)"; break;
	case MapView::Traversability: title = "Traversability (1 ideal .. 0 impassable)"; break;
	case MapView::Slope:          title = "Slope (rad), top = critical"; break;
	case MapView::Roughness:      title = "Roughness (m), top = critical"; break;
	case MapView::Step:           title = "Step height (m), top = critical"; break;
	}
	sf::Text titleText(title, font, 15);
	titleText.setFillColor(textColor);
	sf::FloatRect bounds = titleText.getLocalBounds();
	titleText.setOrigin(bounds.left + bounds.width / 2, bounds.top + bounds.height / 2);
	titleText.setRotation(-90);
	titleText.setPosition(area.left + titleWidth / 2 - 2, (barTop + barBottom) / 2);
	window.draw(titleText);
}

void Board::drawBoard(sf::RenderTarget& window) {
	//outline around the whole map (drawn first; the cells cover its inside)
	sf::RectangleShape outline(sf::Vector2f(boardW, boardH));
	outline.setPosition(origin);
	outline.setFillColor(sf::Color::Transparent);
	outline.setOutlineColor(sf::Color(200, 200, 200));
	outline.setOutlineThickness(4);
	window.draw(outline);

	//the largest |height|, for the Terrain and Elevation views' colour scale (worked out once per frame)
	updateElevationExtent();

	//which lights each cell shows (start / end > path > searched); a selected cell previews the option being chosen
	enum Light { NoLight, SearchedLight, PathLight, WaypointLight, StartLight, EndLight }; //later ones win
	std::vector<Light> lights(map.getCellCount(), NoLight);
	//for (int i : searchedTiles) lights[i] = SearchedLight; (was always lit)
	if (showSearched) {
		for (int i : searchedTiles) lights[i] = SearchedLight;
	}
	//the path: every cell (blue), or with showWaypoints only the cells its waypoints fall in (yellow)
	//(was: for (int i : pathTiles) lights[i] = PathLight; always)
	if (!showWaypoints) {
		for (int i : pathTiles) lights[i] = PathLight;
	}
	else {
		for (const Waypoint& waypoint : waypoints) {
			int column, row;
			if (map.getCell(waypoint.x, waypoint.y, column, row)) lights[map.getIndex(column, row)] = WaypointLight;
		}
	}
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

	//every cell is one sprite at cell size: lethal cells the stone wall or the pit (updatePitCells); the others the floor, its brown recoloured by
	//the view (getGroundColor), with the cell's lights on top. One vertex array per texture, so 6400 cells stay fast.
	//const std::vector<float>& cost = map.getLayer("cost"); (was: to pick the lethal cells for the wall / pit sprites)
	//sf::VertexArray wallCells(sf::Quads), pitCellQuads(sf::Quads), groundCells(sf::Quads);
	sf::VertexArray groundCells(sf::Quads);
	sf::VertexArray lightCells[6] = { sf::VertexArray(sf::Quads), sf::VertexArray(sf::Quads), sf::VertexArray(sf::Quads), sf::VertexArray(sf::Quads),
		sf::VertexArray(sf::Quads), sf::VertexArray(sf::Quads) };
	auto addQuad = [&](sf::VertexArray& cells, int i, sf::Color color, float textureSize) {
		sf::Vector2f p = getTilePos(i);
		cells.append(sf::Vertex(p, color, sf::Vector2f(0, 0)));
		cells.append(sf::Vertex(p + sf::Vector2f(tileW, 0), color, sf::Vector2f(textureSize, 0)));
		cells.append(sf::Vertex(p + sf::Vector2f(tileW, tileH), color, sf::Vector2f(textureSize, textureSize)));
		cells.append(sf::Vertex(p + sf::Vector2f(0, tileH), color, sf::Vector2f(0, textureSize)));
	};
	const float spriteSize = (float)groundTexture.getSize().x; //20
	//const float wallSize = (float)wallTexture.getSize().x;     //10
	//const float pitSize = (float)pitTexture.getSize().x;       //10
	//bool hasPitCells = (int)pitCells.size() == map.getCellCount();
	//no wall / pit sprites for now: every view colours every cell by its number, as a retint of the floor sprite's brown
	//(the texture's average pixel is the colour, which is what the legend shows)
	//(was briefly: the data views as flat colour, only Terrain on the floor sprite)
	for (int i = 0; i < map.getCellCount(); i++) {
		//(was: lethal cells without a light drawn as the wall or pit sprite in every view)
		//if (isLethal(cost[i], parameters) && lights[i] == NoLight) {
		//	if (hasPitCells && pitCells[i]) addQuad(pitCellQuads, i, sf::Color::White, pitSize);
		//	else addQuad(wallCells, i, sf::Color::White, wallSize);
		//	continue;
		//}
		//groundTexture's average pixel is GROUND_LEVEL, so this colour makes the average pixel the ground colour
		sf::Color ground = getGroundColor(i);
		auto scale = [](sf::Uint8 value) { return (sf::Uint8)std::min(255.f, value * 255.f / GROUND_LEVEL); };
		ground = sf::Color(scale(ground.r), scale(ground.g), scale(ground.b));
		addQuad(groundCells, i, ground, spriteSize);
		if (lights[i] != NoLight) addQuad(lightCells[lights[i]], i, sf::Color::White, spriteSize);
	}
	//window.draw(wallCells, &wallTexture);
	//window.draw(pitCellQuads, &pitTexture);
	window.draw(groundCells, &groundTexture);
	window.draw(lightCells[SearchedLight], &searchedLightTexture);
	window.draw(lightCells[PathLight], &pathLightTexture);
	window.draw(lightCells[WaypointLight], &waypointLightTexture);
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

void Board::updatePitCells() {


	// pseudo-code:
	// for each connected area A of lethal cells (8 neighbours), found by a flood fill:
	//     B = walkable cells next to A (4 neighbours)
	//     pit( A ) = mean elevation( A ) < mean elevation( B )      (known elevations only)
	// no elevation layer, or no known elevations in A or B -> wall


	int cellCount = map.getCellCount();
	pitCells.assign(cellCount, false);
	if (!map.hasLayer("elevation")) {
		return;
	}
	const std::vector<float>& cost      = map.getLayer("cost");
	const std::vector<float>& elevation = map.getLayer("elevation");
	std::vector<bool> visited(cellCount, false);


	for (int seed = 0; seed < cellCount; seed++) {
		if (visited[seed] || !isLethal(cost[seed], parameters)) continue;


		//flood fill the lethal area that contains seed, summing its elevation and that of the walkable cells around it
		std::vector<int> area, stack = { seed };
		double areaSum = 0, borderSum = 0;
		int areaKnown = 0, borderKnown = 0;
		std::vector<int> borderCells;
		visited[seed] = true;
		while (!stack.empty()) {
			int cell = stack.back();
			stack.pop_back();
			area.push_back(cell);
			if (!GridMap::isUnknown(elevation[cell])) { areaSum += elevation[cell]; areaKnown++; }
			int column = map.getColumn(cell), row = map.getRow(cell);
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					if ((dx == 0 && dy == 0) || !map.isInside(column + dx, row + dy)) continue;
					int neighbour = map.getIndex(column + dx, row + dy);
					if (isLethal(cost[neighbour], parameters)) {
						if (!visited[neighbour]) { visited[neighbour] = true; stack.push_back(neighbour); }
					}
					else if (dx == 0 || dy == 0) {
						borderCells.push_back(neighbour); //a walkable cell beside the area (may be counted twice; fine for a mean)
					}
				}
			}
		}
		for (int cell : borderCells) {
			if (!GridMap::isUnknown(elevation[cell])) { borderSum += elevation[cell]; borderKnown++; }
		}


		bool isPit = areaKnown > 0 && borderKnown > 0 && areaSum / areaKnown < borderSum / borderKnown;
		for (int cell : area) pitCells[cell] = isPit;
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
