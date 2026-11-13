#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "Button.h"

// The EXPORT panel: a modal list of things to export, one row each with a [Save file] and a [Copy] button, a status
// line for the last result, and [Close]. It only reports which button was clicked; main() does the exporting (with
// core/RosExport.h), so the panel knows nothing about ROS. Esc or Close closes it.
//
//   +--------------------------------------------------------------+
//   |  Export for ROS 2                                            |
//   |  Cost map                             [Save file]  [Copy]    |
//   |    grid_map_msgs/msg/GridMap -> /cost_map                    |
//   |  ...                                                         |
//   |  Saved exports/rugged_cost_map.yaml                          |
//   |                         [ Close ]                            |
//   +--------------------------------------------------------------+

// What a click in the panel asked for.
struct ExportChoice {
	int row = -1;      //the row whose button was clicked, -1 for none
	bool copy = false; //true: Copy; false: Save file
};

class ExportPanel {
public:
	/**
	 * Set the font and the rows: each row's name and the line of detail under it.
	 *
	 * @param font     Font (kept by pointer, so it must outlive the panel).
	 * @param names    Name of each row, top to bottom.
	 * @param details  Detail line under each name (e.g. the message type and topic), same order.
	 */
	void setup(const sf::Font& font, const std::vector<std::string>& names, const std::vector<std::string>& details);
	/**
	 * Open the panel centred on a point, with the status line cleared.
	 *
	 * @param center  Centre of the panel, in window coordinates (pixels).
	 * @param width   Width of the panel, in pixels.
	 */
	void open(sf::Vector2f center, float width);
	/**
	 * Close the panel (it stops drawing and taking input).
	 */
	void close();
	/**
	 * Check whether the panel is showing.
	 *
	 * @return  True while open.
	 */
	bool isOpen() const;
	/**
	 * Show the result of the last export on the status line.
	 *
	 * @param text  One line, e.g. "Saved exports/rugged_path.yaml".
	 */
	void setStatus(const std::string& text);
	/**
	 * Give the panel an event: a click on a row's button is reported; Close or Esc closes the panel.
	 *
	 * @param event  The event.
	 * @param mouse  Mouse position, in window coordinates (pixels).
	 * @return       The row and button clicked (row -1 if none).
	 */
	ExportChoice handleEvent(const sf::Event& event, sf::Vector2f mouse);
	/**
	 * Draw the dimmed screen, the panel, its rows, the status line and Close.
	 *
	 * @param window  Window to draw into.
	 * @param mouse   Mouse position, in window coordinates (pixels), for the button highlights.
	 */
	void draw(sf::RenderWindow& window, sf::Vector2f mouse);

private:
	const sf::Font* font = nullptr;
	std::vector<std::string> names;
	std::vector<std::string> details;
	std::vector<Button> saveButtons;
	std::vector<Button> copyButtons;
	Button closeButton;
	sf::RectangleShape box;
	sf::Text statusText;
	bool showing = false;
};
