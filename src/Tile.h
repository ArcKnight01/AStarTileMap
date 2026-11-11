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
	void pathfinding(Vector2f positionA, Vector2f positionB);
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
	// Fill this with tile indices (start -> end) and drawBoard() lights them up
	// with the blue path sprite. Nothing fills it yet: that's A*'s job.
	std::vector<int> pathTiles;

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

};

