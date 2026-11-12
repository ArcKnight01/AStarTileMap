#pragma once
#include <cmath>
#include <limits>
#include <map>
#include <string>
#include <vector>

// A 2D grid of square cells with named float layers, in the style of grid_map (Fankhauser & Hutter).
// Every stage of the pipeline reads some layers and writes a new one:
//   "elevation" -> "slope", "roughness", "step" -> "traversability" -> "cost"  -> A*
//
// Cells are addressed by (column, row) or by index = row * columns + column (row 0 is the top row on screen).
// World positions are in metres with the map centred on (0, 0), like grid_map (Rajput eqs. 3.2-3.4):
//   x grows with the column (right), y grows upward (toward row 0).
// Unknown values are NaN (for example holes in an elevation map).
//
// Plain C++ only (no SFML, no ROS), so this folder can be moved into a ROS 2 node.
class GridMap {
public:
	GridMap();
	GridMap(int columns, int rows, double resolution);

	int getColumns() const;
	int getRows() const;
	int getCellCount() const;
	double getResolution() const; //metres per cell

	bool isInside(int column, int row) const;
	int getIndex(int column, int row) const;
	int getColumn(int index) const;
	int getRow(int index) const;

	//world position (metres) of a cell's centre
	void getPosition(int column, int row, double& x, double& y) const;
	//the cell containing a world position; false if it is outside the map
	bool getCell(double x, double y, int& column, int& row) const;

	//layers
	void addLayer(const std::string& name, float value = 0.f); //adds it, or resets it to value if it exists
	bool hasLayer(const std::string& name) const;
	std::vector<float>& getLayer(const std::string& name);
	const std::vector<float>& getLayer(const std::string& name) const;
	std::vector<std::string> getLayerNames() const;
	float& at(const std::string& name, int column, int row);
	float at(const std::string& name, int column, int row) const;

	static float getUnknownValue() { return std::numeric_limits<float>::quiet_NaN(); }
	static bool isUnknown(float value) { return std::isnan(value); }

private:
	int columns = 0;
	int rows = 0;
	double resolution = 0.1;
	std::map<std::string, std::vector<float>> layers;
};
