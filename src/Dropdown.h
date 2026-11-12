#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

// A dropdown list: shows the selected option; click it to open the list, click an option to pick it.
// Draw it after everything it can overlap, so the open list appears on top.
class Dropdown {
public:
	void setup(const sf::Font& font, sf::Vector2f center, sf::Vector2f size, const std::vector<std::string>& options, int selected = 0);
	// Call on a left click. Returns true when the dropdown used the click (so nothing underneath should react to it).
	bool handleClick(sf::Vector2f mouse);
	void draw(sf::RenderWindow& window, sf::Vector2f mouse);

	int getSelectedIndex() const;
	void setSelectedIndex(int index); //e.g. when a key changes the same setting
	/**
	 * Move the dropdown, keeping its size, options and selection (e.g. when the side panel is laid out again).
	 *
	 * @param center  New centre of the closed box, in window coordinates.
	 */
	void setCenter(sf::Vector2f center);
	std::string getSelectedText() const;
	bool isOpen() const;

private:
	sf::FloatRect getOptionBounds(int index) const;

	const sf::Font* font = nullptr;
	sf::Vector2f topLeft;
	sf::Vector2f size;
	std::vector<std::string> options;
	int selected = 0;
	bool open = false;
};
