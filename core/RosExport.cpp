#include "RosExport.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include "CostMap.h"
#include "GridMapFile.h"

int getOccupancyValue(float cost, const CostMapParameters& parameters) {


	// pseudo-code:
	// unknown -> -1;  lethal -> 100
	// otherwise: round( cost / ( 1 - traversabilityThreshold ) * 99 ), clamped to 0 .. 99
	// (walkable costs are 1 - traversability, so they run from 0 to 1 - traversabilityThreshold = 0.5)


	if (GridMap::isUnknown(cost)) return -1;
	if (isLethal(cost, parameters)) return 100;
	double highestWalkableCost = 1.0 - parameters.traversabilityThreshold;
	int value = (int)std::lround(cost / highestWalkableCost * 99.0);


	return std::max(0, std::min(99, value));
}

double getPathYaw(const GridMap& map, const std::vector<int>& path, int index) {


	// pseudo-code:
	// (a, b) = ( path[ index ], path[ index + 1 ] ), or ( path[ index - 1 ], path[ index ] ) at the last pose
	// yaw = atan2( y_b - y_a, x_b - x_a )


	if (path.size() < 2) return 0.0;
	int from = (index + 1 < (int)path.size()) ? path[index] : path[index - 1];
	int to   = (index + 1 < (int)path.size()) ? path[index + 1] : path[index];
	double fromX, fromY, toX, toY;
	map.getPosition(map.getColumn(from), map.getRow(from), fromX, fromY);
	map.getPosition(map.getColumn(to), map.getRow(to), toX, toY);


	return std::atan2(toY - fromY, toX - fromX);
}

void writeOccupancyGridMessage(std::ostream& file, const GridMap& map, const CostMapParameters& parameters, const std::string& frameId) {


	// pseudo-code:
	// info: resolution, width = columns, height = rows, origin = the map's lower-left corner ( -columns * r / 2, -rows * r / 2 )
	// data: row-major from the origin (smallest x and y): data[ gridRow * width + column ] with our row = rows - 1 - gridRow
	//       value = getOccupancyValue( cost( column, row ) )


	int columns = map.getColumns();
	int rows    = map.getRows();
	const std::vector<float>& cost = map.getLayer("cost");


	file << "# nav_msgs/msg/OccupancyGrid (ROS 2 Jazzy): the cost layer, -1 unknown, 0 .. 99 cost, 100 lethal\n";
	file << "header:\n";
	file << "  stamp:\n";
	file << "    sec: 0\n";
	file << "    nanosec: 0\n";
	file << "  frame_id: " << frameId << "\n";
	file << "info:\n";
	file << "  map_load_time:\n";
	file << "    sec: 0\n";
	file << "    nanosec: 0\n";
	file << "  resolution: " << getYamlNumberText((float)map.getResolution()) << "\n";
	file << "  width: " << columns << "\n";
	file << "  height: " << rows << "\n";
	file << "  origin:\n";
	file << "    position:\n";
	file << "      x: " << getYamlNumberText(-columns * map.getResolution() / 2) << "\n";
	file << "      y: " << getYamlNumberText(-rows * map.getResolution() / 2) << "\n";
	file << "      z: 0.0\n";
	file << "    orientation:\n";
	file << "      x: 0.0\n";
	file << "      y: 0.0\n";
	file << "      z: 0.0\n";
	file << "      w: 1.0\n";


	//one line of data per grid row, from the bottom row of the map (largest row index) up
	file << "data: [";
	for (int gridRow = 0; gridRow < rows; gridRow++) {
		file << (gridRow == 0 ? "" : ",\n  ");
		int row = rows - 1 - gridRow;
		for (int column = 0; column < columns; column++) {
			file << (column == 0 ? "" : ", ") << getOccupancyValue(cost[map.getIndex(column, row)], parameters);
		}
	}
	file << "]\n";
}

void writePoseStampedMessage(std::ostream& file, const GridMap& map, int cell, double yaw, const std::string& frameId, const std::string& indent) {


	// pseudo-code:
	// position = centre of the cell (metres), z = 0
	// orientation = ( 0, 0, sin( yaw / 2 ), cos( yaw / 2 ) )


	double x, y;
	map.getPosition(map.getColumn(cell), map.getRow(cell), x, y);


	file << indent << "header:\n";
	file << indent << "  stamp:\n";
	file << indent << "    sec: 0\n";
	file << indent << "    nanosec: 0\n";
	file << indent << "  frame_id: " << frameId << "\n";
	file << indent << "pose:\n";
	file << indent << "  position:\n";
	file << indent << "    x: " << getYamlNumberText(x) << "\n";
	file << indent << "    y: " << getYamlNumberText(y) << "\n";
	file << indent << "    z: 0.0\n";
	file << indent << "  orientation:\n";
	file << indent << "    x: 0.0\n";
	file << indent << "    y: 0.0\n";
	file << indent << "    z: " << getYamlNumberText(std::sin(yaw / 2)) << "\n";
	file << indent << "    w: " << getYamlNumberText(std::cos(yaw / 2)) << "\n";
}

void writePathMessage(std::ostream& file, const GridMap& map, const std::vector<int>& path, const std::string& frameId) {


	// pseudo-code:
	// header;  poses = [ PoseStamped( path[ k ], getPathYaw( path, k ) ) for every k ]


	file << "# nav_msgs/msg/Path (ROS 2 Jazzy): " << path.size() << " poses, cell centres in metres\n";
	file << "header:\n";
	file << "  stamp:\n";
	file << "    sec: 0\n";
	file << "    nanosec: 0\n";
	file << "  frame_id: " << frameId << "\n";
	if (path.empty()) {
		file << "poses: []\n";
		return;
	}


	//each pose is a sequence item: its first line starts with "- ", the rest are indented to match
	file << "poses:\n";
	for (int index = 0; index < (int)path.size(); index++) {
		std::ostringstream pose;
		writePoseStampedMessage(pose, map, path[index], getPathYaw(map, path, index), frameId, "  ");
		std::string text = pose.str();
		text.replace(0, 2, "- ");
		file << text;
	}
}

void writeWaypointsMessage(std::ostream& file, const std::vector<Waypoint>& waypoints, const std::string& frameId) {


	// pseudo-code:
	// header;  poses = [ PoseStamped( stamp = time_k, position = ( x_k, y_k, 0 ), orientation = yaw_k ) for every k ]
	// stamp: sec = floor( t ), nanosec = round( ( t - sec ) * 1e9 )   (seconds from the first waypoint; the ROS node adds now())


	file << "# nav_msgs/msg/Path (ROS 2 Jazzy): " << waypoints.size() << " waypoints, stamp = arrival time in s from the first\n";
	file << "header:\n";
	file << "  stamp:\n";
	file << "    sec: 0\n";
	file << "    nanosec: 0\n";
	file << "  frame_id: " << frameId << "\n";
	if (waypoints.empty()) {
		file << "poses: []\n";
		return;
	}


	file << "poses:\n";
	for (const Waypoint& waypoint : waypoints) {
		long long seconds = (long long)std::floor(waypoint.time);
		long long nanoseconds = std::min(999999999LL, (long long)std::llround((waypoint.time - seconds) * 1e9));
		file << "- header:\n";
		file << "    stamp:\n";
		file << "      sec: " << seconds << "\n";
		file << "      nanosec: " << nanoseconds << "\n";
		file << "    frame_id: " << frameId << "\n";
		file << "  pose:\n";
		file << "    position:\n";
		file << "      x: " << getYamlNumberText(waypoint.x) << "\n";
		file << "      y: " << getYamlNumberText(waypoint.y) << "\n";
		file << "      z: 0.0\n";
		file << "    orientation:\n";
		file << "      x: 0.0\n";
		file << "      y: 0.0\n";
		file << "      z: " << getYamlNumberText(std::sin(waypoint.yaw / 2)) << "\n";
		file << "      w: " << getYamlNumberText(std::cos(waypoint.yaw / 2)) << "\n";
	}
}

std::string getRosPublishCommand(const std::string& topic, const std::string& type, const std::string& filename) {


	// pseudo-code:
	// ros2 topic pub --once /<topic> <type> "$(cat '<filename>')"


	return "ros2 topic pub --once /" + topic + " " + type + " \"$(cat '" + filename + "')\"";
}
