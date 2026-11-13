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
	/**
	 * Create an empty map (0 x 0 cells, 0.1 m resolution, no layers).
	 */
	GridMap();
	/**
	 * Create a map with no layers.
	 *
	 * @param columns     Number of columns (x direction).
	 * @param rows        Number of rows (y direction).
	 * @param resolution  Side length of a cell, in metres.
	 */
	GridMap(int columns, int rows, double resolution);

	/**
	 * Get the number of columns.
	 *
	 * @return  Number of columns.
	 */
	int getColumns() const;
	/**
	 * Get the number of rows.
	 *
	 * @return  Number of rows.
	 */
	int getRows() const;
	/**
	 * Get the number of cells.
	 *
	 * @return  columns * rows.
	 */
	int getCellCount() const;
	/**
	 * Get the side length of a cell.
	 *
	 * @return  Resolution in metres per cell.
	 */
	double getResolution() const; //metres per cell

	/**
	 * Check whether a cell lies on the map.
	 *
	 * @param column  Column of the cell (0 = smallest x).
	 * @param row     Row of the cell (0 = top row, largest y).
	 * @return        True if 0 <= column < columns and 0 <= row < rows.
	 */
	bool isInside(int column, int row) const;
	/**
	 * Get the index of a cell.
	 *
	 * @param column  Column of the cell (0 = smallest x).
	 * @param row     Row of the cell (0 = top row, largest y).
	 * @return        row * columns + column.
	 */
	int getIndex(int column, int row) const;
	/**
	 * Get the column of a cell index.
	 *
	 * @param index  Cell index (row * columns + column).
	 * @return       index % columns.
	 */
	int getColumn(int index) const;
	/**
	 * Get the row of a cell index.
	 *
	 * @param index  Cell index (row * columns + column).
	 * @return       index / columns.
	 */
	int getRow(int index) const;

	/**
	 * World position (metres) of a cell's centre.
	 *
	 * @param column  Column of the cell (0 = smallest x).
	 * @param row     Row of the cell (0 = top row, largest y).
	 * @param x       Set to x in metres (map centred on (0, 0), x grows to the right).
	 * @param y       Set to y in metres (y grows upward, toward row 0).
	 */
	void getPosition(int column, int row, double& x, double& y) const;
	/**
	 * The cell containing a world position; false if it is outside the map.
	 *
	 * @param x       World x in metres (map centred on (0, 0)).
	 * @param y       World y in metres.
	 * @param column  Set to the column of the cell.
	 * @param row     Set to the row of the cell.
	 * @return        False if the position is outside the map.
	 */
	bool getCell(double x, double y, int& column, int& row) const;

	//layers
	/**
	 * Add a layer with every cell set to a value.
	 *
	 * @param name   Name of the layer.
	 * @param value  Initial value of every cell (NaN = unknown).
	 */
	void addLayer(const std::string& name, float value = 0.f); //adds it, or resets it to value if it exists
	/**
	 * Check whether a layer exists.
	 *
	 * @param name  Name of the layer.
	 * @return      True if the map has the layer.
	 */
	bool hasLayer(const std::string& name) const;
	/**
	 * Get a layer's values; throws std::out_of_range if it does not exist.
	 *
	 * @param name  Name of the layer.
	 * @return      The values, indexed by row * columns + column.
	 */
	std::vector<float>& getLayer(const std::string& name);
	/**
	 * Get a layer's values, read-only; throws std::out_of_range if it does not exist.
	 *
	 * @param name  Name of the layer.
	 * @return      The values, indexed by row * columns + column.
	 */
	const std::vector<float>& getLayer(const std::string& name) const;
	/**
	 * Get the names of all layers.
	 *
	 * @return  The names, in alphabetical order.
	 */
	std::vector<std::string> getLayerNames() const;
	/**
	 * Get one cell of a layer for writing.
	 *
	 * @param name    Name of the layer.
	 * @param column  Column of the cell (0 = smallest x).
	 * @param row     Row of the cell (0 = top row, largest y).
	 * @return        Reference to the cell's value.
	 */
	float& at(const std::string& name, int column, int row);
	/**
	 * Get the value of one cell of a layer.
	 *
	 * @param name    Name of the layer.
	 * @param column  Column of the cell (0 = smallest x).
	 * @param row     Row of the cell (0 = top row, largest y).
	 * @return        The cell's value (NaN = unknown).
	 */
	float at(const std::string& name, int column, int row) const;

	/**
	 * Get the value that marks an unknown cell.
	 *
	 * @return  Quiet NaN.
	 */
	static float getUnknownValue() { return std::numeric_limits<float>::quiet_NaN(); }
	/**
	 * Check whether a value is unknown.
	 *
	 * @param value  Cell value.
	 * @return       True if the value is NaN.
	 */
	static bool isUnknown(float value) { return std::isnan(value); }

private:
	int columns = 0;
	int rows = 0;
	double resolution = 0.1;
	std::map<std::string, std::vector<float>> layers;
};
