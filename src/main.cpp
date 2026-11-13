// iD Tech Game Plan - SFML Template
// Include Needed for SFML
#include <SFML/Graphics.hpp>
// Include Needed for Animations
#include <SFML/Graphics/Rect.hpp>
// Include Needed for Text
#include <SFML/Graphics/Text.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include "Tile.h"
//CHARACTER (relic, not used on the costmap branch): #include "character.h"
#include "Button.h"
#include "TextBox.h"
#include "Dropdown.h"
#include "Toggle.h"
#include "Dialog.h"
#include "ExportPanel.h"
#include "ControlsList.h"
#include "GridMapFile.h"
#include "RosExport.h"
#include <filesystem>
#include <fstream>

using namespace sf;
/* DEFINE GAMESTATES HERE */
// Which screen is showing:
//   MENU    --START (click it, or press Enter)----------> PLAYING
//   PLAYING --MENU (click it, or press Esc)-------------> MENU  (asks "Save changes?" first if the map has unsaved edits)
//   PLAYING --window X----------------------------------> closes the program (asks "Save changes?" the same way)
//   MENU    --EXIT (click it, or press Esc)-------------> closes the program
// SAVE (or Ctrl+S) saves without leaving; on an example map it asks for a name and saves a copy instead.
// (was: PLAYING --SAVE & MENU (click it, or press E)--> MENU, always saving; PLAYING --Esc--> MENU, never saving and
//  without asking; the window X closed without saving)
// TODO: START both loads and creates (type a saved map's name, or a new one), which relies on remembering map names.
// Later this could become a Load dropdown of the saved maps (Board::getSavedMapNames()) and a separate New map button.
enum GameState {
	MENU,
	PLAYING
};

//constants here:
const float windowW = 1100; //the MENU window
const float windowH = 800;
const float boardMargin = 40;  //empty space between the window edge and the map
const float panelWidth = 300;  //the side panel right of the map (was 260; widened so the Searched and Waypoints toggles fit side by side)
//the colour-scale legend: a column between the map and the panel's text, controls and buttons (Board::drawLegend)
const float legendColumnWidth = 104;
//the PLAYING window is at least this tall, so the side panel fits: its text ends about 740 px down in Set terrain mode,
//and the SAVE / EXPORT / MENU stack (3 x 44 px + 2 x 12 px gaps) sits 40 px above the bottom with about 20 px to spare
//(was windowH = 800 before the searched-cells toggle, then 880 with the buttons side by side)
//(then 960; now 1040: the Colours dropdown added three lines, so the text ends about 815 px down)
const float playMinHeight = 1040;
//the SAVE / EXPORT / MENU buttons, stacked at the bottom of the panel: each this tall, with at least this much space between
const float panelButtonHeight = 44;
const float panelButtonGap = 12;
//PLAYING: the window resizes to the map. Each cell gets the biggest of these sizes that fits on the screen, so the
//20 x 20 sprites are always drawn at a clean scale (x2, x1, x1/2, x1/4); only a map too big even for 5 px cells is
//squeezed to fit.
const float cellSizes[] = { 40, 20, 10, 5 };
//height of one line of the side panel text (19 pt Candal), used to place the panel dropdowns in the gaps left in the text
const float panelLineHeight = 25;

/**
 * Places a text so that its centre is at (x, y).
 *
 * @param text  Text to position; its origin is moved to the centre of its local bounds.
 * @param x     Horizontal centre, in window coordinates (pixels).
 * @param y     Vertical centre, in window coordinates (pixels).
 */
void centerText(Text& text, float x, float y) {
	FloatRect bounds = text.getLocalBounds();
	text.setOrigin(bounds.left + bounds.width / 2, bounds.top + bounds.height / 2);
	text.setPosition(x, y);
}

//main code (an optional map name on the command line pre-fills the menu's text box: ./AStarTileMap dungeon)
/**
 * Main code: runs the menu and the costmap editor / planner until the window closes.
 *
 * @param argc  Number of command-line arguments.
 * @param argv  Command-line arguments; argv[1], if given, is a map name that pre-fills the menu's text box.
 * @return      Exit code: 0 on success.
 */
int main(int argc, char* argv[])
{
	// Create a Window to Display Graphics, Define Size and Window Name
	RenderWindow window(VideoMode((unsigned int)windowW, (unsigned int)windowH), "Aidan's TileMap");

	/* CREATE OBJECTS AND ASSIGN OBJECT ATTRIBUTES HERE */
	Font font;
	font.loadFromFile("Font/Candal.ttf");

	// --- MENU screen ---
	Text title("A* TileMap", font, 64);
	centerText(title, windowW / 2, 90);

	Text mapLabel("Map name", font, 24);
	mapLabel.setPosition(windowW / 2 - 230, 175);
	TextBox mapNameBox;
	mapNameBox.setup(font, Vector2f(windowW / 2, 240), Vector2f(460, 56), "flat");
	if (argc > 1) {
		mapNameBox.setText(argv[1]);
	}
	Text savedMapsText("", font, 18); //filled by refreshSavedMaps()
	savedMapsText.setFillColor(Color(170, 170, 170));

	Text algorithmLabel("Algorithm", font, 24);
	algorithmLabel.setPosition(windowW / 2 - 230, 350);
	Dropdown algorithmDropdown;
	algorithmDropdown.setup(font, Vector2f(windowW / 2, 415), Vector2f(460, 56), getPathAlgorithmNames());

	//size of a new map, in cells per side (a saved map keeps its own size)
	Text sizeLabel("Size of a new map (cells per side)", font, 24);
	sizeLabel.setPosition(windowW / 2 - 230, 475);
	TextBox mapSizeBox;
	mapSizeBox.setup(font, Vector2f(windowW / 2, 540), Vector2f(460, 56), std::to_string(Board::DEFAULT_MAP_SIZE), 3);
	Text sizeHint("", font, 16);
	sizeHint.setFillColor(Color(170, 170, 170));
	sizeHint.setString(std::to_string(Board::MIN_MAP_SIZE) + " to " + std::to_string(Board::MAX_MAP_SIZE) + " at 0.1 m per cell. "
		+ std::to_string(Board::DEFAULT_MAP_SIZE) + " is the default.\nThe window resizes to the map: 40, 20, 10 or 5 px cells.");
	sizeHint.setPosition(windowW / 2 - 230, 578);
	//the number in the size box, or the default if it is empty / not a number; kept within MIN_MAP_SIZE .. MAX_MAP_SIZE
	auto getNewMapSize = [&]() {
		int size = Board::DEFAULT_MAP_SIZE;
		try {
			if (!mapSizeBox.getText().empty()) size = std::stoi(mapSizeBox.getText());
		}
		catch (const std::exception&) {
			size = Board::DEFAULT_MAP_SIZE;
		}
		return std::max(Board::MIN_MAP_SIZE, std::min(Board::MAX_MAP_SIZE, size));
	};

	//START and EXIT side by side at the bottom of the menu
	//(START was alone in the middle: startButton.setup(font, "START", Vector2f(windowW / 2, 705), Vector2f(300, 70), 40);)
	Button startButton;
	startButton.setup(font, "START", Vector2f(windowW / 2 - 160, 705), Vector2f(300, 70), 40);
	//closes the program (same as Esc on the menu)
	Button exitButton;
	exitButton.setup(font, "EXIT", Vector2f(windowW / 2 + 160, 705), Vector2f(300, 70), 40);

	// --- PLAYING screen: side panel to the right of the map ---
	//Button saveMenuButton; (was one SAVE & MENU button that always saved)
	//SAVE, EXPORT and MENU in a row at the bottom of the panel (positioned by layoutForMap() when a map starts)
	Button saveButton;
	Button exportButton;
	Button menuButton;
	//the "Save changes?" / "Save a copy as" dialog, and the EXPORT panel; while either is open it takes all input
	Dialog dialog;
	dialog.setup(font);
	ExportPanel exportPanel;
	//the EXPORT rows: what is exported, as which ROS 2 message, on which topic (ROS2_PLAN.md), and into which file
	//(exports/<map>_<suffix>.yaml). The first three are whole maps (up to ~300 KB), so Copy puts a ros2 topic pub
	//command for the saved file on the clipboard; the path and the poses are small, so Copy puts the YAML itself there.
	struct ExportItem {
		std::string name;
		std::string type;  //ROS 2 message type (or what the plain text is)
		std::string topic; //topic name ("" for plain text that is not a message)
		std::string file;  //file name after "<map>_", with its extension
		bool large;        //true: Copy gives a command for the file instead of the YAML
	};
	const std::vector<ExportItem> exportItems = {
		{ "Elevation map",    "grid_map_msgs/msg/GridMap",     "elevation_map", "elevation_map.yaml",  true },
		{ "Cost map",         "grid_map_msgs/msg/GridMap",     "cost_map",      "cost_map.yaml",       true },
		{ "Cost map (Nav2)",  "nav_msgs/msg/OccupancyGrid",    "cost_map_grid", "cost_map_grid.yaml",  true },
		{ "Path",             "nav_msgs/msg/Path",             "planned_path",  "planned_path.yaml",   false },
		{ "Waypoints",        "nav_msgs/msg/Path",             "waypoints",     "waypoints.yaml",      false }, //stamp = arrival time
		{ "Waypoints (list)", "[x,y] [x,y] ... (husky-stable_ws arguments)", "", "waypoints_list.txt", false },
		{ "Start pose",       "geometry_msgs/msg/PoseStamped", "start_pose",    "start_pose.yaml",     false },
		{ "Goal pose",        "geometry_msgs/msg/PoseStamped", "goal_pose",     "goal_pose.yaml",      false }
	};
	{
		std::vector<std::string> names, details;
		for (const ExportItem& item : exportItems) {
			names.push_back(item.name);
			details.push_back(item.topic.empty() ? item.type : item.type + " -> /" + item.topic);
		}
		exportPanel.setup(font, names, details);
	}
	//a short message over the bottom of the map after saving ("Saved maps/rugged.gridmap"), shown for toastSeconds
	std::string toastText;
	Clock toastClock;
	const float toastSeconds = 2.5f;
	Text panelText("", font, 19);
	//the Controls list at the bottom of the panel text: rows of key / mouse sprites and words (ControlsList.h)
	ControlsList controlsList;
	controlsList.setup(font, 17, panelLineHeight);
	//whether the cells the algorithm searched light up purple (same as pressing S); it sits on its own line under the
	//dropdowns (placed by layoutPanel()), with a purple track to match the searched lights. Off: only the count is shown.
	//(was "Searched (S)", 240 x 24, alone on its line; the S and W keys are now in the Controls rows)
	Toggle searchedToggle;
	searchedToggle.setup(font, "Searched", Vector2f(0, 0), Vector2f(136, 24), true, Color(150, 80, 200), 15);
	//beside it: the path drawn as its waypoints (yellow) instead of every cell (blue), same as pressing W
	Toggle waypointsToggle;
	waypointsToggle.setup(font, "Waypoints", Vector2f(0, 0), Vector2f(136, 24), false, Color(220, 190, 50), 15);
	//Each panel dropdown sits in a gap of two empty lines left under its label in the panel text (placed by layoutPanel()).
	//(was 240 x 40 with three empty lines per gap; 36 px tall fits two lines, so the panel has room for five dropdowns)
	const Vector2f panelDropdownSize(280, 36); //(was 240 wide, in the 260 px panel)
	//the path algorithm, changeable while playing (the menu's dropdown picks the first one). Changing it re-plans at once.
	Dropdown panelAlgorithmDropdown;
	panelAlgorithmDropdown.setup(font, Vector2f(0, 0), panelDropdownSize, getPathAlgorithmNames());
	//the view (same as pressing V)
	Dropdown viewDropdown;
	viewDropdown.setup(font, Vector2f(0, 0), panelDropdownSize, getMapViewNames());
	//the data views' colour scale: red-yellow-green, viridis or grayscale (Tile.h ColorMap)
	Dropdown colorMapDropdown;
	colorMapDropdown.setup(font, Vector2f(0, 0), panelDropdownSize, getColorMapNames());
	//what a left click on the map does: place the start / end, or set terrain (TerrainBrush.h)
	Dropdown clickModeDropdown;
	clickModeDropdown.setup(font, Vector2f(0, 0), panelDropdownSize, getClickModeNames());
	//only shown with "Set terrain": the height the brush adds (0 flattens), and the brush size
	Dropdown terrainHeightDropdown;
	terrainHeightDropdown.setup(font, Vector2f(0, 0), panelDropdownSize, getTerrainHeightNames(), 4); //starts on Mound +0.05 m (was Ledge +0.15 m)
	Dropdown brushDropdown;
	brushDropdown.setup(font, Vector2f(0, 0), panelDropdownSize, getBrushNames(), 1);                //starts on 3 x 3 cells
	ClickMode clickMode = ClickMode::PlaceStartEnd;
	bool isPaintingTerrain = false; //the left button is held down in Set terrain mode: moving the mouse keeps painting
	int lastPaintedTile = -1;       //so holding the mouse still doesn't paint the same cell every event
	float panelLeft = 0;            //left edge of the side panel's text, controls and buttons (set by layoutForMap())
	float panelBottom = 0;          //bottom of the side panel (the window height less the margin; set by layoutForMap())
	//the height the brush paints (m): picking a preset in the Height dropdown sets it, Up / Down change it in 0.01 m
	//steps (Shift: 0.05 m), so heights between the presets can be painted too
	float brushElevation = getTerrainHeights()[terrainHeightDropdown.getSelectedIndex()].elevation;
	//(was: int brushStrength, how far one click moved the cells toward the height, 10 % .. 100 % on Left / Right)

	//the brush height as shown in the panel, e.g. "+0.07 m"
	auto getBrushElevationText = [&]() {
		std::ostringstream out;
		out << std::showpos << std::fixed << std::setprecision(2) << brushElevation << " m";
		return out.str();
	};

	//the panel dropdowns that are showing, top to bottom, each with the label written above it in the panel text
	auto getPanelDropdowns = [&]() {
		std::vector<std::pair<std::string, Dropdown*>> dropdowns = {
			{ "Algorithm:", &panelAlgorithmDropdown },
			{ "View (V):", &viewDropdown },
			{ "Colours:", &colorMapDropdown },
			{ "Left click:", &clickModeDropdown }
		};
		if (clickMode == ClickMode::SetTerrain) {
			dropdowns.push_back({ "Height: " + getBrushElevationText() + " (Up/Down)", &terrainHeightDropdown });
			//dropdowns.push_back({ "Brush: " + std::to_string(brushStrength) + "% (Left/Right)", &brushDropdown }); (was)
			dropdowns.push_back({ "Brush:", &brushDropdown });
		}
		return dropdowns;
	};
	//the first panel dropdown's label is on this line of the panel text (after "Map:" and "Size:"), and each dropdown
	//takes three lines: its label and the two-line gap the dropdown is drawn in
	const int firstPanelDropdownLine = 2, panelDropdownLines = 3;
	//places each showing dropdown in the gap under its label; called when the map starts and when the click mode changes
	auto layoutPanel = [&]() {
		std::vector<std::pair<std::string, Dropdown*>> dropdowns = getPanelDropdowns();
		for (int i = 0; i < (int)dropdowns.size(); i++) {
			float labelTop = boardMargin + (firstPanelDropdownLine + i * panelDropdownLines) * panelLineHeight;
			dropdowns[i].second->setCenter(Vector2f(panelLeft + panelDropdownSize.x / 2, labelTop + panelLineHeight + panelDropdownSize.y / 2 + 2));
		}
		//the searched-cells toggle: on the first line after the dropdowns, centred in that line
		float toggleLineTop = boardMargin + (firstPanelDropdownLine + (int)dropdowns.size() * panelDropdownLines) * panelLineHeight;
		//the two toggles side by side, 136 px each with 8 px between them, filling the dropdowns' 280 px
		searchedToggle.setCenter(Vector2f(panelLeft + 68, toggleLineTop + panelLineHeight / 2));
		waypointsToggle.setCenter(Vector2f(panelLeft + 212, toggleLineTop + panelLineHeight / 2));
	};

	//Window size. Each screen is laid out in a fixed area (layoutW x layoutH drawing units). The program sizes the window
	//to that area itself until the user resizes or maximizes it; after that it keeps the user's size and fits the area into
	//the window (same aspect, centred, bars on the sides). It must not call setSize() then: a maximized window refuses it,
	//but SFML still stores the refused size as the window's size (and no Resized event comes to correct it), so it would
	//draw into one corner and map the mouse wrongly.
	float layoutW = windowW, layoutH = windowH;
	Vector2u windowSize = window.getSize(); //the real size, kept up to date by Resized events
	bool userSizedWindow = false;
	//fits the layout area into the window, keeping its aspect ratio (1 drawing unit = 1 pixel when the sizes match)
	auto applyView = [&]() {
		View view(FloatRect(0, 0, layoutW, layoutH));
		float scale = std::min(windowSize.x / layoutW, windowSize.y / layoutH);
		float viewportW = layoutW * scale / windowSize.x, viewportH = layoutH * scale / windowSize.y;
		view.setViewport(FloatRect((1 - viewportW) / 2, (1 - viewportH) / 2, viewportW, viewportH));
		window.setView(view);
	};
	//switches to a layout area of width x height: resizes the window to it (keeping it on the screen), unless the user has sized the window
	auto resizeWindow = [&](float width, float height) {
		layoutW = width;
		layoutH = height;
		if (!userSizedWindow) {
			windowSize = Vector2u((unsigned int)width, (unsigned int)height);
			window.setSize(windowSize);
			VideoMode desktop = VideoMode::getDesktopMode();
			window.setPosition(Vector2i(std::max(0, ((int)desktop.width - (int)width) / 2), std::max(0, ((int)desktop.height - (int)height) / 2)));
		}
		applyView();
	};

	//PLAYING layout for the loaded map: cell size, window size, the board's place, and the side panel
	auto layoutForMap = [&](Board& board) {
		VideoMode desktop = VideoMode::getDesktopMode();
		//room for the board on this screen (leaving space for the title bar, a taskbar and the side panel)
		float roomW = std::max(200.f, (float)desktop.width - 2 * boardMargin - legendColumnWidth - panelWidth - boardMargin - 40);
		float roomH = std::max(200.f, (float)desktop.height - 2 * boardMargin - 120);
		int columns = board.map.getColumns(), rows = board.map.getRows();
		float cell = 0;
		for (float size : cellSizes) {
			if (columns * size <= roomW && rows * size <= roomH) { cell = size; break; }
		}
		if (cell == 0) cell = std::min(roomW / columns, roomH / rows); //too big even for 5 px cells: squeeze it
		float boardPixelsW = columns * cell, boardPixelsH = rows * cell;
		//the panel needs about playMinHeight of height (text, dropdowns, toggle, SAVE & MENU), so small maps get a taller window
		float playW = boardMargin + boardPixelsW + boardMargin + legendColumnWidth + panelWidth + boardMargin;
		//float playH = std::max(windowH, boardPixelsH + 2 * boardMargin); (was at least the menu's height)
		float playH = std::max(playMinHeight, boardPixelsH + 2 * boardMargin);
		board.setScreenArea(boardPixelsW, boardPixelsH, Vector2f(boardMargin, (playH - boardPixelsH) / 2));
		resizeWindow(playW, playH);
		//panelLeft = 2 * boardMargin + boardPixelsW; (was; the legend column now comes first)
		panelLeft = 2 * boardMargin + boardPixelsW + legendColumnWidth;
		panelBottom = playH - boardMargin;
		panelText.setPosition(panelLeft, boardMargin);
		//(the view dropdown was set up here at boardMargin + 4 * 25 + 25, under "View (V):" on line 3, before the
		//algorithm and terrain dropdowns were added; layoutPanel() now places them all)
		//viewDropdown.setup(font, Vector2f(panelLeft + 120, boardMargin + 4 * 25 + 25), Vector2f(240, 40), getMapViewNames(), viewDropdown.getSelectedIndex());
		layoutPanel();
		//saveMenuButton.setup(font, "SAVE & MENU", Vector2f(panelLeft + panelWidth / 2, playH - boardMargin - 40), Vector2f(240, 70), 26);
		//SAVE, EXPORT, MENU stacked top to bottom, the panel's full 240 px wide, panelButtonGap apart, MENU at the bottom
		//(was a row of three 76 x 50 buttons side by side)
		float buttonStep = panelButtonHeight + panelButtonGap;
		float menuCenterY = playH - boardMargin - panelButtonHeight / 2;
		Vector2f buttonSize(280, panelButtonHeight); //(was 240 wide)
		saveButton.setup(font, "SAVE", Vector2f(panelLeft + 140, menuCenterY - 2 * buttonStep), buttonSize, 20);
		exportButton.setup(font, "EXPORT", Vector2f(panelLeft + 140, menuCenterY - buttonStep), buttonSize, 20);
		menuButton.setup(font, "MENU", Vector2f(panelLeft + 140, menuCenterY), buttonSize, 20);
	};

	// The board is created when START is pressed (a fresh board for every map)
	std::unique_ptr<Board> GameBoard;
	//CHARACTER (relic, not used on the costmap branch):
	//std::unique_ptr<Character> player;

	/* DEFINE VARIABLES HERE */
	GameState gameState = MENU; // the game starts on the menu
	PathAlgorithm algorithm = PathAlgorithm::AStar;

	//lists the saved maps under the text box (later this could become a dropdown that fills the text box)
	auto refreshSavedMaps = [&]() {
		std::string list = "Saved maps:";
		std::vector<std::string> savedMaps = Board::getSavedMapNames();
		for (std::size_t i = 0; i < savedMaps.size(); i++) {
			list += (i == 0 ? " " : ", ") + savedMaps[i];
		}
		list += "\nEmpty loads \"flat\"; a new name starts a new flat map.";
		savedMapsText.setString(list);
		savedMapsText.setPosition(windowW / 2 - 230, 278);
	};
	refreshSavedMaps();

	//MENU -> PLAYING: load (or create) the map named in the text box
	auto startGame = [&]() {
		algorithm = (PathAlgorithm)algorithmDropdown.getSelectedIndex();
		panelAlgorithmDropdown.setSelectedIndex(algorithmDropdown.getSelectedIndex()); //the panel starts on the menu's choice
		//CHARACTER (relic): player.reset(); //the character points at the board, so remove it before replacing the board
		GameBoard = std::make_unique<Board>(windowH - 2 * boardMargin, windowH - 2 * boardMargin, Vector2f(boardMargin, boardMargin));
		GameBoard->boardname = mapNameBox.getText();
		GameBoard->newMapSize = getNewMapSize(); //only used if the map doesn't exist yet
		GameBoard->view = (MapView)viewDropdown.getSelectedIndex(); //keep the view chosen last time
		GameBoard->colorMap = (ColorMap)colorMapDropdown.getSelectedIndex(); //and the colours
		GameBoard->showSearched = searchedToggle.isOn();            //and whether searched cells light up
		GameBoard->showWaypoints = waypointsToggle.isOn();          //and whether the path shows as its waypoints
		GameBoard->importBoard(GameBoard->boardname); //cleans the name: "" -> "flat", "x.gridmap" -> "x"
		layoutForMap(*GameBoard); //the window resizes to the map
		//CHARACTER (relic, not used on the costmap branch):
		//int initialPos;
		//if (GameBoard->startExists) {
		//	initialPos = GameBoard->startPos;
		//}
		//else {
		//	initialPos = GameBoard->firstEmptyTile;
		//}
		//player = std::make_unique<Character>(*GameBoard, initialPos);
		window.setTitle("Aidan's TileMap - " + GameBoard->boardname);
		mapNameBox.setActive(false);
		mapSizeBox.setActive(false);
		gameState = PLAYING;
	};

	//PLAYING -> MENU (saving is done before, by saveMap(); was returnToMenu(bool save), which saved when asked)
	auto returnToMenu = [&]() {
		dialog.close();
		exportPanel.close();
		mapNameBox.setText(GameBoard->boardname); //so START reopens the same map
		algorithmDropdown.setSelectedIndex((int)algorithm); //and with the algorithm last chosen in the panel
		isPaintingTerrain = false;
		refreshSavedMaps();
		window.setTitle("Aidan's TileMap");
		resizeWindow(windowW, windowH); //back to the menu's size
		gameState = MENU;
	};

	//reads the panel dropdowns after one of them (usedDropdown) was used: a new algorithm re-plans, a new view is shown, a
	//new click mode shows or hides the terrain dropdowns, and a Height preset sets the brush height (Brush is read when
	//painting)
	auto applyPanelChoices = [&](Dropdown* usedDropdown) {
		if (usedDropdown == &terrainHeightDropdown) {
			brushElevation = getTerrainHeights()[terrainHeightDropdown.getSelectedIndex()].elevation;
		}


		PathAlgorithm chosenAlgorithm = (PathAlgorithm)panelAlgorithmDropdown.getSelectedIndex();
		if (chosenAlgorithm != algorithm) {
			algorithm = chosenAlgorithm;
			GameBoard->navDirty = true;
		}


		GameBoard->view = (MapView)viewDropdown.getSelectedIndex();
		GameBoard->colorMap = (ColorMap)colorMapDropdown.getSelectedIndex();


		ClickMode chosenClickMode = (ClickMode)clickModeDropdown.getSelectedIndex();
		if (chosenClickMode != clickMode) {
			clickMode = chosenClickMode;
			GameBoard->cancelPlacement(); //a cell selected for Start / End no longer applies
			isPaintingTerrain = false;
			layoutPanel();
		}
	};

	//gives a left click to the panel dropdowns; true when one of them used it (so the map must not react to it).
	//An open list covers what is below it, so it gets the click first; a click outside it closes it and goes on to the
	//closed boxes (top to bottom), so clicking another box while a list is open opens that one.
	auto handlePanelClick = [&](Vector2f mouse) {
		std::vector<std::pair<std::string, Dropdown*>> dropdowns = getPanelDropdowns();
		for (std::pair<std::string, Dropdown*>& dropdown : dropdowns) {
			if (!dropdown.second->isOpen()) continue;
			if (dropdown.second->handleClick(mouse)) {
				applyPanelChoices(dropdown.second);
				return true;
			}
			break;
		}
		for (std::pair<std::string, Dropdown*>& dropdown : dropdowns) {
			if (dropdown.second->handleClick(mouse)) {
				applyPanelChoices(dropdown.second);
				return true;
			}
		}
		return false;
	};

	//Set terrain: adds the brush's tapered Gaussian of the chosen height around the cell under the mouse (0: flattens).
	//Once per click, and once more each time a drag enters another cell (lastPaintedTile).
	auto paintTerrain = [&](int tile) {
		if (tile == -1 || tile == lastPaintedTile) {
			return;
		}
		//float elevation = getTerrainHeights()[terrainHeightDropdown.getSelectedIndex()].elevation; (was the preset only)
		int brushWidth = getBrushWidths()[brushDropdown.getSelectedIndex()];
		GameBoard->setTerrainHeight(tile, brushElevation, brushWidth); //(was: also brushStrength / 100.f)
		lastPaintedTile = tile;
	};

	//Set terrain: Up / Down raise or lower the brush height by one step. The height is rounded to whole centimetres so
	//repeated steps don't drift (0.01 is not exact in binary); when it lands on a preset, the Height dropdown shows it.
	auto changeBrushElevation = [&](float step) {


		// pseudo-code:
		// brushElevation = round( 100 * ( brushElevation + step ) ) / 100


		brushElevation = std::round(100.f * (brushElevation + step)) / 100.f + 0.f; //+ 0.f turns -0.00 into +0.00
		std::vector<TerrainHeight> presets = getTerrainHeights();
		for (int i = 0; i < (int)presets.size(); i++) {
			if (std::fabs(presets[i].elevation - brushElevation) < 0.001f) terrainHeightDropdown.setSelectedIndex(i);
		}
	};

	// --- saving and leaving ---
	// leaveMap( where ): go to the menu / close the window, asking "Save changes?" first if there are unsaved edits.
	// saveMap( then ): save, then go on to where (Stay = keep editing); an example map asks for a copy's name first.
	enum class AfterSave { Stay, Menu, CloseWindow };
	enum class DialogPurpose { SaveChanges, SaveCopy };
	AfterSave pendingAfterSave = AfterSave::Stay;     //where to go once the dialog is answered
	DialogPurpose dialogPurpose = DialogPurpose::SaveChanges;
	std::string overwriteName;                        //a copy's name that already exists, confirmed by a second Save copy

	//shows a short message over the bottom of the map
	auto showToast = [&](const std::string& text) {
		toastText = text;
		toastClock.restart();
	};

	//goes on to where the user was going once the map is saved or not saved
	auto finishLeaving = [&](AfterSave where) {
		if (where == AfterSave::Menu) returnToMenu();
		if (where == AfterSave::CloseWindow) window.close();
	};

	//saves the map (or asks for a copy's name on an example map), then goes on to where
	auto saveMap = [&](AfterSave then) {
		if (Board::isExampleMap(GameBoard->boardname)) {
			pendingAfterSave = then;
			dialogPurpose = DialogPurpose::SaveCopy;
			overwriteName.clear();
			dialog.open("'" + GameBoard->boardname + "' is an example map (tools/generate_maps makes it).\nSave a copy as:",
				{ "Save copy", "Cancel" }, Vector2f(layoutW / 2, layoutH / 2), true, GameBoard->boardname + "_copy");
			return;
		}
		GameBoard->exportBoard(GameBoard->boardname);
		GameBoard->mapIsNew = false;
		showToast("Saved maps/" + GameBoard->boardname + ".gridmap");
		finishLeaving(then);
	};

	//goes to where, asking "Save changes?" first if the map has unsaved edits
	auto leaveMap = [&](AfterSave where) {
		if (!GameBoard->hasUnsavedChanges) {
			finishLeaving(where);
			return;
		}
		pendingAfterSave = where;
		dialogPurpose = DialogPurpose::SaveChanges;
		dialog.open("Save changes to '" + GameBoard->boardname + "'?", { "Save", "Don't save", "Cancel" }, Vector2f(layoutW / 2, layoutH / 2));
	};

	//acts on the dialog button picked (0 = the first)
	auto handleDialogChoice = [&](int choice) {
		if (dialogPurpose == DialogPurpose::SaveChanges) {
			dialog.close();
			if (choice == 0) saveMap(pendingAfterSave);       //Save (may open the Save copy dialog)
			if (choice == 1) finishLeaving(pendingAfterSave); //Don't save
			return;                                           //Cancel: stay
		}
		//Save copy: the typed name, cleaned like a menu name; example names are refused, an existing map needs a second click
		if (choice != 0) {
			dialog.close(); //Cancel: stay, without leaving
			return;
		}
		std::string name = Board::cleanMapName(dialog.getText());
		if (Board::isExampleMap(name)) {
			dialog.setMessage("'" + name + "' is an example map too. Choose another name:");
			return;
		}
		if (std::filesystem::exists("maps/" + name + ".gridmap") && name != overwriteName) {
			overwriteName = name;
			dialog.setMessage("'" + name + "' already exists. Save copy again to overwrite it:");
			return;
		}
		dialog.close();
		GameBoard->boardname = name;
		window.setTitle("Aidan's TileMap - " + name);
		saveMap(pendingAfterSave); //no longer an example map, so this saves and goes on
	};

	//EXPORT: writes the row's ROS 2 message to exports/<map>_<topic>.yaml, and / or copies it (or a command for it)
	auto exportItem = [&](ExportChoice choice) {
		const ExportItem& item = exportItems[choice.row];
		const std::vector<int>& path = GameBoard->pathTiles;
		std::ostringstream yaml;
		std::string missing; //why there is nothing to export, if so
		switch (choice.row) {
		case 0:
			if (GameBoard->map.hasLayer("elevation")) writeGridMapMessage(yaml, GameBoard->map, { "elevation" });
			else missing = "This map has no elevation layer.";
			break;
		case 1:
			writeGridMapMessage(yaml, GameBoard->map, GameBoard->map.getLayerNames());
			break;
		case 2:
			writeOccupancyGridMessage(yaml, GameBoard->map, GameBoard->parameters);
			break;
		case 3:
			if (!path.empty()) writePathMessage(yaml, GameBoard->map, path);
			else missing = "There is no path yet (place a start and an end it can reach).";
			break;
		case 4:
			if (!GameBoard->waypoints.empty()) writeWaypointsMessage(yaml, GameBoard->waypoints);
			else missing = "There are no waypoints yet (they come with a path).";
			break;
		case 5:
			//the positions only, as husky-stable_ws's unified_waypoint_control.py takes them on the command line (it reads
			//its last argument as the marker heights, which are not part of the plan)
			if (!GameBoard->waypoints.empty()) yaml << getWaypointListText(GameBoard->waypoints) << "\n";
			else missing = "There are no waypoints yet (they come with a path).";
			break;
		case 6:
			if (GameBoard->startExists) writePoseStampedMessage(yaml, GameBoard->map, GameBoard->startPos, path.empty() ? 0.0 : getPathYaw(GameBoard->map, path, 0));
			else missing = "There is no start yet.";
			break;
		case 7:
			if (GameBoard->endExists) writePoseStampedMessage(yaml, GameBoard->map, GameBoard->endPos, path.empty() ? 0.0 : getPathYaw(GameBoard->map, path, (int)path.size() - 1));
			else missing = "There is no goal (end) yet.";
			break;
		}
		if (!missing.empty()) {
			exportPanel.setStatus(missing);
			return;
		}


		//the file: always for Save file, and for Copy of a large item (its command reads the file)
		std::string filename = "exports/" + GameBoard->boardname + "_" + item.file; //(was "_" + item.topic + ".yaml")
		if (!choice.copy || item.large) {
			std::filesystem::create_directories("exports");
			std::ofstream file(filename);
			file << yaml.str();
		}
		if (!choice.copy) {
			exportPanel.setStatus("Saved " + filename);
			return;
		}
		if (item.large) {
			//canonical: the real path (build/exports is a link to the repository's exports/), so the command works anywhere
			std::string command = getRosPublishCommand(item.topic, item.type, std::filesystem::canonical(filename).string());
			Clipboard::setString(command);
			exportPanel.setStatus("Copied a ros2 topic pub command for " + filename);
		}
		else {
			Clipboard::setString(yaml.str());
			exportPanel.setStatus("Copied the " + item.name + (item.topic.empty() ? "" : " YAML (" + item.type + ")"));
		}
	};

	// Run the Program while the Window is Open
	while (window.isOpen())
	{
		Vector2f mouse = window.mapPixelToCoords(Mouse::getPosition(window));
		window.clear(Color(15, 15, 35));
		if (gameState == PLAYING) {
			//OLD controls (per-frame flags of the tile Board):
			//GameBoard->newClick = false;
			//GameBoard->spacePressed = false;
			//GameBoard->enterPressed = false;
			GameBoard->leftMousePos = GameBoard->getTileN(mouse.x, mouse.y); //window coordinates, so it stays right if the window is stretched
		}
		sf::Event event;
		while (window.pollEvent(event))
		{
			// When X Button Clicked, Close Window
			//(with unsaved edits on the map, ask "Save changes?" first; was: window.close() right away)
			if (event.type == sf::Event::Closed) {
				if (gameState == PLAYING && !dialog.isOpen()) leaveMap(AfterSave::CloseWindow);
				else if (gameState != PLAYING) window.close();
				continue;
			}
			//the window was resized or maximized (by the user, or by the window manager instead of our setSize()):
			//keep that size from now on and fit the layout into it
			if (event.type == sf::Event::Resized) {
				windowSize = Vector2u(event.size.width, event.size.height);
				if (windowSize != Vector2u((unsigned int)layoutW, (unsigned int)layoutH)) {
					userSizedWindow = true;
				}
				applyView();
				continue;
			}

			/* DEFINE EVENTS (i.e. KEY PRESS & RELEASE) HERE */
			if (gameState == MENU) {
				bool leftClick = (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left);
				//the open dropdown list sits on top of everything, so it gets clicks first
				if (leftClick && algorithmDropdown.handleClick(mouse)) {
					continue;
				}
				mapNameBox.handleEvent(event, mouse);
				mapSizeBox.handleEvent(event, mouse);
				if (leftClick && startButton.contains(mouse)) {
					startGame();
				}
				else if (leftClick && exitButton.contains(mouse)) {
					window.close();
				}
				else if (event.type == sf::Event::KeyPressed) {
					if (event.key.code == sf::Keyboard::Enter) {
						startGame();
					}
					else if (event.key.code == sf::Keyboard::Escape) {
						window.close();
					}
				}
				continue;
			}

			// --- PLAYING ---
			//releasing the left button always ends a terrain drag, even when a dialog that opened mid-drag takes the event
			if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
				isPaintingTerrain = false;
			}
			//an open dialog or EXPORT panel takes every event, so nothing behind it reacts
			if (dialog.isOpen()) {
				int choice = dialog.handleEvent(event, mouse);
				if (choice >= 0) handleDialogChoice(choice);
				continue;
			}
			if (exportPanel.isOpen()) {
				ExportChoice choice = exportPanel.handleEvent(event, mouse);
				if (choice.row >= 0) exportItem(choice);
				continue;
			}
			//the panel dropdowns (algorithm, view, left click, height, brush): an open list sits on top of the map and
			//panel, so they get clicks first
			//(was only the view dropdown: if (... && viewDropdown.handleClick(mouse)) { GameBoard->view = ...; continue; })
			if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left && handlePanelClick(mouse)) {
				continue;
			}
			if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left && searchedToggle.handleClick(mouse)) {
				GameBoard->showSearched = searchedToggle.isOn();
				continue;
			}
			if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left && waypointsToggle.handleClick(mouse)) {
				GameBoard->showWaypoints = waypointsToggle.isOn();
				continue;
			}
			//if (... && saveMenuButton.contains(mouse)) { returnToMenu(true); continue; } (was SAVE & MENU)
			bool leftPress = event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left;
			if (leftPress && saveButton.contains(mouse)) {
				saveMap(AfterSave::Stay);
				continue;
			}
			if (leftPress && exportButton.contains(mouse)) {
				exportPanel.open(Vector2f(layoutW / 2, layoutH / 2), std::min(620.f, layoutW - 20));
				continue;
			}
			if (leftPress && menuButton.contains(mouse)) {
				leaveMap(AfterSave::Menu);
				continue;
			}
			if (event.type == sf::Event::KeyPressed) {
				if (event.key.code == sf::Keyboard::Escape) {
					if (GameBoard->isPlacing) {
						GameBoard->cancelPlacement(); //first Esc only unselects the tile
					}
					else {
						//returnToMenu(false); //back to the menu without saving (was: never saved, never asked)
						leaveMap(AfterSave::Menu); //back to the menu, asking "Save changes?" if there are unsaved edits
					}
					continue;
				}
				//OLD controls: Space toggled the selected tile between ground and wall (see Board::convertTile())
				//if (event.key.code == sf::Keyboard::Space) {
				//	if (GameBoard->newSpace == false) {
				//		GameBoard->spacePressed = true;
				//		GameBoard->newSpace = true;
				//	}
				//	if (GameBoard->spaceWasPressed == false) {
				//		GameBoard->spaceWasPressed = true;
				//	}
				//}
				//E saved and went to the menu (same as the SAVE & MENU button); now Ctrl+S saves and Esc / MENU leaves
				//if (event.key.code == sf::Keyboard::E) {
				//	returnToMenu(true); //same as the SAVE & MENU button
				//	continue;
				//}
				//Ctrl+S: save (same as the SAVE button); checked before S alone, which toggles the searched cells
				if (event.key.code == sf::Keyboard::S && event.key.control) {
					saveMap(AfterSave::Stay);
					continue;
				}
				//OLD controls: Enter placed the start, then the end, on the selected tile (see Board::convertTile())
				//if (event.key.code == sf::Keyboard::Enter) {
				//	if (GameBoard->newEnter == false) {
				//		GameBoard->enterPressed = true;
				//		GameBoard->newEnter = true;
				//	}
				//	if (GameBoard->enterWasPressed == false) {
				//		GameBoard->enterWasPressed = true;
				//	}
				//}
				//NEW controls: Up / Down choose what to place on the selected tile, Enter places it, Backspace unselects.
				//With Set terrain, Up / Down raise / lower the brush height instead (Shift: bigger steps).
				float heightStep = event.key.shift ? brushHeightBigStep : brushHeightStep;
				if (event.key.code == sf::Keyboard::Up) {
					if (clickMode == ClickMode::SetTerrain) changeBrushElevation(+heightStep);
					else GameBoard->movePlacementChoice(-1);
				}
				if (event.key.code == sf::Keyboard::Down) {
					if (clickMode == ClickMode::SetTerrain) changeBrushElevation(-heightStep);
					else GameBoard->movePlacementChoice(+1);
				}
				//(was: Set terrain: Left / Right made the brush weaker / stronger; the brush is additive now, with no strength)
				if (event.key.code == sf::Keyboard::Enter) {
					GameBoard->confirmPlacement();
				}
				if (event.key.code == sf::Keyboard::BackSpace) {
					GameBoard->cancelPlacement();
				}
				//V: show the next layer (Terrain, Cost, Traversability, Slope, Roughness, Step)
				if (event.key.code == sf::Keyboard::V) {
					GameBoard->cycleView();
					viewDropdown.setSelectedIndex((int)GameBoard->view);
				}
				//S: show / hide the purple searched cells (same as the toggle)
				if (event.key.code == sf::Keyboard::S) {
					searchedToggle.setOn(!searchedToggle.isOn());
					GameBoard->showSearched = searchedToggle.isOn();
				}
				//W: show the path as its waypoints (yellow) or as every cell (blue), same as the Waypoints toggle
				if (event.key.code == sf::Keyboard::W) {
					waypointsToggle.setOn(!waypointsToggle.isOn());
					GameBoard->showWaypoints = waypointsToggle.isOn();
				}
				//CHARACTER (relic, not used on the costmap branch):
				//if (event.key.code == sf::Keyboard::W) {
				//	player->moveUp = true;
				//}
				//if (event.key.code == sf::Keyboard::A) {
				//	player->moveLeft = true;
				//}
				//if (event.key.code == sf::Keyboard::S) {
				//	player->moveDown = true;
				//}
				//if (event.key.code == sf::Keyboard::D) {
				//	player->moveRight = true;
				//}
			}
			if (event.type == sf::Event::KeyReleased) {
				//OLD controls (Space / Enter key release)
				//if (event.key.code == sf::Keyboard::Space) {
				//	//GameBoard->spacePressed = false;
				//	GameBoard->newSpace = false;
				//}
				//if (event.key.code == sf::Keyboard::Enter) {
				//	GameBoard->newEnter = false;
				//}
				//CHARACTER (relic, not used on the costmap branch):
				//if (event.key.code == sf::Keyboard::W) {
				//	player->moveUp = false;
				//}
				//if (event.key.code == sf::Keyboard::A) {
				//	player->moveLeft = false;
				//}
				//if (event.key.code == sf::Keyboard::S) {
				//	player->moveDown = false;
				//}
				//if (event.key.code == sf::Keyboard::D) {
				//	player->moveRight = false;
				//}
			}

			if (event.type == sf::Event::MouseButtonPressed) {
				if (event.mouseButton.button == sf::Mouse::Left) {
					//OLD controls: a left click only selected the tile for Space / Enter
					////Register a new mouse click for piece selection functions
					//GameBoard->newClick = true;
					//GameBoard->conversionReset = true;
					//if (GameBoard->hasClicked == false) {
					//	GameBoard->hasClicked = true;
					//}
					////Find the tile that was clicked in (-1 if the click was outside the map)
					//GameBoard->clickPos = GameBoard->getTileN(sf::Mouse::getPosition(window).x, sf::Mouse::getPosition(window).y);
					//// the position of mouse

					//NEW controls: click a tile to select it; click the same tile again to place the previewed type.
					//Clicking a different tile moves the selection there; clicks outside the map are ignored.
					int clickedTile = GameBoard->getTileN(mouse.x, mouse.y);
					//Set terrain: the click paints, and painting goes on while the button is held (MouseMoved below)
					if (clickMode == ClickMode::SetTerrain) {
						if (clickedTile != -1) {
							isPaintingTerrain = true;
							lastPaintedTile = -1;
							paintTerrain(clickedTile);
						}
					}
					else if (clickedTile != -1) {
						if (GameBoard->isPlacing && clickedTile == GameBoard->placingTile) {
							GameBoard->confirmPlacement();
						}
						else {
							GameBoard->beginPlacement(clickedTile);
						}
					}
				}
				if (event.mouseButton.button == sf::Mouse::Right) {
					//OLD controls: a right click selected the tile and toggled it between ground and wall at once
					////Register a new mouse click for piece selection functions
					//GameBoard->newClick = true;
					//GameBoard->conversionReset = true;
					//if (GameBoard->hasClicked == false) {
					//	GameBoard->hasClicked = true;
					//}
					////Find the tile that was clicked in
					//GameBoard->clickPos = GameBoard->getTileN(sf::Mouse::getPosition(window).x, sf::Mouse::getPosition(window).y);
					//// the position of mouse
					//if (GameBoard->newSpace == false) {
					//	GameBoard->spacePressed = true;
					//	GameBoard->newSpace = true;
					//}
					//if (GameBoard->spaceWasPressed == false) {
					//	GameBoard->spaceWasPressed = true;
					//}

					//NEW controls: a right click unselects the tile
					GameBoard->cancelPlacement();
				}
			}
			//Set terrain: dragging with the left button held paints every cell the mouse passes over; releasing it stops
			//(the event's own position, not the frame's mouse, so a fast drag paints every cell it crossed between frames)
			if (event.type == sf::Event::MouseMoved && isPaintingTerrain) {
				Vector2f dragPosition = window.mapPixelToCoords(Vector2i(event.mouseMove.x, event.mouseMove.y));
				paintTerrain(GameBoard->getTileN(dragPosition.x, dragPosition.y));
			}
			if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
				isPaintingTerrain = false;
			}
			//OLD controls (right-click release)
			//if (event.type == sf::Event::MouseButtonReleased) {
			//	if (event.mouseButton.button == sf::Mouse::Right) {
			//		//Register a new mouse click for piece selection functions
			//		GameBoard->newSpace = false;
			//	}
			//}
		}

		/* DEFINE ACTIONS (i.e. MOVEMENT) HERE */

		/* DRAW OBJECTS HERE */
		if (gameState == MENU) {
			window.draw(title);
			window.draw(mapLabel);
			mapNameBox.draw(window);
			window.draw(savedMapsText);
			window.draw(algorithmLabel);
			window.draw(sizeLabel);
			mapSizeBox.draw(window);
			window.draw(sizeHint);
			startButton.draw(window, mouse);
			exitButton.draw(window, mouse);
			algorithmDropdown.draw(window, mouse); //drawn last so its open list covers what is below it
			window.display();
			continue;
		}

		// --- PLAYING ---
		//OLD controls: convertTile() applied the Space / Enter edits; tiles are now placed with Board::confirmPlacement()
		//GameBoard->convertTile();
		if (!GameBoard->terrainEditedTiles.empty()) {
			//the terrain was painted this frame: rebuild the cost around the painted cells (once per frame, however many
			//cells a drag crossed); this marks the plan out of date, so it is re-planned just below
			GameBoard->updateCostMap();
		}
		if (GameBoard->navDirty) {
			//start / end changed (or a map was loaded): re-plan on the cost layer with the chosen algorithm;
			//searched cells are tinted purple, the path is blue
			GameBoard->replan(algorithm);
		}
		GameBoard->drawBoard(window);
		//Set terrain: outline the cells the brush would paint under the mouse
		if (clickMode == ClickMode::SetTerrain) {
			GameBoard->drawTerrainBrush(window, GameBoard->leftMousePos, getBrushWidths()[brushDropdown.getSelectedIndex()]);
		}
		//CHARACTER (relic, not used on the costmap branch):
		//player->update();
		//player->drawCharacter(window);

		//list of what can be placed, next to the selected tile (the highlighted entry is the one previewed on the tile)
		if (GameBoard->isPlacing) {
			const float rowW = 140, rowH = 28;
			int optionCount = (int)GameBoard->placementOptions.size();
			Vector2f tilePos = GameBoard->getTilePos(GameBoard->placingTile);
			float listX = tilePos.x + GameBoard->tileW + 6;
			float boardRight = GameBoard->origin.x + GameBoard->boardW, boardBottom = GameBoard->origin.y + GameBoard->boardH;
			if (listX + rowW > boardRight) {
				listX = tilePos.x - rowW - 6; //no room on the right of the tile: show it on the left
			}
			float listY = tilePos.y;
			if (listY + optionCount * rowH > boardBottom) {
				listY = boardBottom - optionCount * rowH; //keep it inside the map at the bottom
			}
			for (int i = 0; i < optionCount; i++) {
				bool chosen = (i == GameBoard->placementChoice);
				RectangleShape row(Vector2f(rowW, rowH));
				row.setPosition(listX, listY + i * rowH);
				row.setFillColor(chosen ? Color(70, 70, 140) : Color(25, 25, 55, 230));
				row.setOutlineColor(chosen ? Color::Cyan : Color(150, 150, 150));
				row.setOutlineThickness(chosen ? -2.f : -1.f);
				window.draw(row);
				Text optionText((chosen ? "> " : "  ") + GameBoard->getPlacementLabel(GameBoard->placementOptions[i]), font, 17);
				optionText.setPosition(listX + 6, listY + i * rowH + 3);
				window.draw(optionText);
			}
		}

		//side panel: map, algorithm, result, controls, and the SAVE & MENU button
		//formats a number with a fixed count of decimals
		auto toText = [](double value, int decimals) {
			std::ostringstream out;
			out << std::fixed << std::setprecision(decimals) << value;
			return out.str();
		};
		//"*": unsaved edits
		std::string info = "Map: " + GameBoard->boardname + (GameBoard->hasUnsavedChanges ? " *" : "") + (GameBoard->mapIsNew ? " (new)" : "") + "\n";
		info += "Size: " + std::to_string(GameBoard->map.getColumns()) + " x " + std::to_string(GameBoard->map.getRows()) + " cells ("
			+ toText(GameBoard->map.getColumns() * GameBoard->map.getResolution(), 1) + " m)\n";
		//info += "Algorithm: " + getPathAlgorithmName(algorithm) + "\n"; (was text only; now a dropdown)
		//info += "View (V):\n\n\n\n"; (was the only dropdown gap)
		//each showing dropdown's label, then the two empty lines it is drawn in (layoutPanel() uses the same order)
		for (const std::pair<std::string, Dropdown*>& dropdown : getPanelDropdowns()) {
			info += dropdown.first + "\n\n\n";
		}
		info += "\n\n"; //the searched-cells toggle is drawn in the first of these lines (layoutPanel()), the second is a gap
		if (!(GameBoard->startExists && GameBoard->endExists)) {
			info += "Place a start and an end:\nclick a cell, then pick\nStart or End\n";
		}
		else if (GameBoard->pathTiles.empty()) {
			info += "No path!\n";
			info += "Searched: " + std::to_string(GameBoard->searchedTiles.size()) + " cells\n";
		}
		else {
			//path length in metres: each straight step is one cell, each diagonal step sqrt(2) cells
			double lengthCells = 0;
			for (std::size_t i = 1; i < GameBoard->pathTiles.size(); i++) {
				lengthCells += getHeuristic(GameBoard->map, GameBoard->pathTiles[i - 1], GameBoard->pathTiles[i]);
			}
			info += "Path: " + std::to_string(GameBoard->pathTiles.size()) + " cells, "
				+ toText(lengthCells * GameBoard->map.getResolution(), 1) + " m\n";
			//info += "Path cost: " + toText(GameBoard->pathCost, 1) + "\n"; (cost and searched share a line now, so the
			//info += "Searched: " + std::to_string(GameBoard->searchedTiles.size()) + " cells\n"; waypoints fit in three)
			info += "Cost: " + toText(GameBoard->pathCost, 1) + ", searched: " + std::to_string(GameBoard->searchedTiles.size()) + "\n";
			double waypointsTime = GameBoard->waypoints.empty() ? 0.0 : GameBoard->waypoints.back().time;
			info += "Waypoints: " + std::to_string(GameBoard->waypoints.size()) + ", " + toText(waypointsTime, 1) + " s\n";
		}
		//info += "\nControls:\n"; (the heading is gone: the key / mouse sprites say what they are, and its line holds the S / W row)
		info += "\n";
		//the controls as key / mouse sprites and words (ControlsList), one row per line of the panel text; these were
		//text lines (terrain: "Click / drag: set height", "Up/Down: height (Shift x5)", "Left/Right: strength",
		//"V: view, Esc: menu"; start / end: "Click: select a cell", "Up / Down: choose", "Click again / Enter: place",
		//"Right-click / Backspace:\n  unselect", "V: view, Ctrl+S: save", "Esc: unselect, then menu")
		std::vector<std::vector<ControlItem>> controlRows;
		if (clickMode == ClickMode::SetTerrain) {
			controlRows = {
				{ { ControlIcon::MouseLeft, "" }, { ControlIcon::Text, "click / drag: add height" } },
				{ { ControlIcon::ArrowUp, "" }, { ControlIcon::ArrowDown, "" }, { ControlIcon::Text, "height" }, { ControlIcon::WideKey, "Shift" }, { ControlIcon::Text, "x5" } },
				//{ { ControlIcon::ArrowLeft, "" }, { ControlIcon::ArrowRight, "" }, { ControlIcon::Text, "strength" } }, (was)
				{ { ControlIcon::Key, "V" }, { ControlIcon::Text, "view" }, { ControlIcon::WideKey, "Esc" }, { ControlIcon::Text, "menu" } },
				{ { ControlIcon::Key, "S" }, { ControlIcon::Text, "searched" }, { ControlIcon::Key, "W" }, { ControlIcon::Text, "waypoints" } }
			};
		}
		else {
			controlRows = {
				{ { ControlIcon::MouseLeft, "" }, { ControlIcon::Text, "select a cell" } },
				{ { ControlIcon::ArrowUp, "" }, { ControlIcon::ArrowDown, "" }, { ControlIcon::Text, "choose" } },
				{ { ControlIcon::MouseLeft, "" }, { ControlIcon::Text, "again /" }, { ControlIcon::WideKey, "Enter" }, { ControlIcon::Text, "place" } },
				{ { ControlIcon::MouseRight, "" }, { ControlIcon::Text, "/" }, { ControlIcon::WideKey, "Back" }, { ControlIcon::Text, "unselect" } },
				{ { ControlIcon::Key, "V" }, { ControlIcon::Text, "view" }, { ControlIcon::WideKey, "Ctrl" }, { ControlIcon::Key, "S" }, { ControlIcon::Text, "save" } },
				{ { ControlIcon::WideKey, "Esc" }, { ControlIcon::Text, "unselect, then menu" } },
				{ { ControlIcon::Key, "S" }, { ControlIcon::Text, "searched" }, { ControlIcon::Key, "W" }, { ControlIcon::Text, "waypoints" } }
			};
		}
		//the rows start on the line after the blank one; leave that many empty lines in the text for them
		int controlsLine = (int)std::count(info.begin(), info.end(), '\n');
		info += std::string(controlRows.size(), '\n');
		panelText.setString(info);
		window.draw(panelText);
		controlsList.draw(window, Vector2f(panelLeft, boardMargin + controlsLine * panelLineHeight), controlRows);
		//the colour scale of the current view, in its column left of the panel text, controls and buttons
		GameBoard->drawLegend(window, font, FloatRect(panelLeft - legendColumnWidth, boardMargin, legendColumnWidth - 12, panelBottom - boardMargin));
		//saveMenuButton.draw(window, mouse);
		saveButton.draw(window, mouse);
		exportButton.draw(window, mouse);
		menuButton.draw(window, mouse);
		searchedToggle.draw(window, mouse); //before the dropdowns, so an open list covers it
		waypointsToggle.draw(window, mouse);
		//viewDropdown.draw(window, mouse); (was drawn alone, last)
		//the dropdowns are drawn last, bottom one first, so an open list covers the map, the panel and the dropdowns below it
		std::vector<std::pair<std::string, Dropdown*>> panelDropdowns = getPanelDropdowns();
		for (int i = (int)panelDropdowns.size() - 1; i >= 0; i--) {
			panelDropdowns[i].second->draw(window, mouse);
		}
		//the toast: a short message over the bottom of the map, for toastSeconds after saving
		if (!toastText.empty() && toastClock.getElapsedTime().asSeconds() < toastSeconds) {
			Text toast(toastText, font, 17);
			FloatRect bounds = toast.getLocalBounds();
			RectangleShape toastBox(Vector2f(bounds.width + 24, 34));
			toastBox.setOrigin(toastBox.getSize() / 2.f);
			toastBox.setPosition(GameBoard->origin.x + GameBoard->boardW / 2, GameBoard->origin.y + GameBoard->boardH - 30);
			toastBox.setFillColor(Color(25, 25, 55, 230));
			toastBox.setOutlineColor(Color(150, 220, 150));
			toastBox.setOutlineThickness(2);
			window.draw(toastBox);
			centerText(toast, toastBox.getPosition().x, toastBox.getPosition().y);
			window.draw(toast);
		}
		//the dialog and the EXPORT panel go over everything
		exportPanel.draw(window, mouse);
		dialog.draw(window, mouse);
		/* DRAW OBJECTS HERE */

		window.display();
	}
}
