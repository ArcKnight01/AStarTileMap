#include "Button.h"

void Button::setup(const sf::Font& font, const std::string& text, sf::Vector2f center, sf::Vector2f size, unsigned int characterSize) {
	box.setSize(size);
	box.setOrigin(size.x / 2, size.y / 2);
	box.setPosition(center);
	box.setOutlineColor(sf::Color::White);
	box.setOutlineThickness(3);

	label.setFont(font);
	label.setString(text);
	label.setCharacterSize(characterSize);
	label.setFillColor(sf::Color::White);
	sf::FloatRect bounds = label.getLocalBounds();
	label.setOrigin(bounds.left + bounds.width / 2, bounds.top + bounds.height / 2);
	label.setPosition(center);
}

bool Button::contains(sf::Vector2f point) const {
	return box.getGlobalBounds().contains(point);
}

void Button::draw(sf::RenderTarget& window, sf::Vector2f mouse) {
	box.setFillColor(contains(mouse) ? sf::Color(70, 70, 140) : sf::Color(35, 35, 80)); //lighter when hovered
	window.draw(box);
	window.draw(label);
}
