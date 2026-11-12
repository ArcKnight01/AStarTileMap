#pragma once
#include <SFML/Graphics.hpp>
// Include Needed for Animations
#include <SFML/Graphics/Rect.hpp>
// Include Needed for Text
#include <SFML/Graphics/Text.hpp>
#include <vector>
#include <iostream>
#include <string>
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include "NavGraph.h"
using namespace sf;
using namespace std;

enum tiletype {
	wall,
	ground,
	startnode,
	endnode,
	checkpoint
};

class Board {
public:
	sf::RectangleShape tileShape; //board spaces 

	float boardH, boardW; //board width and height 
	float tileH, tileW; //tile width and height 
	float tileL;

	//rows per tile and col
	int tilesPerRow, tilesPerCol;

	//total number of tiles 
	int tileCount;
	//vector of tile types
	std::vector<tiletype> tileTypes;

	//name of board 
	string boardname;

	int clickPos = 0;
	int leftMousePos = -1;
	int lastTileConverted = 0; //0 = nothing converted yet (the code checks for this)

	bool conversionReset = false; // if true then 
	bool hasBeenConverted = false;
	bool hasClicked = false;
	bool newClick = false;
	bool spacePressed;
	bool newSpace = false;
	bool spaceWasPressed = false;

	bool newEnter = false;
	bool enterPressed;
	bool enterWasPressed = false;

	bool startExists = false;
	bool endExists = false;
	bool firstEmptyTileExists = false;

	bool playerTileExists;
	int playerTile;

	int startPos;
	int endPos;
	int firstEmptyTile = 0;
	bool pieceJumping = false;
	bool blockedTile = false;

	//overloaded constructor: dictates the  board height and width 
	Board(sf::RenderWindow& window, float height, float width, int tilesInRow,  int tilesInCol);

	void drawBoard(sf::RenderWindow& window); //draws board
	void convertTile();
	void createStartNode();
	void createEndNode();
	void exportBoard(string name);
	void importBoard(string& name);
	//A* (Sebastian Lague): tile indices from start to end, empty if there is no path
	std::vector<int> pathfinding(int nodeA, int nodeB);
	//A* cost between two tiles: 10 per straight step, 14 per diagonal step
	int getDistance(int tileA, int tileB);
	//gets the position of the tile from index number (N)
	sf::Vector2f getTilePos(int N);
	//gets the index of the tile from the posiion
	int getTileN(float x, float y);

	// --- Navigation graph (merged from the C4-Template NavGraph) ---
	NavGraph nav;          // walkable tiles + their walkable neighbours
	bool navDirty = true;  // set when a tile changes; main() then rebuilds nav
	tiletype getTile(int x, int y); // off-map counts as wall
	void calcNeighbors(vertex& v);
	void recalculateNav();

	// --- Path display ---
	// pathTiles: the A* path (start -> end), drawn with the blue light.
	// searchedTiles: every tile A* evaluated (its CLOSED set), drawn with the purple light.
	// pathfinding() fills both; drawBoard() draws purple first, then blue on top.
	std::vector<int> pathTiles;
	std::vector<int> searchedTiles;

	sf::Texture wallTexture;
	sf::Sprite wallSprite;

	sf::Texture startTexture;
	sf::Sprite startSprite;

	sf::Texture endTexture;
	sf::Sprite endSprite;

	sf::Texture floorTexture;
	sf::Sprite floorSprite;

	sf::Texture checkPointTexture;
	sf::Sprite checkPointSprite;

	//blue light used to draw pathTiles (yellow is the player's breadcrumb).
	//Other colours available: Textures/light_{red,orange,yellow,green,blue,purple}.png
	sf::Texture pathTexture;
	sf::Sprite pathSprite;

	//purple light used to draw searchedTiles
	sf::Texture searchedTexture;
	sf::Sprite searchedSprite;

};

