#include "GridMapFile.h"
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>

// The file is YAML, but only the part of YAML that `ros2 topic echo` prints is read: block-style mappings ("key: value",
// one per line, nested by indentation) and sequences in block ("- a") or flow ("[a, b, ...]") style. The reader walks
// the lines in order and looks for the next expected key, so the fields must come in the message's order.

// one line of the file without its comment: how far it is indented, and the text after the indentation
struct YamlLine {
	int indent;
	std::string text;
};

//the lines of a YAML file with comments and blank lines removed
static std::vector<YamlLine> readYamlLines(std::ifstream& file) {
	std::vector<YamlLine> lines;
	std::string line;
	while (std::getline(file, line)) {
		std::size_t comment = line.find(" #");
		if (comment != std::string::npos) line.resize(comment);
		if (!line.empty() && line[0] == '#') line.clear();
		while (!line.empty() && isspace((unsigned char)line.back())) line.pop_back();
		std::size_t first = line.find_first_not_of(' ');
		if (first == std::string::npos) continue;
		lines.push_back({ (int)first, line.substr(first) });
	}
	return lines;
}

//index of the next line at or after position whose text is "key: ..." (or "- key: ..." inside a sequence), -1 if none
static int findYamlKey(const std::vector<YamlLine>& lines, int position, const std::string& key) {
	for (int i = std::max(0, position); i < (int)lines.size(); i++) {
		std::string text = lines[i].text;
		if (text.rfind("- ", 0) == 0) text = text.substr(2);
		if (text.rfind(key + ":", 0) == 0) return i;
	}
	return -1;
}

//the value after "key:" on a line, without surrounding spaces or quotes
static std::string getYamlValue(const YamlLine& line) {
	std::string value = line.text.substr(line.text.find(':') + 1);
	std::size_t first = value.find_first_not_of(' ');
	value = (first == std::string::npos) ? "" : value.substr(first);
	if (value.size() >= 2 && (value[0] == '"' || value[0] == '\'') && value.back() == value[0]) {
		value = value.substr(1, value.size() - 2);
	}
	return value;
}

//the items of the sequence whose key is on line keyLine: a flow sequence "[a, b, ...]" (it may go on over several
//lines), or a block sequence of "- item" lines under it. position is moved past the sequence.
static std::vector<std::string> readYamlSequence(const std::vector<YamlLine>& lines, int keyLine, int& position) {
	std::vector<std::string> items;
	std::string flow = getYamlValue(lines[keyLine]);
	position = keyLine + 1;
	if (flow.empty()) {
		//block: "- item" lines at the key's indentation or deeper
		while (position < (int)lines.size() && lines[position].indent >= lines[keyLine].indent
			&& lines[position].text.rfind("- ", 0) == 0 && lines[position].text.find(':') == std::string::npos) {
			items.push_back(lines[position].text.substr(2));
			position++;
		}
		return items;
	}
	//flow: everything up to the closing bracket
	while (flow.find(']') == std::string::npos && position < (int)lines.size()) {
		flow += " " + lines[position].text;
		position++;
	}
	std::string inside = flow.substr(flow.find('[') + 1, flow.find(']') - flow.find('[') - 1);
	std::stringstream stream(inside);
	std::string item;
	while (std::getline(stream, item, ',')) {
		std::size_t first = item.find_first_not_of(' ');
		std::size_t last = item.find_last_not_of(' ');
		if (first != std::string::npos) items.push_back(item.substr(first, last - first + 1));
	}
	return items;
}

//a YAML float: ".nan" / "nan" -> NaN, ".inf" / "-.inf" -> infinity, otherwise the number
static float getYamlFloat(const std::string& text) {
	std::string lower = text;
	for (char& character : lower) character = (char)tolower((unsigned char)character);
	if (lower == ".nan" || lower == "nan") return GridMap::getUnknownValue();
	if (lower == ".inf" || lower == "+.inf" || lower == "inf") return std::numeric_limits<float>::infinity();
	if (lower == "-.inf" || lower == "-inf") return -std::numeric_limits<float>::infinity();
	return std::stof(text);
}

//a float as YAML reads it back as a float: the shortest text that gives back the same value, always with a "." (YAML
//reads "1e-05" and "7" as a string and an integer, which a float32[] field refuses), NaN as .nan
template <typename Number>
static std::string getYamlNumberText(Number value) {
	if (std::isnan(value)) return ".nan";
	if (std::isinf(value)) return value > 0 ? ".inf" : "-.inf";
	char buffer[64];
	std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), value);
	std::string text(buffer, result.ptr);
	if (text.find('.') == std::string::npos) {
		std::size_t exponent = text.find('e');
		text.insert(exponent == std::string::npos ? text.size() : exponent, ".0");
	}
	return text;
}

int getMessageIndex(const GridMap& map, int column, int row) {


	// pseudo-code:
	// i = columns - 1 - column      (grid_map's index i grows toward -x, from the largest x)
	// j = row                       (j grows toward -y, from the largest y = our row 0)
	// index = j * columns + i       (column-major: one matrix column of length columns after another)


	return row * map.getColumns() + (map.getColumns() - 1 - column);
}

std::string getEndpointsFilename(const std::string& filename) {


	// pseudo-code:
	// "<path>.gridmap" -> "<path>.endpoints.yaml"   (any other name: append ".endpoints.yaml")


	const std::string extension = ".gridmap";
	bool hasExtension = filename.size() > extension.size() && filename.compare(filename.size() - extension.size(), extension.size(), extension) == 0;
	std::string stem = hasExtension ? filename.substr(0, filename.size() - extension.size()) : filename;


	return stem + ".endpoints.yaml";
}

bool saveGridMapMessage(const std::string& filename, const GridMap& map, const std::string& frameId) {


	// pseudo-code:
	// header: stamp 0, frame_id
	// info:   resolution, length_x = columns * resolution, length_y = rows * resolution, pose = identity at (0, 0, 0)
	// layers: names;  basic_layers: []
	// data:   per layer, layout { column_index: rows, stride rows * columns ; row_index: columns, stride columns }
	//         and values data[ j * columns + i ] = layer( column = columns - 1 - i, row = j )
	// outer_start_index = inner_start_index = 0


	std::ofstream file(filename);
	if (!file.is_open()) {
		return false;
	}
	int columns = map.getColumns();
	int rows    = map.getRows();
	std::vector<std::string> names = map.getLayerNames();


	file << "# grid_map_msgs/msg/GridMap (ROS 2 Jazzy), " << columns << " x " << rows << " cells\n";
	file << "header:\n";
	file << "  stamp:\n";
	file << "    sec: 0\n";
	file << "    nanosec: 0\n";
	file << "  frame_id: " << frameId << "\n";
	file << "info:\n";
	file << "  resolution: " << getYamlNumberText(map.getResolution()) << "\n";
	file << "  length_x: " << getYamlNumberText(columns * map.getResolution()) << "\n";
	file << "  length_y: " << getYamlNumberText(rows * map.getResolution()) << "\n";
	file << "  pose:\n";
	file << "    position:\n";
	file << "      x: 0.0\n";
	file << "      y: 0.0\n";
	file << "      z: 0.0\n";
	file << "    orientation:\n";
	file << "      x: 0.0\n";
	file << "      y: 0.0\n";
	file << "      z: 0.0\n";
	file << "      w: 1.0\n";
	file << "layers:\n";
	for (const std::string& name : names) {
		file << "- " << name << "\n";
	}
	file << "basic_layers: []\n";


	//one Float32MultiArray per layer; each line of data is one of grid_map's matrix columns (one of our rows, read from
	//the largest x to the smallest)
	file << "data:\n";
	for (const std::string& name : names) {
		const std::vector<float>& values = map.getLayer(name);
		file << "- layout:\n";
		file << "    dim:\n";
		file << "    - label: column_index\n";
		file << "      size: " << rows << "\n";
		file << "      stride: " << rows * columns << "\n";
		file << "    - label: row_index\n";
		file << "      size: " << columns << "\n";
		file << "      stride: " << columns << "\n";
		file << "    data_offset: 0\n";
		file << "  data: [";
		for (int j = 0; j < rows; j++) {
			file << (j == 0 ? "" : ",\n    ");
			for (int i = 0; i < columns; i++) {
				file << (i == 0 ? "" : ", ") << getYamlNumberText(values[map.getIndex(columns - 1 - i, j)]);
			}
		}
		file << "]\n";
	}
	file << "outer_start_index: 0\n";
	file << "inner_start_index: 0\n";


	return true;
}

bool loadGridMapMessage(const std::string& filename, GridMap& map) {


	// pseudo-code:
	// columns = round( length_x / resolution ),  rows = round( length_y / resolution )
	// for each layer: storage (s_i, s_j) of index (i, j) = ( ( i + outer_start ) mod columns, ( j + inner_start ) mod rows )
	//     column-major: value = data[ offset + s_j * columns + s_i ];  row-major: data[ offset + s_i * rows + s_j ]
	//     layer( column = columns - 1 - i, row = j ) = value


	std::ifstream file(filename);
	if (!file.is_open()) {
		return false;
	}
	std::vector<YamlLine> lines = readYamlLines(file);
	int resolutionLine = findYamlKey(lines, 0, "resolution");
	int lengthXLine    = findYamlKey(lines, resolutionLine, "length_x");
	int lengthYLine    = findYamlKey(lines, lengthXLine, "length_y");
	int layersLine     = findYamlKey(lines, lengthYLine, "layers");
	if (resolutionLine < 0 || lengthXLine < 0 || lengthYLine < 0 || layersLine < 0) {
		return false;
	}
	double resolution = std::stod(getYamlValue(lines[resolutionLine]));
	int columns       = (int)std::lround(std::stod(getYamlValue(lines[lengthXLine])) / resolution);
	int rows          = (int)std::lround(std::stod(getYamlValue(lines[lengthYLine])) / resolution);
	int position      = 0;
	std::vector<std::string> names = readYamlSequence(lines, layersLine, position);
	if (columns <= 0 || rows <= 0) {
		return false;
	}


	//the start indices come after the data; read them first so the data can be unshifted as it is read
	int outerLine  = findYamlKey(lines, position, "outer_start_index");
	int innerLine  = findYamlKey(lines, position, "inner_start_index");
	int outerStart = outerLine < 0 ? 0 : std::stoi(getYamlValue(lines[outerLine]));
	int innerStart = innerLine < 0 ? 0 : std::stoi(getYamlValue(lines[innerLine]));


	GridMap loaded(columns, rows, resolution);
	for (const std::string& name : names) {
		int labelLine  = findYamlKey(lines, position, "label");
		int offsetLine = findYamlKey(lines, position, "data_offset");
		int dataLine   = findYamlKey(lines, offsetLine, "data");
		if (labelLine < 0 || offsetLine < 0 || dataLine < 0) {
			return false;
		}
		bool columnMajor = getYamlValue(lines[labelLine]) == "column_index"; //grid_map_ros: dim[0] names the outer index
		int offset       = std::stoi(getYamlValue(lines[offsetLine]));
		std::vector<std::string> values = readYamlSequence(lines, dataLine, position);
		if ((int)values.size() < offset + columns * rows) {
			return false;
		}


		loaded.addLayer(name);
		std::vector<float>& layer = loaded.getLayer(name);
		for (int j = 0; j < rows; j++) {
			for (int i = 0; i < columns; i++) {
				int storageI = (i + outerStart) % columns;
				int storageJ = (j + innerStart) % rows;
				int index    = offset + (columnMajor ? storageJ * columns + storageI : storageI * rows + storageJ);
				layer[loaded.getIndex(columns - 1 - i, j)] = getYamlFloat(values[index]);
			}
		}
	}
	map = loaded;


	return true;
}

bool saveEndpoints(const std::string& filename, const GridMap& map, int startIndex, int endIndex) {


	// pseudo-code:
	// for start and end (if set): (x, y) = position of the cell's centre;  write  name: { x, y }


	std::ofstream file(filename);
	if (!file.is_open()) {
		return false;
	}
	file << "# start and end of the path (not part of grid_map_msgs/GridMap): centre of the cell, metres, map frame\n";
	const std::vector<std::pair<std::string, int>> endpoints = { { "start", startIndex }, { "end", endIndex } };
	for (const std::pair<std::string, int>& endpoint : endpoints) {
		if (endpoint.second < 0) continue;
		double x, y;
		map.getPosition(map.getColumn(endpoint.second), map.getRow(endpoint.second), x, y);
		file << endpoint.first << ":\n";
		file << "  x: " << getYamlNumberText(x) << "\n";
		file << "  y: " << getYamlNumberText(y) << "\n";
	}


	return true;
}

void loadEndpoints(const std::string& filename, const GridMap& map, int& startIndex, int& endIndex) {


	// pseudo-code:
	// for start and end: (x, y) from the file -> the cell containing it, or -1 if missing / outside the map


	startIndex = -1;
	endIndex   = -1;
	std::ifstream file(filename);
	if (!file.is_open()) {
		return;
	}
	std::vector<YamlLine> lines = readYamlLines(file);


	const std::vector<std::pair<std::string, int*>> endpoints = { { "start", &startIndex }, { "end", &endIndex } };
	for (const std::pair<std::string, int*>& endpoint : endpoints) {
		int keyLine = findYamlKey(lines, 0, endpoint.first);
		int xLine   = findYamlKey(lines, keyLine + 1, "x");
		int yLine   = findYamlKey(lines, keyLine + 1, "y");
		if (keyLine < 0 || xLine != keyLine + 1 || yLine != keyLine + 2) continue;
		int column, row;
		if (map.getCell(std::stod(getYamlValue(lines[xLine])), std::stod(getYamlValue(lines[yLine])), column, row)) {
			*endpoint.second = map.getIndex(column, row);
		}
	}
}

bool saveGridMap(const std::string& filename, const GridMap& map, int startIndex, int endIndex) {


	// pseudo-code:
	// save the message to filename;  save the endpoints to getEndpointsFilename( filename )


	if (!saveGridMapMessage(filename, map)) {
		return false;
	}


	return saveEndpoints(getEndpointsFilename(filename), map, startIndex, endIndex);
}

bool loadGridMap(const std::string& filename, GridMap& map, int& startIndex, int& endIndex) {


	// pseudo-code:
	// load the message from filename;  load the endpoints from getEndpointsFilename( filename )


	if (!loadGridMapMessage(filename, map)) {
		return false;
	}
	loadEndpoints(getEndpointsFilename(filename), map, startIndex, endIndex);


	return true;
}
