#pragma once
#include <SFML/Graphics.hpp>
#include <string>

// A clickable button: a rectangle with its text centered on it. It lights up while the mouse is over it.
// (Same idea as the Button in PingPong, moved into its own file.)
class Button {
public:
	void setup(const sf::Font& font, const std::string& text, sf::Vector2f center, sf::Vector2f size, unsigned int characterSize = 32);
	bool contains(sf::Vector2f point) const;
	void draw(sf::RenderWindow& window, sf::Vector2f mouse);

private:
	sf::RectangleShape box;
	sf::Text label;
};
