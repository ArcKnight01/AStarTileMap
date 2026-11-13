#pragma once
#include <SFML/Graphics.hpp>
#include <string>

// A rectangular on / off switch with its label: a box with the label on the left and a switch on the right. Click it to
// flip it. When on, the knob sits on the right and the track takes onColor; when off, the knob sits on the left and
// the track is grey. It lights up while the mouse is over it, like Button.
//
//   +------------------------------------+
//   | Searched (S)               [  |##]   on   (track 2 : 1, square knob)
//   | Searched (S)               [##|  ]   off
//   +------------------------------------+
class Toggle {
public:
	/**
	 * Set up the toggle's look, place and starting state.
	 *
	 * @param font           Font of the label (kept by pointer, so it must outlive the toggle).
	 * @param text           Label shown on the left.
	 * @param center         Centre of the box, in window coordinates (pixels).
	 * @param size           Size of the box, in pixels.
	 * @param on             Starting state.
	 * @param onColor        Colour of the track when on.
	 * @param characterSize  Size of the label text, in pixels.
	 */
	void setup(const sf::Font& font, const std::string& text, sf::Vector2f center, sf::Vector2f size, bool on,
		sf::Color onColor = sf::Color(70, 170, 200), unsigned int characterSize = 18);
	/**
	 * Move the toggle, keeping its look and state (e.g. when the side panel is laid out again).
	 *
	 * @param center  New centre of the box, in window coordinates.
	 */
	void setCenter(sf::Vector2f center);
	/**
	 * Give the toggle a left click: it flips if the click is on it.
	 *
	 * @param mouse  Click position, in window coordinates.
	 * @return       True when the click was on the toggle (so nothing underneath should react to it).
	 */
	bool handleClick(sf::Vector2f mouse);
	/**
	 * Check whether a point lies on the toggle.
	 *
	 * @param point  Point to test, in window coordinates (pixels).
	 * @return       True when the point is inside the box.
	 */
	bool contains(sf::Vector2f point) const;
	/**
	 * Get the toggle's state.
	 *
	 * @return  True when on.
	 */
	bool isOn() const;
	/**
	 * Set the toggle's state without a click.
	 *
	 * @param on  New state.
	 */
	void setOn(bool on); //e.g. when a key changes the same setting
	/**
	 * Draw the box (lighter while hovered), the label and the switch.
	 *
	 * @param window  Window to draw into.
	 * @param mouse   Mouse position, in window coordinates (pixels).
	 */
	void draw(sf::RenderWindow& window, sf::Vector2f mouse);

private:
	sf::RectangleShape box;
	sf::Text label;
	bool on = true;
	sf::Color onColor;
};
