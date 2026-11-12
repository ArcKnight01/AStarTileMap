#include "TerrainGenerator.h"
#include <algorithm>
#include <cmath>
#include <random>

static const double PI = 3.14159265358979323846;

GridMap createFlatMap(int columns, int rows, double resolution) {
	GridMap map(columns, rows, resolution);
	map.addLayer("elevation", 0.f);
	return map;
}

void addDunes(GridMap& map, double amplitude, double wavelengthX, double wavelengthY, double phase) {
	std::vector<float>& elevation = map.getLayer("elevation");
	for (int row = 0; row < map.getRows(); row++) {
		for (int column = 0; column < map.getColumns(); column++) {
			double x, y;
			map.getPosition(column, row, x, y);
			elevation[map.getIndex(column, row)] += (float)(amplitude * std::sin(2 * PI * x / wavelengthX + phase) * std::cos(2 * PI * y / wavelengthY));
		}
	}
}

void addHill(GridMap& map, int centreColumn, int centreRow, double height, double radius) {
	std::vector<float>& elevation = map.getLayer("elevation");
	double cx, cy;
	map.getPosition(centreColumn, centreRow, cx, cy);
	for (int row = 0; row < map.getRows(); row++) {
		for (int column = 0; column < map.getColumns(); column++) {
			double x, y;
			map.getPosition(column, row, x, y);
			double distanceSquared = (x - cx) * (x - cx) + (y - cy) * (y - cy);
			elevation[map.getIndex(column, row)] += (float)(height * std::exp(-distanceSquared / (2 * radius * radius)));
		}
	}
}

void addNoiseInRectangle(GridMap& map, int column0, int row0, int column1, int row1, double amplitude, unsigned int seed) {
	std::vector<float>& elevation = map.getLayer("elevation");
	std::mt19937 random(seed);
	std::uniform_real_distribution<double> bump(-amplitude, amplitude);
	for (int row = std::max(0, row0); row <= std::min(map.getRows() - 1, row1); row++) {
		for (int column = std::max(0, column0); column <= std::min(map.getColumns() - 1, column1); column++) {
			elevation[map.getIndex(column, row)] += (float)bump(random);
		}
	}
}

void addNoise(GridMap& map, double amplitude, unsigned int seed) {
	addNoiseInRectangle(map, 0, 0, map.getColumns() - 1, map.getRows() - 1, amplitude, seed);
}

void addBlock(GridMap& map, int column0, int row0, int column1, int row1, double height) {
	std::vector<float>& elevation = map.getLayer("elevation");
	for (int row = std::max(0, row0); row <= std::min(map.getRows() - 1, row1); row++) {
		for (int column = std::max(0, column0); column <= std::min(map.getColumns() - 1, column1); column++) {
			float& z = elevation[map.getIndex(column, row)];
			z = std::max(z, (float)height);
		}
	}
}
