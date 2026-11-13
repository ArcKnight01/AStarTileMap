#include "Toggle.h"

void Toggle::setup(const sf::Font& font, const std::string& text, sf::Vector2f center, sf::Vector2f size, bool on,
	sf::Color onColor, unsigned int characterSize) {
	this->on = on;
	this->onColor = onColor;
	box.setSize(size);
	box.setOrigin(size.x / 2, size.y / 2);
	box.setOutlineColor(sf::Color::White);
	box.setOutlineThickness(2);

	label.setFont(font);
	label.setString(text);
	label.setCharacterSize(characterSize);
	label.setFillColor(sf::Color::White);
	setCenter(center);
}

void Toggle::setCenter(sf::Vector2f center) {


	// pseudo-code:
	// box centred on center;  label 10 px in from the box's left edge, centred vertically


	box.setPosition(center);
	sf::FloatRect bounds = label.getLocalBounds();
	label.setOrigin(bounds.left, bounds.top + bounds.height / 2);
	label.setPosition(center.x - box.getSize().x / 2 + 10, center.y);
}

bool Toggle::handleClick(sf::Vector2f mouse) {


	// pseudo-code:
	// on = not on   if the click is inside the box


	if (!contains(mouse)) {
		return false;
	}
	on = !on;


	return true;
}

bool Toggle::contains(sf::Vector2f point) const {
	return box.getGlobalBounds().contains(point);
}

bool Toggle::isOn() const {
	return on;
}

void Toggle::setOn(bool on) {
	this->on = on;
}

void Toggle::draw(sf::RenderWindow& window, sf::Vector2f mouse) {


	// pseudo-code:
	// box (lighter when hovered), label
	// track: h = box height - 2 * margin, a 2h x h rectangle (width : height = 2 : 1) at the right end of the box,
	//        onColor when on, grey when off
	// knob:  a square in one half of the track, the right half when on, the left half when off
	// (was: track 2 * box height wide and half the box height tall (4 : 1), knob half the track wide (not square))


	box.setFillColor(contains(mouse) ? sf::Color(70, 70, 140) : sf::Color(20, 20, 40)); //lighter when hovered, like Button
	window.draw(box);
	window.draw(label);


	//the switch: its track sits at the right end of the box, with a margin of a sixth of the box height (was a quarter)
	sf::Vector2f boxSize   = box.getSize();
	sf::Vector2f boxCenter = box.getPosition();
	float margin           = boxSize.y / 6;
	float trackHeight      = boxSize.y - 2 * margin;
	sf::Vector2f trackSize(2 * trackHeight, trackHeight);
	sf::Vector2f trackTopLeft(boxCenter.x + boxSize.x / 2 - margin - trackSize.x, boxCenter.y - trackSize.y / 2);
	sf::RectangleShape track(trackSize);
	track.setPosition(trackTopLeft);
	track.setFillColor(on ? onColor : sf::Color(80, 80, 80));
	track.setOutlineColor(sf::Color(200, 200, 200));
	track.setOutlineThickness(1);
	window.draw(track);


	//the knob: a square 2 px inside its half of the track (each half is trackHeight x trackHeight)
	float knobSide = trackHeight - 4;
	sf::RectangleShape knob(sf::Vector2f(knobSide, knobSide));
	knob.setPosition(trackTopLeft.x + (on ? trackHeight : 0) + 2, trackTopLeft.y + 2);
	knob.setFillColor(sf::Color::White);
	window.draw(knob);
}
