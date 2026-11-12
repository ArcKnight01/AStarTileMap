#include "GridMapFile.h"
#include <fstream>
#include <iomanip>
#include <sstream>

bool saveGridMap(const std::string& filename, const GridMap& map, int startIndex, int endIndex) {
	std::ofstream file(filename);
	if (!file.is_open()) {
		return false;
	}
	auto writeCell = [&](const char* label, int index) {
		if (index < 0) {
			file << label << " -1 -1\n";
		}
		else {
			file << label << " " << map.getColumn(index) << " " << map.getRow(index) << "\n";
		}
	};
	file << "gridmap 1\n";
	file << "size " << map.getColumns() << " " << map.getRows() << "\n";
	file << "resolution " << map.getResolution() << "\n";
	writeCell("start", startIndex);
	writeCell("end", endIndex);
	for (const std::string& name : map.getLayerNames()) {
		file << "layer " << name << "\n";
		const std::vector<float>& values = map.getLayer(name);
		for (int row = 0; row < map.getRows(); row++) {
			for (int column = 0; column < map.getColumns(); column++) {
				float value = values[map.getIndex(column, row)];
				if (column > 0) file << " ";
				if (GridMap::isUnknown(value)) file << "nan";
				else file << std::setprecision(5) << value;
			}
			file << "\n";
		}
	}
	return true;
}

bool loadGridMap(const std::string& filename, GridMap& map, int& startIndex, int& endIndex) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		return false;
	}
	std::string word;
	int version = 0;
	int columns = 0, rows = 0;
	double resolution = 0.1;
	int startColumn = -1, startRow = -1, endColumn = -1, endRow = -1;
	if (!(file >> word >> version) || word != "gridmap") return false;
	if (!(file >> word >> columns >> rows) || word != "size" || columns <= 0 || rows <= 0) return false;
	if (!(file >> word >> resolution) || word != "resolution") return false;
	if (!(file >> word >> startColumn >> startRow) || word != "start") return false;
	if (!(file >> word >> endColumn >> endRow) || word != "end") return false;

	map = GridMap(columns, rows, resolution);
	startIndex = map.isInside(startColumn, startRow) ? map.getIndex(startColumn, startRow) : -1;
	endIndex = map.isInside(endColumn, endRow) ? map.getIndex(endColumn, endRow) : -1;

	std::string name;
	while (file >> word >> name) {
		if (word != "layer") return false;
		map.addLayer(name);
		std::vector<float>& values = map.getLayer(name);
		for (int i = 0; i < map.getCellCount(); i++) {
			std::string number;
			if (!(file >> number)) return false;
			values[i] = (number == "nan") ? GridMap::getUnknownValue() : std::stof(number);
		}
	}
	return true;
}
