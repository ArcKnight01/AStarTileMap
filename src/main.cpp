// iD Tech Game Plan - SFML Template
// Include Needed for SFML
#include <SFML/Graphics.hpp>
// Include Needed for Animations
#include <SFML/Graphics/Rect.hpp>
// Include Needed for Text
#include <SFML/Graphics/Text.hpp>
#include <iomanip>
#include <memory>
#include <sstream>
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
const float windowW = 1100;
const float windowH = 800;
//(the map's size in cells now comes from the loaded .gridmap file)
const float boardMargin = 40;                             //empty space between the window edge and the map
const float boardSize = windowH - 2 * boardMargin;         //the map is square: 720 x 720
const float panelLeft = 2 * boardMargin + boardSize;       //the side panel starts right of the map
const float panelCenterX = (panelLeft + windowW - boardMargin) / 2;

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
	centerText(title, windowW / 2, 110);

	Text mapLabel("Map name", font, 24);
	mapLabel.setPosition(windowW / 2 - 230, 230);
	TextBox mapNameBox;
	mapNameBox.setup(font, Vector2f(windowW / 2, 300), Vector2f(460, 60), "flat");
	if (argc > 1) {
		mapNameBox.setText(argv[1]);
	}
	Text savedMapsText("", font, 18); //filled by refreshSavedMaps()
	savedMapsText.setFillColor(Color(170, 170, 170));

	Text algorithmLabel("Algorithm", font, 24);
	algorithmLabel.setPosition(windowW / 2 - 230, 400);
	Dropdown algorithmDropdown;
	algorithmDropdown.setup(font, Vector2f(windowW / 2, 470), Vector2f(460, 56), getPathAlgorithmNames());

	Button startButton;
	startButton.setup(font, "START", Vector2f(windowW / 2, 660), Vector2f(300, 80), 40);

	// --- PLAYING screen: side panel to the right of the map ---
	Button saveMenuButton;
	saveMenuButton.setup(font, "SAVE & MENU", Vector2f(panelCenterX, windowH - boardMargin - 40), Vector2f(240, 70), 26);
	Text panelText("", font, 19);
	panelText.setPosition(panelLeft, boardMargin);

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
		savedMapsText.setPosition(windowW / 2 - 230, 342);
	};
	refreshSavedMaps();

	//MENU -> PLAYING: load (or create) the map named in the text box
	auto startGame = [&]() {
		algorithm = (PathAlgorithm)algorithmDropdown.getSelectedIndex();
		//CHARACTER (relic): player.reset(); //the character points at the board, so remove it before replacing the board
		GameBoard = std::make_unique<Board>(boardSize, boardSize, Vector2f(boardMargin, boardMargin));
		GameBoard->boardname = mapNameBox.getText();
		GameBoard->importBoard(GameBoard->boardname); //cleans the name: "" -> "flat", "x.gridmap" -> "x"
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
			GameBoard->leftMousePos = GameBoard->getTileN(sf::Mouse::getPosition(window).x, sf::Mouse::getPosition(window).y);
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
					int clickedTile = GameBoard->getTileN(sf::Mouse::getPosition(window).x, sf::Mouse::getPosition(window).y);
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
			const float rowW = 110, rowH = 28;
			int optionCount = (int)GameBoard->placementOptions.size();
			Vector2f tilePos = GameBoard->getTilePos(GameBoard->placingTile);
			float listX = tilePos.x + GameBoard->tileW + 6;
			if (listX + rowW > boardMargin + boardSize) {
				listX = tilePos.x - rowW - 6; //no room on the right of the tile: show it on the left
			}
			float listY = tilePos.y;
			if (listY + optionCount * rowH > boardMargin + boardSize) {
				listY = boardMargin + boardSize - optionCount * rowH; //keep it inside the map at the bottom
			}
			for (int i = 0; i < optionCount; i++) {
				bool chosen = (i == GameBoard->placementChoice);
				RectangleShape row(Vector2f(rowW, rowH));
				row.setPosition(listX, listY + i * rowH);
				row.setFillColor(chosen ? Color(70, 70, 140) : Color(25, 25, 55, 230));
				row.setOutlineColor(chosen ? Color::Cyan : Color(150, 150, 150));
				row.setOutlineThickness(chosen ? -2.f : -1.f);
				window.draw(row);
				Text optionText((chosen ? "> " : "  ") + Board::getPlacementName(GameBoard->placementOptions[i]), font, 17);
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
		info += "Algorithm: " + getPathAlgorithmName(algorithm) + "\n";
		info += "View: " + getMapViewNames()[(int)GameBoard->view] + "  (V)\n\n";
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
		/* DRAW OBJECTS HERE */

		window.display();
	}
}
