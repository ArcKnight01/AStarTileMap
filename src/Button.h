#pragma once
#include <SFML/Graphics.hpp>
#include <string>

// A clickable button: a rectangle with its text centered on it. It lights up while the mouse is over it.
// (Same idea as the Button in PingPong, moved into its own file.)
class Button {
public:
	/**
	 * Set up the button's box and centred label.
	 *
	 * @param font           Font of the label (kept by pointer, so it must outlive the button).
	 * @param text           Label shown on the button.
	 * @param center         Centre of the box, in window coordinates (pixels).
	 * @param size           Size of the box, in pixels.
	 * @param characterSize  Size of the label text, in pixels.
	 */
	void setup(const sf::Font& font, const std::string& text, sf::Vector2f center, sf::Vector2f size, unsigned int characterSize = 32);
	/**
	 * Check whether a point lies on the button.
	 *
	 * @param point  Point to test, in window coordinates (pixels).
	 * @return       True when the point is inside the box.
	 */
	bool contains(sf::Vector2f point) const;
	/**
	 * Draw the button, lighter while the mouse is over it.
	 *
	 * @param window  Window (or texture) to draw into.
	 * @param mouse   Mouse position, in window coordinates (pixels).
	 */
	void draw(sf::RenderTarget& window, sf::Vector2f mouse); //(was sf::RenderWindow&; any target, so it can be drawn off-screen)

private:
	sf::RectangleShape box;
	sf::Text label;
};
