#pragma once
#include <SFML/Graphics.hpp>
#include <string>

// A one-line text box. Click it to type; click elsewhere to stop. Shows grey placeholder text while empty.
// It only stores text, so the same box can later be filled from a dropdown of saved maps.
class TextBox {
public:
	void setup(const sf::Font& font, sf::Vector2f center, sf::Vector2f size, const std::string& placeholder, std::size_t maxLength = 30);
	// Pass every event here: mouse clicks focus / unfocus it, typed characters and Backspace edit it.
	void handleEvent(const sf::Event& event, sf::Vector2f mouse);
	void draw(sf::RenderWindow& window);

	std::string getText() const;
	void setText(const std::string& newText);
	bool isActive() const;
	void setActive(bool isNowActive);

private:
	sf::RectangleShape box;
	sf::Text shownText;
	std::string text;
	std::string placeholder;
	std::size_t maxLength = 30;
	bool active = false;
};
