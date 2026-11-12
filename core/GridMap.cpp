#include "GridMap.h"
#include <stdexcept>

GridMap::GridMap() : GridMap(0, 0, 0.1) {
}

GridMap::GridMap(int columns, int rows, double resolution) {
	this->columns = columns;
	this->rows = rows;
	this->resolution = resolution;
}

int GridMap::getColumns() const { return columns; }
int GridMap::getRows() const { return rows; }
int GridMap::getCellCount() const { return columns * rows; }
double GridMap::getResolution() const { return resolution; }

bool GridMap::isInside(int column, int row) const {
	return column >= 0 && column < columns && row >= 0 && row < rows;
}

int GridMap::getIndex(int column, int row) const {
	return row * columns + column;
}

int GridMap::getColumn(int index) const {
	return index % columns;
}

int GridMap::getRow(int index) const {
	return index / columns;
}

void GridMap::getPosition(int column, int row, double& x, double& y) const {
	//centre of the map is (0, 0); x to the right, y upward
	x = (column - (columns - 1) / 2.0) * resolution;
	y = ((rows - 1) / 2.0 - row) * resolution;
}

bool GridMap::getCell(double x, double y, int& column, int& row) const {
	column = (int)std::floor(x / resolution + columns / 2.0);
	row = (int)std::floor(rows / 2.0 - y / resolution);
	return isInside(column, row);
}

void GridMap::addLayer(const std::string& name, float value) {
	layers[name] = std::vector<float>(getCellCount(), value);
}

bool GridMap::hasLayer(const std::string& name) const {
	return layers.count(name) > 0;
}

std::vector<float>& GridMap::getLayer(const std::string& name) {
	auto found = layers.find(name);
	if (found == layers.end()) {
		throw std::out_of_range("GridMap has no layer \"" + name + "\"");
	}
	return found->second;
}

const std::vector<float>& GridMap::getLayer(const std::string& name) const {
	auto found = layers.find(name);
	if (found == layers.end()) {
		throw std::out_of_range("GridMap has no layer \"" + name + "\"");
	}
	return found->second;
}

std::vector<std::string> GridMap::getLayerNames() const {
	std::vector<std::string> names;
	for (const auto& layer : layers) {
		names.push_back(layer.first);
	}
	return names;
}

float& GridMap::at(const std::string& name, int column, int row) {
	return getLayer(name)[getIndex(column, row)];
}

float GridMap::at(const std::string& name, int column, int row) const {
	return getLayer(name)[getIndex(column, row)];
}
