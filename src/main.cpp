// iD Tech Game Plan - SFML Template
// Include Needed for SFML
#include <SFML/Graphics.hpp>
// Include Needed for Animations
#include <SFML/Graphics/Rect.hpp>
// Include Needed for Text
#include <SFML/Graphics/Text.hpp>
#include <algorithm>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include "Tile.h"
//CHARACTER (relic, not used on the costmap branch): #include "character.h"
#include "Button.h"
#include "TextBox.h"
#include "Dropdown.h"

using namespace sf;
/* DEFINE GAMESTATES HERE */
// Which screen is showing:
//   MENU    --START (click it, or press Enter)----------> PLAYING
//   PLAYING --SAVE & MENU (click it, or press E)--------> MENU  (saves the map first)
//   PLAYING --Esc---------------------------------------> MENU  (without saving)
enum GameState {
	MENU,
	PLAYING
};

//constants here:
const float windowW = 1100; //the MENU window
const float windowH = 800;
const float boardMargin = 40;  //empty space between the window edge and the map
const float panelWidth = 260;  //the side panel right of the map
//PLAYING: the window resizes to the map. Each cell gets the biggest of these sizes that fits on the screen, so the
//20 x 20 sprites are always drawn at a clean scale (x2, x1, x1/2, x1/4); only a map too big even for 5 px cells is
//squeezed to fit.
const float cellSizes[] = { 40, 20, 10, 5 };

//places a text so that its centre is at (x, y)
void centerText(Text& text, float x, float y) {
	FloatRect bounds = text.getLocalBounds();
	text.setOrigin(bounds.left + bounds.width / 2, bounds.top + bounds.height / 2);
	text.setPosition(x, y);
}

//main code (an optional map name on the command line pre-fills the menu's text box: ./AStarTileMap dungeon)
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

	Button startButton;
	startButton.setup(font, "START", Vector2f(windowW / 2, 705), Vector2f(300, 70), 40);

	// --- PLAYING screen: side panel to the right of the map ---
	Button saveMenuButton;
	//(positioned by layoutForMap() when a map starts)
	Text panelText("", font, 19);
	//the view (same as pressing V); it sits in the gap left under the "View (V):" line of the panel text
	Dropdown viewDropdown;
	viewDropdown.setup(font, Vector2f(0, 0), Vector2f(240, 40), getMapViewNames());

	//resizes the window to a window (width x height) and keeps it on the screen, with 1 drawing unit = 1 pixel
	auto resizeWindow = [&](float width, float height) {
		window.setSize(Vector2u((unsigned int)width, (unsigned int)height));
		window.setView(View(FloatRect(0, 0, width, height)));
		VideoMode desktop = VideoMode::getDesktopMode();
		window.setPosition(Vector2i(std::max(0, ((int)desktop.width - (int)width) / 2), std::max(0, ((int)desktop.height - (int)height) / 2)));
	};

	//PLAYING layout for the loaded map: cell size, window size, the board's place, and the side panel
	auto layoutForMap = [&](Board& board) {
		VideoMode desktop = VideoMode::getDesktopMode();
		//room for the board on this screen (leaving space for the title bar, a taskbar and the side panel)
		float roomW = std::max(200.f, (float)desktop.width - 2 * boardMargin - panelWidth - boardMargin - 40);
		float roomH = std::max(200.f, (float)desktop.height - 2 * boardMargin - 120);
		int columns = board.map.getColumns(), rows = board.map.getRows();
		float cell = 0;
		for (float size : cellSizes) {
			if (columns * size <= roomW && rows * size <= roomH) { cell = size; break; }
		}
		if (cell == 0) cell = std::min(roomW / columns, roomH / rows); //too big even for 5 px cells: squeeze it
		float boardPixelsW = columns * cell, boardPixelsH = rows * cell;
		//the panel needs about windowH of height (text, view dropdown, SAVE & MENU), so small maps get a taller window
		float playW = boardMargin + boardPixelsW + boardMargin + panelWidth + boardMargin;
		float playH = std::max(windowH, boardPixelsH + 2 * boardMargin);
		board.setScreenArea(boardPixelsW, boardPixelsH, Vector2f(boardMargin, (playH - boardPixelsH) / 2));
		resizeWindow(playW, playH);
		float panelLeft = 2 * boardMargin + boardPixelsW;
		panelText.setPosition(panelLeft, boardMargin);
		viewDropdown.setup(font, Vector2f(panelLeft + 120, boardMargin + 4 * 25 + 25), Vector2f(240, 40), getMapViewNames(), viewDropdown.getSelectedIndex());
		saveMenuButton.setup(font, "SAVE & MENU", Vector2f(panelLeft + panelWidth / 2, playH - boardMargin - 40), Vector2f(240, 70), 26);
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
		//CHARACTER (relic): player.reset(); //the character points at the board, so remove it before replacing the board
		GameBoard = std::make_unique<Board>(windowH - 2 * boardMargin, windowH - 2 * boardMargin, Vector2f(boardMargin, boardMargin));
		GameBoard->boardname = mapNameBox.getText();
		GameBoard->newMapSize = getNewMapSize(); //only used if the map doesn't exist yet
		GameBoard->view = (MapView)viewDropdown.getSelectedIndex(); //keep the view chosen last time
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

	//PLAYING -> MENU, saving the map first if asked
	auto returnToMenu = [&](bool save) {
		if (save) {
			GameBoard->exportBoard(GameBoard->boardname);
		}
		mapNameBox.setText(GameBoard->boardname); //so START reopens the same map
		refreshSavedMaps();
		window.setTitle("Aidan's TileMap");
		resizeWindow(windowW, windowH); //back to the menu's size
		gameState = MENU;
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
			if (event.type == sf::Event::Closed)
				window.close();

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
			//the open view list sits on top of the map and panel, so it gets clicks first
			if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left && viewDropdown.handleClick(mouse)) {
				GameBoard->view = (MapView)viewDropdown.getSelectedIndex();
				continue;
			}
			if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left && saveMenuButton.contains(mouse)) {
				returnToMenu(true);
				continue;
			}
			if (event.type == sf::Event::KeyPressed) {
				if (event.key.code == sf::Keyboard::Escape) {
					if (GameBoard->isPlacing) {
						GameBoard->cancelPlacement(); //first Esc only unselects the tile
					}
					else {
						returnToMenu(false); //back to the menu without saving
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
				if (event.key.code == sf::Keyboard::E) {
					returnToMenu(true); //same as the SAVE & MENU button
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
				//NEW controls: Up / Down choose what to place on the selected tile, Enter places it, Backspace unselects
				if (event.key.code == sf::Keyboard::Up) {
					GameBoard->movePlacementChoice(-1);
				}
				if (event.key.code == sf::Keyboard::Down) {
					GameBoard->movePlacementChoice(+1);
				}
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
					if (clickedTile != -1) {
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
			algorithmDropdown.draw(window, mouse); //drawn last so its open list covers what is below it
			window.display();
			continue;
		}

		// --- PLAYING ---
		//OLD controls: convertTile() applied the Space / Enter edits; tiles are now placed with Board::confirmPlacement()
		//GameBoard->convertTile();
		if (GameBoard->navDirty) {
			//start / end changed (or a map was loaded): re-plan on the cost layer with the chosen algorithm;
			//searched cells are tinted purple, the path is blue
			GameBoard->replan(algorithm);
		}
		GameBoard->drawBoard(window);
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
		std::string info = "Map: " + GameBoard->boardname + (GameBoard->mapIsNew ? " (new)" : "") + "\n";
		info += "Size: " + std::to_string(GameBoard->map.getColumns()) + " x " + std::to_string(GameBoard->map.getRows()) + " cells ("
			+ toText(GameBoard->map.getColumns() * GameBoard->map.getResolution(), 1) + " m)\n";
		info += "Algorithm: " + getPathAlgorithmName(algorithm) + "\n";
		info += "View (V):\n\n\n\n"; //the view dropdown is drawn in these lines
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
			info += "Path cost: " + toText(GameBoard->pathCost, 1) + "\n";
			info += "Searched: " + std::to_string(GameBoard->searchedTiles.size()) + " cells\n";
		}
		info += "\nControls:\n";
		info += "Click: select a cell\n";
		info += "Up / Down: choose\n";
		info += "Click again / Enter: place\n";
		info += "Right-click / Backspace:\n  unselect\n";
		info += "V: change view\n";
		info += "Esc: unselect, then menu\n";
		panelText.setString(info);
		window.draw(panelText);
		saveMenuButton.draw(window, mouse);
		viewDropdown.draw(window, mouse); //drawn last so its open list covers the map and panel
		/* DRAW OBJECTS HERE */

		window.display();
	}
}
