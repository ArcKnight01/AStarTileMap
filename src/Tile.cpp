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

	for (int i = 0; i < tileCount; i++) {
			tileTypes[i] = ground;
	}
	pathfinding(getTilePos(1), getTilePos(9));
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

	//2) light up the path (blue sprite); start/end keep their green/red sprites
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
void Board::pathfinding(Vector2f positionA, Vector2f positionB)
{
	//initial node where the start position is in
	int nodeA = getTileN(positionA.x, positionA.y);
	//target node where the end position is in
	int nodeB = getTileN(positionA.x, positionA.y);

	float distBtwnAdjTiles = tileL; //10 units for example
	float distBtwnAdj = distBtwnAdjTiles * sqrtf(2); // ~= to 14 units

	//vector of all nodes (tiles)
	vector<int> totalNodes = std::vector<int>(tileCount); 

	//OPEN : the set of nodes to be evaluated
	vector<int> openNodes;
	//CLOSED : the set of nodes already evaluated
	vector<int> closedNodes;

	//vector initialization: 
	/*
	//openNodes = std::vector<int>(tileCount);
	//closedNodes = std::vector<int>(tileCount);
	*/
	for (int i = 0; i < openNodes.size(); i++) {
		cout << "open[" << i << "]: " << openNodes[i] << endl; //prints number at index i
	}
	for (int i = 0; i < closedNodes.size(); i++) {
		cout << "closed[" << i << "]: " << closedNodes[i] << endl; //prints number at index i
	}

	//add the start node to OPEN:
	openNodes.push_back(nodeA);

	for (int i = 0; i < openNodes.size(); i++) {
		cout << "open[" << i << "]: " << openNodes[i] << endl; //prints number at index i
	}
	for (int i = 0; i < closedNodes.size(); i++) {
		cout << "closed[" << i << "]: " << closedNodes[i] << endl; //prints number at index i
	}


	

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