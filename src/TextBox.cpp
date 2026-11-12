#include "TextBox.h"

void TextBox::setup(const sf::Font& font, sf::Vector2f center, sf::Vector2f size, const std::string& placeholder, std::size_t maxLength) {
	this->placeholder = placeholder;
	this->maxLength = maxLength;

	box.setSize(size);
	box.setOrigin(size.x / 2, size.y / 2);
	box.setPosition(center);
	box.setFillColor(sf::Color(20, 20, 40));
	box.setOutlineThickness(3);

	shownText.setFont(font);
	shownText.setCharacterSize((unsigned int)(size.y * 0.5f));
	//left-aligned inside the box, vertically centred
	shownText.setPosition(center.x - size.x / 2 + 12, center.y - size.y * 0.33f);
}

void TextBox::handleEvent(const sf::Event& event, sf::Vector2f mouse) {
	if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
		active = box.getGlobalBounds().contains(mouse);
	}
	if (active && event.type == sf::Event::TextEntered) {
		sf::Uint32 character = event.text.unicode;
		if (character == 8) { //Backspace
			if (!text.empty()) {
				text.pop_back();
			}
		}
		else if (character >= 32 && character < 127 && text.size() < maxLength) { //printable ASCII
			text += (char)character;
		}
	}
}

void TextBox::draw(sf::RenderWindow& window) {
	box.setOutlineColor(active ? sf::Color::Cyan : sf::Color::White);
	if (text.empty() && !active) {
		shownText.setString(placeholder);
		shownText.setFillColor(sf::Color(130, 130, 130));
	}
	else {
		shownText.setString(text + (active ? "_" : "")); //"_" marks where typing goes
		shownText.setFillColor(sf::Color::White);
	}
	window.draw(box);
	window.draw(shownText);
}

std::string TextBox::getText() const {
	return text;
}

void TextBox::setText(const std::string& newText) {
	text = newText.substr(0, maxLength);
}

bool TextBox::isActive() const {
	return active;
}

void TextBox::setActive(bool isNowActive) {
	active = isNowActive;
}
