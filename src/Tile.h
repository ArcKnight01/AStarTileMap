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
#include "Waypoints.h"
using namespace sf;
using namespace std;

// costmap branch: the Board is the SFML view and editor of a core GridMap (core/ has no SFML).
// It loads and saves maps/<name>.gridmap, draws one layer, lets you place the start and end, and asks the core to plan
// on the "cost" layer. (The tile-based Board, NavGraph and planners of the main branch are replaced by core/.)

// What the map shows. V cycles through these on the PLAYING screen.
// The data views follow the stages of the elevation-mapping pipeline (ETH: Fankhauser's elevation_mapping and grid_map,
// Wermelinger's traversability_estimation; then Rajput's cost map):
//   1. elevation map (sensors -> 2.5D height grid; here painted with the brush)    -> Elevation (and Terrain)
//   2. terrain features from the elevation, over small windows                     -> Slope, Roughness, Step
//   3. traversability: a weighted average of the feature scores                    -> Traversability
//   4. cost: lethal below the traversability threshold, else 1 - t; then inflation -> Cost (what the planner reads)
// Every view colours each cell by its number only, as a retint of the floor sprite's brown (Terrain: browns; the data
// views: the chosen colour map), with no wall / pit sprites; the legend beside the panel shows the scale. Unknown is black.
enum class MapView {
	Terrain,        //elevation on the floor sprite: dark brown (low) -> brown (0 m) -> grey (high)
	                //(was: the cost layer drawn as brown -> darker -> grey, with lethal cells as wall / pit sprites)
	Elevation,      //height, symmetric around 0 m: -max|z| .. +max|z|
	Cost,           //the cost layer the planner uses: 0 .. 0.5 (worst walkable); lethal cells (incl. inflation) a darker red
	Traversability, //1 (ideal) .. 0 (not traversable)
	Slope,          //0 .. criticalSlope
	Roughness,      //0 .. criticalRoughness
	Step            //0 .. criticalStep
};
/**
 * Get the name of every map view, in MapView order (as shown in the panel).
 *
 * @return  One name per MapView value.
 */
vector<string> getMapViewNames();

// The colour scale of the data views (the panel's "Colours" dropdown). Position 0 is the good / low end, 1 the bad / high end.
enum class ColorMap {
	RedYellowGreen, //green -> yellow -> red (ColorBrewer's RdYlGn, reversed; "traffic light")
	Viridis,        //dark purple -> blue -> green -> yellow (perceptually uniform, readable with colour blindness)
	Grayscale       //black -> white
};
/**
 * Get the name of every colour map, in ColorMap order (as shown in the dropdown).
 *
 * @return  One name per ColorMap value.
 */
vector<string> getColorMapNames();
/**
 * Get the colour at a position on a colour map.
 *
 * @param colorMap  The colour map.
 * @param amount    Position on the scale, 0 .. 1 (clamped).
 * @return          The colour.
 */
sf::Color getColorMapColor(ColorMap colorMap, double amount);
/**
 * Get the colour for lethal cells in the Cost view, set apart from the colour map's own colours.
 *
 * @param colorMap  The colour map.
 * @return          The lethal colour.
 */
sf::Color getLethalColor(ColorMap colorMap);

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
	//true after an edit (terrain painted, start / end placed, moved or cleared) until the map is saved or loaded again;
	//leaving the map then asks whether to save, and the panel shows "*" after the map's name
	bool hasUnsavedChanges = false;

	int leftMousePos = -1; //cell under the mouse (-1 = outside the board)

	bool startExists = false;
	bool endExists = false;
	int startPos = -1;
	int endPos = -1;

	MapView view = MapView::Terrain;
	ColorMap colorMap = ColorMap::RedYellowGreen; //the data views' colour scale

	// --- path display ---
	// pathTiles: the path (start -> end), drawn blue. searchedTiles: every cell the algorithm evaluated, drawn purple.
	std::vector<int> pathTiles;
	std::vector<int> searchedTiles;
	bool showSearched = true; //false: searchedTiles are still found and counted, but not lit purple (the panel's toggle / S)
	// --- waypoints (core/Waypoints.h), recomputed with the path ---
	// prunedTiles: the path pruned to straight stretches (thesis Algorithm 3); waypoints: every corner plus equal pieces of at
	// most waypointParameters.spacing. With showWaypoints the path is drawn as its waypoints (yellow lights on the cells
	// they fall in) instead of every cell (blue); with showSearched off too, only the waypoints and the start / end show.
	WaypointParameters waypointParameters;
	std::vector<int> prunedTiles;
	std::vector<Waypoint> waypoints;
	bool showWaypoints = false; //the panel's Waypoints toggle / W
	double pathCost = 0;   //total step cost of the path (Rajput eq. 3.8)
	bool navDirty = true;  //set when start / end / the map change; main() then re-plans

	/**
	 * Create a board with a flat map of newMapSize cells per side; loads the sprites and builds the brush kernels.
	 *
	 * @param height  Height of the board, in pixels.
	 * @param width   Width of the board, in pixels.
	 * @param origin  Top-left corner of the board, in window coordinates (pixels).
	 */
	Board(float height, float width, sf::Vector2f origin);

	/**
	 * Loads maps/<name>.gridmap, or starts a new flat map if it doesn't exist. Name is cleaned first (cleanMapName).
	 *
	 * @param name  Map name as typed; replaced by the cleaned name.
	 */
	void importBoard(string& name);
	/**
	 * Save the map, with the start and end, to maps/<name>.gridmap.
	 *
	 * @param name  Map name (already cleaned), without the extension.
	 */
	void exportBoard(string name);
	/**
	 * Names of the maps saved in maps/ (".gridmap" files, without the extension), sorted.
	 *
	 * @return  The sorted names; empty when maps/ is missing.
	 */
	static vector<string> getSavedMapNames();
	/**
	 * "rugged", "rugged.gridmap", "maps/rugged" and " rugged " all become "rugged"; "" becomes "flat".
	 *
	 * @param name  Map name as typed.
	 * @return      The bare map name.
	 */
	static string cleanMapName(string name);
	/**
	 * Check whether a map is one of the example maps that tools/generate_maps makes and the tests read.
	 *
	 * @param name  Map name (already cleaned).
	 * @return      True for flat, rugged, dungeon and hybrid; SAVE then saves a copy instead of overwriting them.
	 */
	static bool isExampleMap(const string& name);

	/**
	 * Plans on the cost layer with the chosen algorithm and stores the path / searched cells for drawing.
	 *
	 * @param algorithm  Planner to use; without both a start and an end, the path is cleared instead.
	 */
	void replan(PathAlgorithm algorithm);

	/**
	 * Where the board is drawn in the window, and how big (pixels); the cell size follows from the map size.
	 *
	 * @param width      Width of the board, in pixels.
	 * @param height     Height of the board, in pixels.
	 * @param newOrigin  Top-left corner of the board, in window coordinates (pixels).
	 */
	void setScreenArea(float width, float height, sf::Vector2f newOrigin);

	/**
	 * Draw the board in the current view, with its lights, the placement preview and the highlights.
	 *
	 * @param window  Window to draw into.
	 */
	void drawBoard(sf::RenderTarget& window); //draws the chosen view, the search, the path, start / end, highlights (was sf::RenderWindow&; any target, so it can be drawn off-screen)
	/**
	 * Switch to the next map view, wrapping around after the last.
	 */
	void cycleView();

	/**
	 * Gets the position of the cell from index number (N), in the window.
	 *
	 * @param N  Cell index (row * columns + column).
	 * @return   Top-left corner of the cell, in window coordinates (pixels).
	 */
	sf::Vector2f getTilePos(int N);
	/**
	 * Gets the index of the cell under a window position (-1 if the position is outside the board).
	 *
	 * @param x  Horizontal position, in window coordinates (pixels).
	 * @param y  Vertical position, in window coordinates (pixels).
	 * @return   Cell index (row * columns + column), or -1 outside the board.
	 */
	int getTileN(float x, float y);

	// --- placing the start and end (TilePlacement.cpp) ---
	// Click a cell to select it, pick from the list with Up / Down (the cell previews it), click it again (or Enter) to
	// place, right-click / Backspace to unselect. Start and End are offered on every walkable cell (they move there if
	// they already exist), never on a lethal cell; Clear is offered on the cell that is the start or end.
	bool isPlacing = false;
	int placingTile = -1;
	std::vector<PlacementOption> placementOptions;
	int placementChoice = 0;
	/**
	 * Select a cell and list what can be placed on it; cancels instead when nothing can (lethal or outside).
	 *
	 * @param tile  Cell index (row * columns + column).
	 */
	void beginPlacement(int tile);
	/**
	 * Move the highlighted option in the list, wrapping around in both directions.
	 *
	 * @param step  Number of options to move (+1 down, -1 up).
	 */
	void movePlacementChoice(int step);
	/**
	 * Place, move or clear the start / end as the highlighted option says, set navDirty and end the selection.
	 */
	void confirmPlacement();
	/**
	 * Unselect the cell and clear the option list.
	 */
	void cancelPlacement();
	/**
	 * Get the plain name of a placement option.
	 *
	 * @param option  The option.
	 * @return        "Start", "End" or "Clear".
	 */
	static string getPlacementName(PlacementOption option);
	/**
	 * Get the label of a placement option as shown in the list.
	 *
	 * @param option  The option.
	 * @return        "Move start" / "Move end" when it already exists, otherwise its plain name.
	 */
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
	// Add a tapered Gaussian (0 at the brush box's border) of a peak height to the elevation of the cells under the brush:
	// z <- z + elevation * w, or with elevation 0 flatten them toward 0 m: z <- z * ( 1 - w ) (TerrainBrush.h).
	// w is the brush kernel's weight for that cell (1 at the centre). Cells outside the map are skipped. The brushed cells
	// are added to terrainEditedTiles.
	// (was: z <- z + strength * w * ( elevation - z ), moving each cell part of the way toward the height)
	/**
	 * Add a tapered Gaussian mound / hollow under the brush (or flatten it), adding the cells to terrainEditedTiles.
	 *
	 * @param tile        Index of the cell under the mouse (row * columns + column).
	 * @param elevation   Peak height added at the centre, in metres (negative digs; 0 flattens the box toward 0 m).
	 * @param brushWidth  Side of the square brush, in cells (one of getBrushWidths()).
	 */
	void setTerrainHeight(int tile, float elevation, int brushWidth);
	// The whole pipeline is run on a copy of the map, and only the cells an edit can reach are copied back, so on a map
	// built by the pipeline the result equals a full re-run, while a map whose cost layer was written directly (dungeon)
	// keeps its cost everywhere else.
	// Clears terrainEditedTiles and sets navDirty so the path is planned again.
	/**
	 * Rebuild the derived layers (slope, roughness, step, traversability, cost) around the cells in terrainEditedTiles.
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
	sf::Texture waypointLightTexture; //light_yellow.png: the cells the waypoints fall in
	// --- lethal cells: wall or pit ---
	// A lethal cell is lethal because of its slope / step / roughness (or the inflation around such cells), which a
	// raised block and a dug hole both cause, so the cost alone can't tell them apart. Each connected area of lethal
	// cells is a pit when its mean elevation is below the mean elevation of the walkable cells bordering it, and a wall
	// otherwise (also when the map has no elevation, like the dungeon). Deciding per area means the inflated ring around a
	// hole, which is at ground level, is drawn as part of the pit.
	std::vector<bool> pitCells; //true for the lethal cells drawn as a pit
	// Decide for every lethal cell whether it is drawn as a wall or a pit, from the elevation of its connected lethal area
	// against the walkable cells bordering that area (8-connected areas, 4-connected border).
	// Called when a map loads and after updateCostMap().
	/**
	 * Decide for every lethal cell whether it is drawn as a wall or a pit, filling pitCells.
	 */
	void updatePitCells();
	/**
	 * What colour a cell's floor sprite is retinted to (Terrain view: browns by elevation; data views: the colour map).
	 *
	 * @param index  Cell index (row * columns + column).
	 * @return       The colour for the current view; black when the value is unknown.
	 */
	sf::Color getGroundColor(int index);
	/**
	 * What colour a value of the current view's layer is drawn in (used by the cells and by the legend).
	 *
	 * @param value  Value of the view's layer (metres, radians, 0 .. 1, or cost); NaN = unknown.
	 * @return       The colour; black when the value is unknown.
	 */
	sf::Color getValueColor(float value);
	/**
	 * Get the name of the layer the current view shows ("elevation" for Terrain and Elevation).
	 *
	 * @return  The layer name.
	 */
	std::string getViewLayer() const;
	/**
	 * Get the range of values the current view's colour scale covers (values outside it get the end colours).
	 *
	 * @param lowest   Set to the value at the bottom of the scale.
	 * @param highest  Set to the value at the top of the scale.
	 */
	void getViewRange(float& lowest, float& highest) const;
	// The legend: a vertical colour bar of the current view's scale (top = highest value), with five labelled ticks
	// and units, the view's name written up its left side, a "lethal" swatch above it in the Cost view, and an
	// "unknown" (black) swatch below it.
	/**
	 * Draw the legend of the current view in a column.
	 *
	 * @param window  Target to draw into.
	 * @param font    Font for the labels.
	 * @param area    The legend's column, in window coordinates (pixels): the bar runs down its left side.
	 */
	void drawLegend(sf::RenderTarget& window, const sf::Font& font, sf::FloatRect area);

private:
	/**
	 * Recompute the cell size (tileW, tileH) in pixels from the board size and the map size.
	 */
	void updateGeometry(); //tile size from the board size and the map size
	/**
	 * Load the wall and pit textures and split the cell sprites into the grey ground texture and the light textures.
	 */
	void loadSprites();
	//float elevationLowest = 0.f, elevationHighest = 0.f; (was the lowest / highest height, the Elevation view's range)
	float elevationExtent = 0.01f; //m: the Terrain and Elevation views run from -elevationExtent to +elevationExtent
	                               //(the largest |height| on the map, at least 1 cm; set by updateElevationExtent())
	/**
	 * Set elevationExtent from the map's elevation layer.
	 */
	void updateElevationExtent();
};
