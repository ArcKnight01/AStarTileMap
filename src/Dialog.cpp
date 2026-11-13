#include "Dialog.h"
#include <algorithm>

// layout of the dialog, in pixels
static const float dialogWidth   = 480;
static const float dialogPadding = 24;  //space between the box's edge and its contents, and between rows
static const float buttonHeight  = 46;
static const float buttonGap     = 12;  //space between two buttons
static const float textBoxHeight = 44;

void Dialog::setup(const sf::Font& font) {
	this->font = &font;
	messageText.setFont(font);
	messageText.setCharacterSize(20);
	messageText.setFillColor(sf::Color::White);
}

void Dialog::open(const std::string& message, const std::vector<std::string>& buttonLabels, sf::Vector2f center,
	bool hasTextInput, const std::string& text) {


	// pseudo-code:
	// height = padding + message height + ( text box height + padding ) + button height + padding
	// buttons share the width: width_b = ( dialogWidth - 2 padding - ( n - 1 ) gap ) / n, laid out left to right


	this->hasTextInput = hasTextInput;
	showing = true;
	messageText.setString(message);
	float messageHeight = messageText.getLocalBounds().top + messageText.getLocalBounds().height;
	float height        = dialogPadding + messageHeight + dialogPadding + (hasTextInput ? textBoxHeight + dialogPadding : 0) + buttonHeight + dialogPadding;
	sf::Vector2f topLeft(center.x - dialogWidth / 2, center.y - height / 2);


	box.setSize(sf::Vector2f(dialogWidth, height));
	box.setPosition(topLeft);
	box.setFillColor(sf::Color(25, 25, 55));
	box.setOutlineColor(sf::Color::Cyan);
	box.setOutlineThickness(3);
	messageText.setPosition(topLeft.x + dialogPadding, topLeft.y + dialogPadding);


	float rowTop = topLeft.y + dialogPadding + messageHeight + dialogPadding;
	if (hasTextInput) {
		textBox.setup(*font, sf::Vector2f(center.x, rowTop + textBoxHeight / 2), sf::Vector2f(dialogWidth - 2 * dialogPadding, textBoxHeight), "", 40);
		textBox.setText(text);
		textBox.setActive(true); //ready to type
		rowTop += textBoxHeight + dialogPadding;
	}


	int count = (int)buttonLabels.size();
	float buttonWidth = (dialogWidth - 2 * dialogPadding - (count - 1) * buttonGap) / count;
	buttons.assign(count, Button());
	for (int i = 0; i < count; i++) {
		float buttonCenterX = topLeft.x + dialogPadding + i * (buttonWidth + buttonGap) + buttonWidth / 2;
		buttons[i].setup(*font, buttonLabels[i], sf::Vector2f(buttonCenterX, rowTop + buttonHeight / 2), sf::Vector2f(buttonWidth, buttonHeight), 20);
	}
}

void Dialog::close() {
	showing = false;
}

bool Dialog::isOpen() const {
	return showing;
}

void Dialog::setMessage(const std::string& message) {
	messageText.setString(message);
}

int Dialog::handleEvent(const sf::Event& event, sf::Vector2f mouse) {


	// pseudo-code:
	// Enter -> 0 (default);  Esc -> last (cancel);  a click on button i -> i;  anything else -> the text box, -1


	if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Enter) return 0;
	if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) return (int)buttons.size() - 1;
	if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
		for (int i = 0; i < (int)buttons.size(); i++) {
			if (buttons[i].contains(mouse)) return i;
		}
	}
	if (hasTextInput) {
		textBox.handleEvent(event, mouse);
	}


	return -1;
}

std::string Dialog::getText() const {
	return hasTextInput ? textBox.getText() : "";
}

void Dialog::draw(sf::RenderWindow& window, sf::Vector2f mouse) {


	// pseudo-code:
	// a see-through dark rectangle over the whole view, then the box, message, text box and buttons


	if (!showing) return;
	sf::Vector2f viewSize = window.getView().getSize();
	sf::RectangleShape dim(viewSize);
	dim.setPosition(window.getView().getCenter() - viewSize / 2.f);
	dim.setFillColor(sf::Color(0, 0, 0, 150));
	window.draw(dim);


	window.draw(box);
	window.draw(messageText);
	if (hasTextInput) textBox.draw(window);
	for (Button& button : buttons) button.draw(window, mouse);
}
