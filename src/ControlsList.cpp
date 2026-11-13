#include "ControlsList.h"
#include <cmath>

// space after each item, in pixels
static const float itemGap = 5;

void ControlsList::setup(const sf::Font& font, unsigned int characterSize, float lineHeight) {
	this->font          = &font;
	this->characterSize = characterSize;
	this->lineHeight    = lineHeight;
	const std::map<ControlIcon, std::string> files = {
		{ ControlIcon::Key,        "key.png" },
		{ ControlIcon::WideKey,    "key_wide.png" },
		{ ControlIcon::ArrowUp,    "key_up.png" },
		{ ControlIcon::ArrowDown,  "key_down.png" },
		{ ControlIcon::ArrowLeft,  "key_left.png" },
		{ ControlIcon::ArrowRight, "key_right.png" },
		{ ControlIcon::MouseLeft,  "mouse_left.png" },
		{ ControlIcon::MouseRight, "mouse_right.png" }
	};
	for (const std::pair<const ControlIcon, std::string>& file : files) {
		textures[file.first].loadFromFile("Textures/controls/" + file.second);
	}
}

void ControlsList::draw(sf::RenderTarget& window, sf::Vector2f topLeft, const std::vector<std::vector<ControlItem>>& rows) {


	// pseudo-code:
	// for row k: x = left edge;  for each item: draw it at x, vertically centred on the row;  x += its width + gap
	//   text: the label;  sprite: at whole pixels (so the pixel art stays sharp), with a key's label centred on its face


	for (int k = 0; k < (int)rows.size(); k++) {
		float rowTop  = topLeft.y + k * lineHeight;
		float x       = topLeft.x;
		for (const ControlItem& item : rows[k]) {
			if (item.icon == ControlIcon::Text) {
				sf::Text text(item.label, *font, characterSize);
				text.setPosition(std::round(x), std::round(rowTop + (lineHeight - characterSize) / 2 - 3));
				window.draw(text);
				x += text.getLocalBounds().left + text.getLocalBounds().width + itemGap;
				continue;
			}


			const sf::Texture& texture = textures[item.icon];
			sf::Sprite sprite(texture);
			float spriteTop = std::round(rowTop + (lineHeight - texture.getSize().y) / 2);
			sprite.setPosition(std::round(x), spriteTop);
			window.draw(sprite);


			//a key's label, centred on the keycap's face (the face is the top 15 rows; the front lip is below it)
			if ((item.icon == ControlIcon::Key || item.icon == ControlIcon::WideKey) && !item.label.empty()) {
				sf::Text label(item.label, *font, item.icon == ControlIcon::Key ? 12 : 11);
				label.setFillColor(sf::Color(45, 45, 60));
				sf::FloatRect bounds = label.getLocalBounds();
				label.setOrigin(std::round(bounds.left + bounds.width / 2), std::round(bounds.top + bounds.height / 2));
				label.setPosition(std::round(x + texture.getSize().x / 2.f), spriteTop + 8);
				window.draw(label);
			}
			x += texture.getSize().x + itemGap;
		}
	}
}
