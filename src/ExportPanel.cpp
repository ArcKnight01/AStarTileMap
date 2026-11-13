#include "ExportPanel.h"

// layout of the panel, in pixels
static const float panelPadding  = 20;  //space between the panel's edge and its contents
static const float titleHeight   = 44;
static const float rowHeight     = 54;  //name line + detail line
static const float statusHeight  = 34;
static const float closeHeight   = 46;
static const sf::Vector2f saveButtonSize(108, 34);
static const sf::Vector2f copyButtonSize(76, 34);

void ExportPanel::setup(const sf::Font& font, const std::vector<std::string>& names, const std::vector<std::string>& details) {
	this->font    = &font;
	this->names   = names;
	this->details = details;
	statusText.setFont(font);
	statusText.setCharacterSize(15);
	statusText.setFillColor(sf::Color(150, 220, 150));
}

void ExportPanel::open(sf::Vector2f center, float width) {


	// pseudo-code:
	// height = padding + title + rows * rowHeight + status + Close + padding
	// row k: name / detail on the left, [Save file] [Copy] on the right, at top + title + k * rowHeight


	showing = true;
	statusText.setString("Save file: writes exports/<map>_<what>.yaml.  Copy: puts it on the clipboard.");
	int rows     = (int)names.size();
	float height = panelPadding + titleHeight + rows * rowHeight + statusHeight + closeHeight + panelPadding;
	sf::Vector2f topLeft(center.x - width / 2, center.y - height / 2);
	box.setSize(sf::Vector2f(width, height));
	box.setPosition(topLeft);
	box.setFillColor(sf::Color(25, 25, 55));
	box.setOutlineColor(sf::Color::Cyan);
	box.setOutlineThickness(3);


	float right = topLeft.x + width - panelPadding;
	saveButtons.assign(rows, Button());
	copyButtons.assign(rows, Button());
	for (int k = 0; k < rows; k++) {
		float rowCenterY = topLeft.y + panelPadding + titleHeight + k * rowHeight + rowHeight / 2;
		copyButtons[k].setup(*font, "Copy", sf::Vector2f(right - copyButtonSize.x / 2, rowCenterY), copyButtonSize, 16);
		saveButtons[k].setup(*font, "Save file", sf::Vector2f(right - copyButtonSize.x - 10 - saveButtonSize.x / 2, rowCenterY), saveButtonSize, 16);
	}


	float statusTop = topLeft.y + panelPadding + titleHeight + rows * rowHeight;
	statusText.setPosition(topLeft.x + panelPadding, statusTop + 8);
	closeButton.setup(*font, "Close", sf::Vector2f(center.x, statusTop + statusHeight + closeHeight / 2), sf::Vector2f(160, closeHeight), 20);
}

void ExportPanel::close() {
	showing = false;
}

bool ExportPanel::isOpen() const {
	return showing;
}

void ExportPanel::setStatus(const std::string& text) {
	statusText.setString(text);
}

ExportChoice ExportPanel::handleEvent(const sf::Event& event, sf::Vector2f mouse) {


	// pseudo-code:
	// Esc, or a click on Close -> close;  a click on row k's Save file / Copy -> { k, copy }


	ExportChoice choice;
	if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
		close();
		return choice;
	}
	if (!(event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left)) {
		return choice;
	}
	if (closeButton.contains(mouse)) {
		close();
		return choice;
	}


	for (int k = 0; k < (int)names.size(); k++) {
		if (saveButtons[k].contains(mouse)) { choice.row = k; choice.copy = false; }
		if (copyButtons[k].contains(mouse)) { choice.row = k; choice.copy = true; }
	}


	return choice;
}

void ExportPanel::draw(sf::RenderWindow& window, sf::Vector2f mouse) {


	// pseudo-code:
	// dim the whole view;  box;  title;  per row: name, detail (grey), buttons;  status;  Close


	if (!showing) return;
	sf::Vector2f viewSize = window.getView().getSize();
	sf::RectangleShape dim(viewSize);
	dim.setPosition(window.getView().getCenter() - viewSize / 2.f);
	dim.setFillColor(sf::Color(0, 0, 0, 150));
	window.draw(dim);
	window.draw(box);


	sf::Vector2f topLeft = box.getPosition();
	sf::Text title("Export for ROS 2", *font, 24);
	title.setPosition(topLeft.x + panelPadding, topLeft.y + panelPadding);
	window.draw(title);
	for (int k = 0; k < (int)names.size(); k++) {
		float rowTop = topLeft.y + panelPadding + titleHeight + k * rowHeight;
		sf::Text name(names[k], *font, 18);
		name.setPosition(topLeft.x + panelPadding, rowTop + 4);
		window.draw(name);
		sf::Text detail(details[k], *font, 13);
		detail.setFillColor(sf::Color(170, 170, 170));
		detail.setPosition(topLeft.x + panelPadding + 12, rowTop + 30);
		window.draw(detail);
		saveButtons[k].draw(window, mouse);
		copyButtons[k].draw(window, mouse);
	}


	window.draw(statusText);
	closeButton.draw(window, mouse);
}
