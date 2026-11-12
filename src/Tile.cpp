#include "Tile.h"


using namespace std;
Board::Board(sf::RenderWindow& window, float height, float width, int tilesInRow, int tilesInCol) {
	
	boardH = height;
	boardW = width;

	tilesPerRow = tilesInRow;
	tilesPerCol = tilesInCol;

	tileCount = tilesPerCol * tilesPerRow;
	tileH = boardH / tilesPerCol;
	tileW = boardW / tilesPerRow;
	tileL = (tileH == tileW) ? tileH : (tileH > tileW) ? tileH : tileW;
	tileShape = sf::RectangleShape(sf::Vector2f(tileW, tileH));
	//Generate vector for tiles
	tileTypes = std::vector<tiletype>(tileCount);

	
	sf::IntRect rect[2];
	rect[0] = sf::IntRect(0, 0, 10, 10); //section of sprite image
	rect[1] = sf::IntRect(0, 0, 20, 20);

	wallTexture.loadFromFile("Textures/wall.png");
	wallSprite.setTexture(wallTexture);
	wallSprite.setTextureRect(rect[0]); //make section of sprite image apear
	wallSprite.setScale(tileW/10, tileH/10);
	
	startTexture.loadFromFile("Textures/startTile.png");
	startSprite.setTexture(startTexture);
	startSprite.setTextureRect(rect[1]); //make section of sprite image apear
	startSprite.setScale(tileW/20, tileH/20); //20*20 multiplied by 5,5 = 100x100 //scale = 100(tileH) / 20 = 5

	endTexture.loadFromFile("Textures/endTile.png");
	endSprite.setTexture(endTexture);
	endSprite.setTextureRect(rect[1]); //make section of sprite image apear
	endSprite.setScale(tileW/20, tileH/20);

	
	checkPointTexture.loadFromFile("Textures/checkPointTile.png");
	checkPointSprite.setTexture(checkPointTexture);
	checkPointSprite.setTextureRect(rect[1]); //make section of sprite image apear
	checkPointSprite.setScale(tileW/20, tileH/20);

	floorTexture.loadFromFile("Textures/floorTile.png");
	floorSprite.setTexture(floorTexture);
	floorSprite.setTextureRect(rect[1]); //make section of sprite image apear
	floorSprite.setScale(tileW / 20, tileH / 20);

	pathTexture.loadFromFile("Textures/light_blue.png");
	pathSprite.setTexture(pathTexture);
	pathSprite.setTextureRect(rect[1]);
	pathSprite.setScale(tileW / 20, tileH / 20);

	searchedTexture.loadFromFile("Textures/light_purple.png");
	searchedSprite.setTexture(searchedTexture);
	searchedSprite.setTextureRect(rect[1]);
	searchedSprite.setScale(tileW / 20, tileH / 20);

	for (int i = 0; i < tileCount; i++) {
			tileTypes[i] = ground;
	}
}

void Board::drawBoard(sf::RenderWindow& window) {
	hasBeenConverted = (hasClicked && (spaceWasPressed || enterWasPressed) && clickPos == lastTileConverted);

	//1) every tile always draws its own sprite, so its state stays visible under any highlight
	for (int i = 0; i < tileCount; i++) {
		sf::Sprite* sprite = &floorSprite;
		switch (tileTypes[i]) {
		case wall:       sprite = &wallSprite;       break;
		case startnode:  sprite = &startSprite;      break;
		case endnode:    sprite = &endSprite;        break;
		case checkpoint: sprite = &checkPointSprite; break;
		case ground:     sprite = &floorSprite;      break;
		}
		sprite->setPosition(getTilePos(i));
		window.draw(*sprite);
	}

	//2) light up the tiles A* searched (purple), then the path itself (blue) on top;
	//   start/end keep their green/red sprites
	for (int i : searchedTiles) {
		if (i >= 0 && i < tileCount && tileTypes[i] != startnode && tileTypes[i] != endnode && tileTypes[i] != wall) {
			searchedSprite.setPosition(getTilePos(i));
			window.draw(searchedSprite);
		}
	}
	for (int i : pathTiles) {
		if (i >= 0 && i < tileCount && tileTypes[i] != startnode && tileTypes[i] != endnode && tileTypes[i] != wall) {
			pathSprite.setPosition(getTilePos(i));
			window.draw(pathSprite);
		}
	}

	//3) highlights go ON TOP of the tile: a see-through tint plus an outline drawn inward
	//   (negative thickness), so the sprite underneath and the neighbouring tiles stay visible
	auto drawHighlight = [&](int i, sf::Color color, sf::Uint8 tintAlpha, float outline) {
		if (i < 0 || i >= tileCount) return; //mouse outside the board
		sf::RectangleShape box(sf::Vector2f(tileW, tileH));
		box.setPosition(getTilePos(i));
		box.setFillColor(sf::Color(color.r, color.g, color.b, tintAlpha));
		box.setOutlineColor(color);
		box.setOutlineThickness(-outline);
		window.draw(box);
	};

	//selected tile (same rule as before for when the selection shows: it hides right after
	//the tile is converted, until you click again)
	bool showSelection = hasClicked && !spacePressed && !enterPressed &&
		((hasBeenConverted == false && (clickPos != lastTileConverted || (lastTileConverted == 0 && (!spaceWasPressed || !enterWasPressed))))
		|| (clickPos == lastTileConverted && hasBeenConverted && conversionReset));
	if (showSelection) {
		drawHighlight(clickPos, sf::Color::Cyan, 45, 5.f);
	}
	//hovered tile (skipped when it is the visible selection, which already has a frame)
	if (!(showSelection && leftMousePos == clickPos)) {
		drawHighlight(leftMousePos, sf::Color::White, 40, 3.f);
	}
}

tiletype Board::getTile(int x, int y) {
	if (x < 0 || x >= tilesPerRow || y < 0 || y >= tilesPerCol) {
		return wall;
	}
	return tileTypes[y * tilesPerRow + x];
}

void Board::calcNeighbors(vertex& v) {
	int x = v.coords.first;
	int y = v.coords.second;

	int coords_to_check[8][2] = { {-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1} };

	for (auto coord : coords_to_check) {
		int nx = x + coord[0];
		int ny = y + coord[1];
		if (getTile(nx, ny) != wall) {
			// If the tile is pathable, add it (its real coordinates) to this vertex's connections
			v.adjacent.push_back(std::make_pair(nx, ny));
		}
	}
}

void Board::recalculateNav() {
	nav.clear();
	for (int y = 0; y < tilesPerCol; y++) {
		for (int x = 0; x < tilesPerRow; x++) {
			if (getTile(x, y) != wall) {
				vertex v;
				v.coords = std::make_pair(x, y);
				calcNeighbors(v);
				nav.addVertex(x, y, v);
			}
		}
	}
	navDirty = false;
}
void Board::convertTile() {
	sf::Vector2f tilePos;
	for (int i = 0; i < tileCount; i++) {
		tilePos = getTilePos(i);
		if (i == clickPos && hasClicked && spacePressed && conversionReset == true) {//&&(i != lastTileConverted || (lastTileConverted == 0 && !spaceWasPressed))
			//conversionReset is true if mouse was clicked once and hasnt already been converted without clicking again
			if (tileTypes[i] == ground) {
				tileTypes[i] = wall;
			}
			else if (tileTypes[i] == wall) {
				tileTypes[i] = ground;
			}
			else if ((tileTypes[i] == startnode && startExists) || (tileTypes[i] == endnode && endExists)) {
				if (tileTypes[i] == startnode) {
					startExists = false;
				}
				else if (tileTypes[i] == endnode) {
					endExists = false;
				}
				tileTypes[i] = ground;
			}
			lastTileConverted = i; //which means lastTileConverted == clickPos
			navDirty = true; //a tile changed, so the nav graph must be rebuilt
			hasBeenConverted = true;
			conversionReset = false;
		}
		else if (i == clickPos && hasClicked && enterPressed && conversionReset == true) {//&&(i != lastTileConverted || (lastTileConverted == 0 && !spaceWasPressed))
			//conversionReset is true if mouse was clicked once and hasnt already been converted without clicking again
			if (tileTypes[i] == ground) {
				if (!startExists) {
					tileTypes[i] = startnode;
					startPos = i;
					startExists = true;
				} 
				else if (!endExists) {
					tileTypes[i] = endnode;
					endPos = i;
					endExists = true;
				}
				else if (startExists && endExists) {
					tileTypes[i] = ground;
				}
			}
			else if ((tileTypes[i] == startnode && startExists ) || (tileTypes[i] == endnode && endExists)) {
				if (tileTypes[i] == startnode) {
					startExists = false;
				}
				else if (tileTypes[i] == endnode){
					endExists = false;
				}
				tileTypes[i] = ground;
			}
			else if (tileTypes[i] == wall) {
				tileTypes[i] = wall;
			}
			lastTileConverted = i; //which means lastTileConverted == clickPos
			navDirty = true; //a tile changed, so the nav graph must be rebuilt
			hasBeenConverted = true;
			conversionReset = false;
		}
	}
}
void Board::createStartNode() {
	if (startExists) {

	}
}
void Board::createEndNode() {
	if (endExists) {

	}
}
void Board::exportBoard(string name)
{
	// vector of tile numbers, 0 -> ground, 1 -> wall.
	vector<int> tileNumbers;
	tileNumbers = std::vector<int>(tileCount);

	fstream file;
	string filename = "maps/" + name + ".txt";
	file.open(filename, ios::out);

	//file << boardH << endl;
	//file << boardW << endl;
	//file << tilesPerRow << endl;
	//file << tilesPerCol << endl;

	for (int i = 0; i < tileCount; i++) {
		if (tileTypes[i] == ground) {
			tileNumbers[i] = 0;
		}
		else if (tileTypes[i] == wall) {
			tileNumbers[i] = 1;
		}
		else if (tileTypes[i] == startnode) {
			tileNumbers[i] = 2;
		}
		else if (tileTypes[i] == endnode) {
			tileNumbers[i] = 3;
		}
	}

	for (int i = 0; i < tileCount; i++) {
		file << tileNumbers[i] << endl;
	}

	file.close();
}
void Board::importBoard(string& name) {
	//create a new board or import a saved board
	fstream file;
	cout << "Enter name of map (size 20x20): ";
	getline(cin, name);

	vector<int> tileNumbers;
	tileNumbers = std::vector<int>(tileCount);

	// If they enter no name, use a default
	if (name == "") {
		name = "default";
	}
	string filename = "maps/" + name + ".txt";

	file.open(filename, ios::in);
	if (file.is_open()) {
		// File exists, read data from file
		cout << "File found, loading from file." << endl;

		//file >> boardH;
		//file >> boardW;
		//file >> tilesPerRow;
		//file >> tilesPerCol;
		//tileCount = tilesPerRow * tilesPerCol;

		for (int i = 0; i < tileCount; i++) {
			file >> tileNumbers[i];
		}

		for (int i = 0; i < tileCount; i++) {
			if (tileNumbers[i] == 0) {
				tileTypes[i] = ground;
				if (!firstEmptyTileExists) {
					firstEmptyTile = i;
					firstEmptyTileExists = true;
				}
			}
			else if (tileNumbers[i] == 1) {
				tileTypes[i] = wall;
			}
			else if (tileNumbers[i] == 2) {
				tileTypes[i] = startnode;
				startPos = i;
				startExists = true;
			}
			else if (tileNumbers[i] == 3) {
				tileTypes[i] = endnode;
				endPos = i;
				endExists = true;
			}
		}

	}
	else {
		// File doesn't exist, have the user fill it in
		for (int i = 0; i < tileCount; i++) {
			if (i % tilesPerRow == 0 || i % tilesPerRow == (tilesPerRow-1) || i / tilesPerCol == 0 || i/tilesPerCol == (tilesPerCol-1)) {
				tileTypes[i] = wall;
			}
			else {
				tileTypes[i] = ground;
				if (!firstEmptyTileExists) {
					firstEmptyTile = i;
					firstEmptyTileExists = true;
				}
			}
		}
	}
	navDirty = true; //new map loaded, rebuild the nav graph
}
// Distance between two tiles in A* cost units: 10 per straight step, 14 per diagonal step
// (14 ~= 10 * sqrt(2)). Same as Sebastian Lague's GetDistance().
int Board::getDistance(int tileA, int tileB)
{
	int dstX = abs(tileA % tilesPerRow - tileB % tilesPerRow);
	int dstY = abs(tileA / tilesPerRow - tileB / tilesPerRow);
	if (dstX > dstY) {
		return 14 * dstY + 10 * (dstX - dstY);
	}
	return 14 * dstX + 10 * (dstY - dstX);
}

// A* path planning, following Sebastian Lague's
// "A* Pathfinding (E01: algorithm explanation)": https://www.youtube.com/watch?v=-L-WgKMFuhE
// Simplest version: OPEN is a plain list searched for the lowest F cost each step.
// Returns the tile indices from nodeA (start) to nodeB (end), both included,
// or an empty list if there is no path.
std::vector<int> Board::pathfinding(int nodeA, int nodeB)
{
	std::vector<int> path;
	searchedTiles.clear();
	if (nodeA < 0 || nodeA >= tileCount || nodeB < 0 || nodeB >= tileCount) {
		return path;
	}
	if (navDirty) {
		recalculateNav();
	}

	//per-tile node data
	//G cost = distance from the start, H cost = estimated distance to the end, F cost = G + H
	std::vector<int> gCost(tileCount, 0);
	std::vector<int> hCost(tileCount, 0);
	std::vector<int> parent(tileCount, -1);   //tile we came from (-1 = none)
	std::vector<bool> inOpen(tileCount, false);
	std::vector<bool> inClosed(tileCount, false);

	//OPEN : the set of nodes to be evaluated
	std::vector<int> openNodes;
	//CLOSED : the set of nodes already evaluated (tracked with inClosed)

	//add the start node to OPEN:
	openNodes.push_back(nodeA);
	inOpen[nodeA] = true;
	hCost[nodeA] = getDistance(nodeA, nodeB);

	while (!openNodes.empty()) {
		//current = the node in OPEN with the lowest F cost (if tied, the lowest H cost)
		int best = 0;
		for (int k = 1; k < (int)openNodes.size(); k++) {
			int candidate = openNodes[k];
			int current = openNodes[best];
			int fCandidate = gCost[candidate] + hCost[candidate];
			int fCurrent = gCost[current] + hCost[current];
			if (fCandidate < fCurrent || (fCandidate == fCurrent && hCost[candidate] < hCost[current])) {
				best = k;
			}
		}
		int current = openNodes[best];

		//remove current from OPEN, add it to CLOSED
		openNodes.erase(openNodes.begin() + best);
		inOpen[current] = false;
		inClosed[current] = true;
		searchedTiles.push_back(current); //shown in purple

		//path has been found: retrace it from the end back to the start
		if (current == nodeB) {
			for (int tile = nodeB; tile != -1; tile = parent[tile]) {
				path.push_back(tile);
			}
			std::reverse(path.begin(), path.end());
			return path;
		}

		//for each neighbour of the current node (walls are never in the nav graph)
		vertex v;
		if (!nav.getVertex(current % tilesPerRow, current / tilesPerRow, v)) {
			continue;
		}
		for (const gridcoord& coord : v.adjacent) {
			int neighbour = coord.second * tilesPerRow + coord.first;

			//skip it if it has already been evaluated
			if (inClosed[neighbour]) {
				continue;
			}

			//if the new path to the neighbour is shorter, or the neighbour is not in OPEN yet
			int newCostToNeighbour = gCost[current] + getDistance(current, neighbour);
			if (newCostToNeighbour < gCost[neighbour] || !inOpen[neighbour]) {
				gCost[neighbour] = newCostToNeighbour;
				hCost[neighbour] = getDistance(neighbour, nodeB);
				parent[neighbour] = current;

				if (!inOpen[neighbour]) {
					openNodes.push_back(neighbour);
					inOpen[neighbour] = true;
				}
			}
		}
	}

	//OPEN ran out without reaching the end: no path exists
	return path;
}
int Board::getTileN(float x, float y) {
	//Find the index number starting at (0,0) [index 0]going right then down 
	int tileX, tileY;
	tileX = int(x / tileW);
	tileY = int(y / tileH);
	return tileX + (tileY * tilesPerCol);
}

sf::Vector2f Board::getTilePos(int N) {
	//Return position of tile's origin in pixels
	float posX, posY;
	posX = (N % tilesPerRow) * tileW;
	posY = (N / tilesPerCol) * tileH;
	return sf::Vector2f(posX, posY);
}


/*	fstream file;
	string name;
	cout << "Map Name: ";
	getline(cin, name);

	// If they enter no name, use a default
	if (name == "") {
		name = "Unamed_Map";
	}
	string filename = name + ".txt";

	file.open(filename, ios::in);
	if (file.is_open()) {
		// File exists, read data from file
		cout << "File found, loading from file." << endl;

		//file >> str;
	}
	else {
	for (int i = 0; i < tileCount; i++) {

	}

	*/


	//for (int i = 0; i < tileCount; i++) {
		//Try running through the first few iterations, what happens when 8 <= i < 16?
		//if ((i % 2 - (i / tilesPerRow) % 2) == 0) {
	//	tileTypes[i] = ground;
		//}
		//else {
		//tileTypes[i] = wall;
		//}
	//}
/*
void setupPlayerStats(string& name, int& str, int& def, int& armor, int& skill, int& wins) {
	// Load the character's file or create a new character.
	fstream file;

	cout << "Map Name: ";
	getline(cin, name);

	// If they enter no name, use a default
	if (name == "") {
		name = "Unamed_Map";
	}
	string filename = name + ".txt";

	file.open(filename, ios::in);
	if (file.is_open()) {
		// File exists, read data from file
		cout << "File found, loading from file." << endl;

		file >> str;
	}
	else {
		// File doesn't exist, have the user fill it in
		bool accepted = false;
		while (!accepted) {
			
			if () {
				accepted = true;
			}
			else {
				cout << "Not Accepted!" << endl;
			}
		}
	}
}

void savePlayerStats(string name, int str, int def, int armor, int skill, int wins) {
	// Save the player's stats to a file.
	fstream file;
	string filename = name + ".txt";

	file.open(filename, ios::out);

	file << str << endl;
	file << def << endl;
	file << armor << endl;
	file << skill << endl;
	file << wins << endl;

	file.close();
}
*/