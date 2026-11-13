#pragma once
#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <vector>

// The side panel's Controls list: each row is a few items drawn left to right, each a piece of text, a mouse button
// or a key, using the sprites in assets/Textures/controls (made by tools/make_control_sprites.py). Keys are blank
// keycaps with their label written on them in the app's font, so one sprite serves every letter.
//
//   [mouse L] select a cell
//   [^][v] choose
//   [Esc] unselect, then menu

// What one item of a row is.
enum class ControlIcon {
	Text,       //label is drawn as text
	Key,        //a 20 x 20 keycap with label on it (one or two characters: "V", "S")
	WideKey,    //a 40 x 20 keycap with label on it ("Esc", "Ctrl", "Shift", "Enter", "Back")
	ArrowUp,    //the arrow keys (label unused)
	ArrowDown,
	ArrowLeft,
	ArrowRight,
	MouseLeft,  //a mouse with its left button pressed (label unused)
	MouseRight
};

// One item of a row: what it is, and its text or key label.
struct ControlItem {
	ControlIcon icon;
	std::string label;
};

class ControlsList {
public:
	/**
	 * Load the control sprites and set the text style and the row height.
	 *
	 * @param font           Font of the text and the key labels (kept by pointer, so it must outlive the list).
	 * @param characterSize  Size of the text items, in pixels.
	 * @param lineHeight     Height of one row, in pixels (the panel text's line height, so rows line up with it).
	 */
	void setup(const sf::Font& font, unsigned int characterSize, float lineHeight);
	/**
	 * Draw rows of items, one row per line, starting at a point.
	 *
	 * @param window   Window (or texture) to draw into.
	 * @param topLeft  Top-left corner of the first row, in window coordinates (pixels).
	 * @param rows     The rows, top to bottom; each row's items left to right.
	 */
	void draw(sf::RenderTarget& window, sf::Vector2f topLeft, const std::vector<std::vector<ControlItem>>& rows);

private:
	const sf::Font* font = nullptr;
	unsigned int characterSize = 17;
	float lineHeight = 25;
	std::map<ControlIcon, sf::Texture> textures;
};
