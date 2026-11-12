#pragma once
#include <SFML/Graphics.hpp>
// Include Needed for Animations
#include <SFML/Graphics/Rect.hpp>
// Include Needed for Text
#include <SFML/Graphics/Text.hpp>
#include <vector>
#include <string>
#include "CostMapParameters.h"
#include "GridMap.h"
#include "PathPlanner.h"
using namespace sf;
using namespace std;

// costmap branch: the Board is the SFML view and editor of a core GridMap (core/ has no SFML).
// It loads and saves maps/<name>.gridmap, draws one layer, lets you place the start and end, and asks the core to plan
// on the "cost" layer. (The tile-based Board, NavGraph and planners of the main branch are replaced by core/.)

// What the map shows. V cycles through these on the PLAYING screen.
enum class MapView {
	Terrain,        //elevation, brown (low) to grey (high); lethal cells drawn with the stone wall texture
	Cost,           //the cost layer the planner uses: green (0) -> yellow -> red (0.5); lethal = stone
	Traversability, //0 (red) .. 1 (green)
	Slope,          //0 .. criticalSlope
	Roughness,      //0 .. criticalRoughness
	Step            //0 .. criticalStep
};
vector<string> getMapViewNames();

// What can be placed on a selected cell (the list beside it).
enum class PlacementOption {
	Start,
	End,
	Clear //remove the start / end from this cell
};

class Board {
public:
	GridMap map;                  //all layers ("elevation", ..., "cost"); the planner reads "cost"
	CostMapParameters parameters; //lethal cost, cost weight, etc. (core/CostMapParameters.h)

	float boardH, boardW; //board width and height in pixels
	float tileH, tileW;   //cell width and height in pixels (board size / map size)
	sf::Vector2f origin;  //top-left corner of the board in the window (the margin around the map)

	//name of board
	string boardname;
	bool mapIsNew = false; //true when importBoard() found no saved map and started a new flat one

	int leftMousePos = -1; //cell under the mouse (-1 = outside the board)

	bool startExists = false;
	bool endExists = false;
	int startPos = -1;
	int endPos = -1;

	MapView view = MapView::Terrain;

	// --- path display ---
	// pathTiles: the path (start -> end), drawn blue. searchedTiles: every cell the algorithm evaluated, drawn purple.
	std::vector<int> pathTiles;
	std::vector<int> searchedTiles;
	double pathCost = 0;   //total step cost of the path (Rajput eq. 3.8)
	bool navDirty = true;  //set when start / end / the map change; main() then re-plans

	Board(float height, float width, sf::Vector2f origin);

	//loads maps/<name>.gridmap, or starts a new flat map if it doesn't exist. name is cleaned first (cleanMapName)
	void importBoard(string& name);
	void exportBoard(string name);
	//names of the maps saved in maps/ (".gridmap" files, without the extension), sorted
	static vector<string> getSavedMapNames();
	//"rugged", "rugged.gridmap", "maps/rugged" and " rugged " all become "rugged"; "" becomes "flat"
	static string cleanMapName(string name);

	//plans on the cost layer with the chosen algorithm and stores the path / searched cells for drawing
	void replan(PathAlgorithm algorithm);

	void drawBoard(sf::RenderWindow& window); //draws the chosen view, the search, the path, start / end, highlights
	void cycleView();

	//gets the position of the cell from index number (N), in the window
	sf::Vector2f getTilePos(int N);
	//gets the index of the cell under a window position (-1 if the position is outside the board)
	int getTileN(float x, float y);

	// --- placing the start and end (TilePlacement.cpp) ---
	// Click a cell to select it, pick from the list with Up / Down (the cell previews it), click it again (or Enter) to
	// place, right-click / Backspace to unselect. Start and End are only offered while they don't exist yet, and never on
	// a lethal cell; Clear is offered on the cell that is the start or end.
	bool isPlacing = false;
	int placingTile = -1;
	std::vector<PlacementOption> placementOptions;
	int placementChoice = 0;
	void beginPlacement(int tile);
	void movePlacementChoice(int step);
	void confirmPlacement();
	void cancelPlacement();
	static string getPlacementName(PlacementOption option);

	sf::Texture wallTexture; //stone, for lethal cells

private:
	void updateGeometry(); //tile size from the board size and the map size
	sf::Color getCellColor(int index);
	float elevationLowest = 0.f, elevationHighest = 0.f; //the Terrain view's colour range (set in drawBoard)
};
