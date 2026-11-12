#pragma once
#include <SFML/Graphics.hpp>
// Include Needed for Animations
#include <SFML/Graphics/Rect.hpp>
// Include Needed for Text
#include <SFML/Graphics/Text.hpp>
#include <map>
#include <vector>
#include <string>
#include "CostMapParameters.h"
#include "GridMap.h"
#include "PathPlanner.h"
#include "TerrainBrush.h"
using namespace sf;
using namespace std;

// costmap branch: the Board is the SFML view and editor of a core GridMap (core/ has no SFML).
// It loads and saves maps/<name>.gridmap, draws one layer, lets you place the start and end, and asks the core to plan
// on the "cost" layer. (The tile-based Board, NavGraph and planners of the main branch are replaced by core/.)

// What the map shows. V cycles through these on the PLAYING screen.
enum class MapView {
	Terrain,        //the floor sprite: its brown gets darker, then grey, as the ground gets rougher (cost 0 -> 0.5)
	Elevation,      //height: low (green) -> high (red)
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

	//where the board is drawn in the window, and how big (pixels); the cell size follows from the map size
	void setScreenArea(float width, float height, sf::Vector2f newOrigin);

	void drawBoard(sf::RenderWindow& window); //draws the chosen view, the search, the path, start / end, highlights
	void cycleView();

	//gets the position of the cell from index number (N), in the window
	sf::Vector2f getTilePos(int N);
	//gets the index of the cell under a window position (-1 if the position is outside the board)
	int getTileN(float x, float y);

	// --- placing the start and end (TilePlacement.cpp) ---
	// Click a cell to select it, pick from the list with Up / Down (the cell previews it), click it again (or Enter) to
	// place, right-click / Backspace to unselect. Start and End are offered on every walkable cell (they move there if
	// they already exist), never on a lethal cell; Clear is offered on the cell that is the start or end.
	bool isPlacing = false;
	int placingTile = -1;
	std::vector<PlacementOption> placementOptions;
	int placementChoice = 0;
	void beginPlacement(int tile);
	void movePlacementChoice(int step);
	void confirmPlacement();
	void cancelPlacement();
	static string getPlacementName(PlacementOption option);
	string getPlacementLabel(PlacementOption option) const; //as shown in the list: "Move start" when the start exists

	// --- editing the terrain with a brush (TerrainBrush.cpp) ---
	// With "Left click: Set terrain", a click or drag calls setTerrainHeight() on the cell under the mouse. The cost map
	// is rebuilt once per frame by updateCostMap() (main() calls it when terrainEditedTiles is not empty), then re-planned.
	std::vector<int> terrainEditedTiles; //cells whose elevation changed since the last updateCostMap()
	std::map<int, std::vector<float>> brushKernels; //Gaussian kernel of each brush width (TerrainBrush.h), built once
	/**
	 * Build the Gaussian kernel of every brush width offered in the Brush dropdown into brushKernels.
	 */
	void buildBrushKernels();
	/**
	 * Move the elevation of every cell under a square Gaussian brush centred on a cell part of the way toward a height:
	 * z <- z + strength * w * ( elevation - z ), with w the brush kernel's weight for that cell (1 at the centre).
	 *
	 * @param tile        Index of the cell under the mouse (row * columns + column).
	 * @param elevation   Target height of the brush, in metres.
	 * @param brushWidth  Side of the square brush, in cells (one of getBrushWidths()). Cells outside the map are skipped.
	 * @param strength    Fraction of the gap closed at the centre per call, 0 .. 1 (1: the centre reaches the target).
	 * @return            Nothing. The brushed cells are added to terrainEditedTiles.
	 */
	void setTerrainHeight(int tile, float elevation, int brushWidth, float strength);
	/**
	 * Rebuild the derived layers (slope, roughness, step, traversability, cost) around the cells in terrainEditedTiles.
	 *
	 * The whole pipeline is run on a copy of the map, and only the cells an edit can reach are copied back, so on a map
	 * built by the pipeline the result equals a full re-run, while a map whose cost layer was written directly (dungeon)
	 * keeps its cost everywhere else.
	 *
	 * @return  Nothing. Clears terrainEditedTiles and sets navDirty so the path is planned again.
	 */
	void updateCostMap();
	/**
	 * Draw the outline of the square brush around a cell (the cells setTerrainHeight() would change).
	 *
	 * @param window      Window to draw into.
	 * @param tile        Index of the cell under the mouse (-1 draws nothing).
	 * @param brushWidth  Side of the square brush, in cells.
	 */
	void drawTerrainBrush(sf::RenderWindow& window, int tile, int brushWidth);

	//size of a new map (cells per side) when importBoard() finds no saved map; set from the menu. Saved maps keep their size.
	int newMapSize = DEFAULT_MAP_SIZE;
	//36 (3.6 m): fits on a 1080p screen at 20 px cells, so the 20 x 20 sprites are drawn 1:1 (pixel-perfect)
	static constexpr int DEFAULT_MAP_SIZE = 36;
	static constexpr int MIN_MAP_SIZE = 5, MAX_MAP_SIZE = 180; //180 -> 4 px cells, the smallest that still shows the lights

	// --- the sprites (assets/Textures), drawn one per cell at cell size, as on the main branch ---
	// Every cell sprite is the same brown floor plus coloured lights. At load time each is split in two:
	//  groundTexture: the floor's brown as brightness only (grey), so a vertex colour can recolour it per cell
	//  light textures: only the pixels where that sprite differs from floorTile.png (its lights), the rest transparent
	sf::Texture wallTexture;          //stone (wall.png, 10 x 10): lethal cells that stand above the ground around them
	sf::Texture pitTexture;           //a dark hollow (pit.png, 10 x 10): lethal cells that lie below the ground around them
	sf::Texture groundTexture;        //from floorTile.png
	sf::Texture startLightTexture;    //startTile.png: green lights
	sf::Texture endLightTexture;      //endTile.png: red lights
	sf::Texture pathLightTexture;     //light_blue.png: the path
	sf::Texture searchedLightTexture; //light_purple.png: cells the algorithm searched
	// --- lethal cells: wall or pit ---
	// A lethal cell is lethal because of its slope / step / roughness (or the inflation around such cells), which a
	// raised block and a dug hole both cause, so the cost alone can't tell them apart. Each connected area of lethal
	// cells is a pit when its mean elevation is below the mean elevation of the walkable cells bordering it, and a wall
	// otherwise (also when the map has no elevation, like the dungeon). Deciding per area means the inflated ring around a
	// hole, which is at ground level, is drawn as part of the pit.
	std::vector<bool> pitCells; //true for the lethal cells drawn as a pit
	/**
	 * Decide for every lethal cell whether it is drawn as a wall or a pit, from the elevation of its connected lethal area
	 * against the walkable cells bordering that area (8-connected areas, 4-connected border).
	 *
	 * @return  Nothing. Fills pitCells. Called when a map loads and after updateCostMap().
	 */
	void updatePitCells();
	//what colour the brown of a cell is drawn in (Terrain view: brown -> darker -> grey; other views: their heat colour)
	sf::Color getGroundColor(int index);

private:
	void updateGeometry(); //tile size from the board size and the map size
	void loadSprites();
	float elevationLowest = 0.f, elevationHighest = 0.f; //the Terrain view's colour range (set in drawBoard)
};
