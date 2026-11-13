#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "Button.h"
#include "TextBox.h"

// A modal dialog: a message, an optional one-line text box, and a row of buttons, drawn over a dimmed screen. While it
// is open it takes every click and key, so nothing behind it reacts. Enter picks the first button (the default) and
// Esc the last one (the cancel button), so put the default first and Cancel last.
//
//   +----------------------------------------------+
//   |  Save changes to 'rugged'?                    |
//   |  [ name text box ]            (if asked for)  |
//   |  [  Save  ]   [ Don't save ]   [  Cancel  ]   |
//   +----------------------------------------------+
class Dialog {
public:
	/**
	 * Set the font of the message, the text box and the buttons.
	 *
	 * @param font  Font (kept by pointer, so it must outlive the dialog).
	 */
	void setup(const sf::Font& font);
	/**
	 * Open the dialog with a message and buttons, sized to fit them, centred on a point.
	 *
	 * @param message       Text shown at the top (may contain "\n").
	 * @param buttonLabels  Buttons left to right: the first is the default (Enter), the last the cancel (Esc).
	 * @param center        Centre of the dialog, in window coordinates (pixels).
	 * @param hasTextInput  True to show a text box under the message (e.g. for a file name).
	 * @param text          Starting text of the text box.
	 */
	void open(const std::string& message, const std::vector<std::string>& buttonLabels, sf::Vector2f center,
		bool hasTextInput = false, const std::string& text = "");
	/**
	 * Close the dialog (it stops drawing and taking input).
	 */
	void close();
	/**
	 * Check whether the dialog is showing.
	 *
	 * @return  True while open.
	 */
	bool isOpen() const;
	/**
	 * Replace the message while the dialog stays open (e.g. to say why a name was refused).
	 *
	 * @param message  New text (one line, so the dialog keeps its size).
	 */
	void setMessage(const std::string& message);
	/**
	 * Give the dialog an event: typing goes to the text box, clicks and Enter / Esc pick a button.
	 *
	 * @param event  The event.
	 * @param mouse  Mouse position, in window coordinates (pixels).
	 * @return       Index of the button picked (Enter: 0, Esc: the last), or -1 if none was.
	 */
	int handleEvent(const sf::Event& event, sf::Vector2f mouse);
	/**
	 * Get the text typed in the text box.
	 *
	 * @return  The text (empty without a text box).
	 */
	std::string getText() const;
	/**
	 * Draw the dimmed screen, the dialog box, the message, the text box and the buttons.
	 *
	 * @param window  Window to draw into.
	 * @param mouse   Mouse position, in window coordinates (pixels), for the button highlight.
	 */
	void draw(sf::RenderWindow& window, sf::Vector2f mouse);

private:
	const sf::Font* font = nullptr;
	sf::RectangleShape box;
	sf::Text messageText;
	std::vector<Button> buttons;
	TextBox textBox;
	bool hasTextInput = false;
	bool showing = false;
};
