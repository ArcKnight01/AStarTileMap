#include "Dropdown.h"

void Dropdown::setup(const sf::Font& font, sf::Vector2f center, sf::Vector2f size, const std::vector<std::string>& options, int selected) {
	this->font = &font;
	this->size = size;
	this->options = options;
	this->selected = selected;
	topLeft = sf::Vector2f(center.x - size.x / 2, center.y - size.y / 2);
}

//option 0 sits directly under the closed box, option 1 under that, and so on
sf::FloatRect Dropdown::getOptionBounds(int index) const {
	return sf::FloatRect(topLeft.x, topLeft.y + size.y * (index + 1), size.x, size.y);
}

bool Dropdown::handleClick(sf::Vector2f mouse) {
	sf::FloatRect closedBox(topLeft, size);
	if (open) {
		for (int i = 0; i < (int)options.size(); i++) {
			if (getOptionBounds(i).contains(mouse)) {
				selected = i;
				open = false;
				return true;
			}
		}
		open = false;                        //clicked somewhere else: close the list
		return closedBox.contains(mouse);    //a click on the box itself only closes it
	}
	if (closedBox.contains(mouse)) {
		open = true;
		return true;
	}
	return false;
}

void Dropdown::draw(sf::RenderWindow& window, sf::Vector2f mouse) {
	unsigned int characterSize = (unsigned int)(size.y * 0.5f);

	//the closed box: the selected option and an arrow
	sf::RectangleShape box(size);
	box.setPosition(topLeft);
	box.setFillColor(sf::Color(20, 20, 40));
	box.setOutlineColor(open ? sf::Color::Cyan : sf::Color::White);
	box.setOutlineThickness(3);
	window.draw(box);

	sf::Text text(getSelectedText(), *font, characterSize);
	text.setPosition(topLeft.x + 12, topLeft.y + size.y * 0.17f);
	window.draw(text);

	sf::ConvexShape arrow(3); //points down when closed, up when open
	float arrowX = topLeft.x + size.x - 30, arrowY = topLeft.y + size.y / 2;
	if (open) {
		arrow.setPoint(0, sf::Vector2f(arrowX - 8, arrowY + 5));
		arrow.setPoint(1, sf::Vector2f(arrowX + 8, arrowY + 5));
		arrow.setPoint(2, sf::Vector2f(arrowX, arrowY - 6));
	}
	else {
		arrow.setPoint(0, sf::Vector2f(arrowX - 8, arrowY - 5));
		arrow.setPoint(1, sf::Vector2f(arrowX + 8, arrowY - 5));
		arrow.setPoint(2, sf::Vector2f(arrowX, arrowY + 6));
	}
	window.draw(arrow);

	if (!open) {
		return;
	}
	//the open list
	for (int i = 0; i < (int)options.size(); i++) {
		sf::FloatRect bounds = getOptionBounds(i);
		sf::RectangleShape row(sf::Vector2f(bounds.width, bounds.height));
		row.setPosition(bounds.left, bounds.top);
		bool hovered = bounds.contains(mouse);
		row.setFillColor(hovered ? sf::Color(70, 70, 140) : (i == selected ? sf::Color(45, 45, 95) : sf::Color(30, 30, 60)));
		row.setOutlineColor(sf::Color(180, 180, 180));
		row.setOutlineThickness(1);
		window.draw(row);

		sf::Text optionText(options[i], *font, characterSize);
		optionText.setPosition(bounds.left + 12, bounds.top + size.y * 0.17f);
		window.draw(optionText);
	}
}

int Dropdown::getSelectedIndex() const {
	return selected;
}

void Dropdown::setSelectedIndex(int index) {
	if (index >= 0 && index < (int)options.size()) selected = index;
}

void Dropdown::setCenter(sf::Vector2f center) {


	// pseudo-code:
	// topLeft = center - size / 2


	topLeft = sf::Vector2f(center.x - size.x / 2, center.y - size.y / 2);
}

std::string Dropdown::getSelectedText() const {
	return options.empty() ? "" : options[selected];
}

bool Dropdown::isOpen() const {
	return open;
}
