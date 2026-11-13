#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

// A dropdown list: shows the selected option; click it to open the list, click an option to pick it.
// Draw it after everything it can overlap, so the open list appears on top.
class Dropdown {
public:
	/**
	 * Set up the dropdown's place, options and starting selection; it starts closed.
	 *
	 * @param font      Font of the text (kept by pointer, so it must outlive the dropdown).
	 * @param center    Centre of the closed box, in window coordinates (pixels).
	 * @param size      Size of the closed box and of each option row, in pixels.
	 * @param options   Text of each option, top to bottom.
	 * @param selected  Index of the option selected at the start.
	 */
	void setup(const sf::Font& font, sf::Vector2f center, sf::Vector2f size, const std::vector<std::string>& options, int selected = 0);
	/**
	 * Call on a left click: opens the list, picks an option, or closes the list.
	 *
	 * @param mouse  Click position, in window coordinates (pixels).
	 * @return       True when the dropdown used the click (so nothing underneath should react to it).
	 */
	bool handleClick(sf::Vector2f mouse);
	/**
	 * Draw the closed box with its arrow and, when open, the option list with the hovered row lit.
	 *
	 * @param window  Window to draw into.
	 * @param mouse   Mouse position, in window coordinates (pixels).
	 */
	void draw(sf::RenderWindow& window, sf::Vector2f mouse);

	/**
	 * Get the index of the selected option.
	 *
	 * @return  Index into the options given to setup().
	 */
	int getSelectedIndex() const;
	/**
	 * Select an option without a click; an index outside the list is ignored.
	 *
	 * @param index  Index of the option to select.
	 */
	void setSelectedIndex(int index); //e.g. when a key changes the same setting
	/**
	 * Move the dropdown, keeping its size, options and selection (e.g. when the side panel is laid out again).
	 *
	 * @param center  New centre of the closed box, in window coordinates.
	 */
	void setCenter(sf::Vector2f center);
	/**
	 * Get the text of the selected option.
	 *
	 * @return  The selected option's text, or "" when there are no options.
	 */
	std::string getSelectedText() const;
	/**
	 * Check whether the option list is open.
	 *
	 * @return  True while the list is shown.
	 */
	bool isOpen() const;

private:
	/**
	 * Get the rectangle of an option row in the open list (option 0 directly under the closed box).
	 *
	 * @param index  Index of the option.
	 * @return       The row's rectangle, in window coordinates (pixels).
	 */
	sf::FloatRect getOptionBounds(int index) const;

	const sf::Font* font = nullptr;
	sf::Vector2f topLeft;
	sf::Vector2f size;
	std::vector<std::string> options;
	int selected = 0;
	bool open = false;
};
