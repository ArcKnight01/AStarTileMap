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
#include <filesystem>
#include <cctype>
#include "NavGraph.h"
#include "Pathfinding.h"
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
	sf::Vector2f origin; //top-left corner of the board in the window (the margin around the map)

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
	bool mapIsNew = false; //true when importBoard() found no saved map and started an empty one

	bool playerTileExists;
	int playerTile;

	int startPos;
	int endPos;
	int firstEmptyTile = 0;
	bool pieceJumping = false;
	bool blockedTile = false;

	//overloaded constructor: dictates the  board height and width, and where its top-left corner sits in the window
	Board(sf::RenderWindow& window, float height, float width, int tilesInRow,  int tilesInCol, sf::Vector2f origin = sf::Vector2f(0, 0));

	void drawBoard(sf::RenderWindow& window); //draws board (and an outline around it)
	void convertTile();
	void createStartNode();
	void createEndNode();
	void exportBoard(string name);
	//loads maps/<name>.txt, or starts an empty map with a wall border if it doesn't exist.
	//name is cleaned first (see cleanMapName) and left holding the cleaned name.
	void importBoard(string& name);
	//names of the maps saved in maps/ (without ".txt"), sorted
	static vector<string> getSavedMapNames();
	//"dungeon", "dungeon.txt", "maps/dungeon" and " dungeon " all become "dungeon"; "" becomes "default"
	static string cleanMapName(string name);

	// --- Placing tiles (TilePlacement.cpp) ---
	// Click a tile to select it, pick a type with Up / Down (the tile previews it), click it again (or Enter) to place,
	// right-click or Backspace to unselect. Start and End are only offered while they don't exist yet (or on the tile that
	// already is the start / end), so there is never more than one of each.
	bool isPlacing = false;              //a tile is selected and waiting for a type
	int placingTile = -1;                //the selected tile
	std::vector<tiletype> placementOptions; //what can be placed on it (ground, wall, and start / end when allowed)
	int placementChoice = 0;             //index into placementOptions (the one previewed)
	void beginPlacement(int tile);       //select a tile and build its list of options
	void movePlacementChoice(int step);  //Up = -1, Down = +1 (wraps around)
	void confirmPlacement();             //place the previewed type on the selected tile
	void cancelPlacement();              //unselect without changing anything
	tiletype getPlacementPreview();      //the type currently previewed
	static string getTileTypeName(tiletype type);
	sf::Sprite& getSpriteFor(tiletype type); //the sprite that draws this tile type

	// --- Path planning (Pathfinding.cpp) ---
	//runs the chosen algorithm: tile indices from start to end, empty if there is no path.
	//Every algorithm also fills searchedTiles with the tiles it evaluated.
	std::vector<int> findPath(PathAlgorithm algorithm, int nodeA, int nodeB);
	//A* (Sebastian Lague): tile indices from start to end, empty if there is no path
	std::vector<int> pathfinding(int nodeA, int nodeB);
	//Dijkstra's: like A* without the H cost, so it spreads out evenly; also finds the cheapest path
	std::vector<int> pathfindingDijkstra(int nodeA, int nodeB);
	//breadth-first search: fewest steps (ignores that diagonals cost more)
	std::vector<int> pathfindingBFS(int nodeA, int nodeB);
	//depth-first search: follows one direction as far as it can; finds a path, usually not a short one
	std::vector<int> pathfindingDFS(int nodeA, int nodeB);
	//A* cost between two tiles: 10 per straight step, 14 per diagonal step
	int getDistance(int tileA, int tileB);
	//gets the position of the tile from index number (N)
	sf::Vector2f getTilePos(int N);
	//gets the index of the tile from the posiion (-1 if the position is outside the board)
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

