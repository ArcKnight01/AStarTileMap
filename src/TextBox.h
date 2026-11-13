#pragma once
#include <SFML/Graphics.hpp>
#include <string>

// A one-line text box. Click it to type; click elsewhere to stop. Shows grey placeholder text while empty.
// It only stores text, so the same box can later be filled from a dropdown of saved maps.
class TextBox {
public:
	/**
	 * Set up the text box's place, placeholder and length limit.
	 *
	 * @param font         Font of the text (kept by pointer, so it must outlive the text box).
	 * @param center       Centre of the box, in window coordinates (pixels).
	 * @param size         Size of the box, in pixels.
	 * @param placeholder  Grey text shown while the box is empty and inactive.
	 * @param maxLength    Largest number of characters that can be typed.
	 */
	void setup(const sf::Font& font, sf::Vector2f center, sf::Vector2f size, const std::string& placeholder, std::size_t maxLength = 30);
	/**
	 * Pass every event here: mouse clicks focus / unfocus it, typed characters and Backspace edit it.
	 *
	 * @param event  The SFML event.
	 * @param mouse  Mouse position, in window coordinates (pixels).
	 */
	void handleEvent(const sf::Event& event, sf::Vector2f mouse);
	/**
	 * Draw the box with its text (and a "_" cursor while active), or the placeholder while empty.
	 *
	 * @param window  Window to draw into.
	 */
	void draw(sf::RenderWindow& window);

	/**
	 * Get the typed text.
	 *
	 * @return  The text, without the placeholder or cursor.
	 */
	std::string getText() const;
	/**
	 * Replace the text, cut to the maximum length.
	 *
	 * @param newText  The new text.
	 */
	void setText(const std::string& newText);
	/**
	 * Check whether the box has focus (receives typing).
	 *
	 * @return  True while the box is active.
	 */
	bool isActive() const;
	/**
	 * Give or take away the box's focus.
	 *
	 * @param isNowActive  True to focus the box.
	 */
	void setActive(bool isNowActive);

private:
	sf::RectangleShape box;
	sf::Text shownText;
	std::string text;
	std::string placeholder;
	std::size_t maxLength = 30;
	bool active = false;
};
