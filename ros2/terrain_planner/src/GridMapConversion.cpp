#include "terrain_planner/GridMapConversion.h"
#include <algorithm>
#include <cmath>
#include "CostMap.h"

namespace terrain_planner {

void getWorldPosition(const GridMap& map, const MapPlacement& placement, int column, int row, double& x, double& y) {
	map.getPosition(column, row, x, y);
	x += placement.centerX;
	y += placement.centerY;
}

bool getCellAtWorld(const GridMap& map, const MapPlacement& placement, double x, double y, int& column, int& row) {
	return map.getCell(x - placement.centerX, y - placement.centerY, column, row);
}

bool fromGridMap(const grid_map::GridMap& in, GridMap& out, MapPlacement& placement, const std::vector<std::string>& layers) {
	std::vector<std::string> names = layers.empty() ? in.getLayers() : layers;
	for (const std::string& name : names) {
		if (!in.exists(name)) return false;
	}
	//grid_map's size is (cells along x, cells along y): x = our columns, y = our rows
	const grid_map::Size size = in.getSize();
	out = GridMap(size(0), size(1), in.getResolution());
	placement.frameId = in.getFrameId();
	placement.centerX = in.getPosition().x();
	placement.centerY = in.getPosition().y();
	placement.timestampNs = in.getTimestamp();

	for (const std::string& name : names) {
		out.addLayer(name, GridMap::getUnknownValue());
		std::vector<float>& layer = out.getLayer(name);
		for (int row = 0; row < out.getRows(); row++) {
			for (int column = 0; column < out.getColumns(); column++) {
				double x, y;
				getWorldPosition(out, placement, column, row, x, y);
				grid_map::Index index;
				if (in.getIndex(grid_map::Position(x, y), index)) {
					layer[out.getIndex(column, row)] = in.at(name, index); //NaN stays NaN
				}
			}
		}
	}
	return true;
}

grid_map::GridMap toGridMap(const GridMap& in, const MapPlacement& placement, const std::vector<std::string>& layers) {
	std::vector<std::string> names = layers.empty() ? in.getLayerNames() : layers;
	grid_map::GridMap out(names);
	out.setFrameId(placement.frameId);
	out.setTimestamp(placement.timestampNs);
	out.setGeometry(grid_map::Length(in.getColumns() * in.getResolution(), in.getRows() * in.getResolution()),
		in.getResolution(), grid_map::Position(placement.centerX, placement.centerY));

	for (const std::string& name : names) {
		if (!in.hasLayer(name)) continue; //stays NaN in the output
		const std::vector<float>& layer = in.getLayer(name);
		for (int row = 0; row < in.getRows(); row++) {
			for (int column = 0; column < in.getColumns(); column++) {
				double x, y;
				getWorldPosition(in, placement, column, row, x, y);
				grid_map::Index index;
				if (out.getIndex(grid_map::Position(x, y), index)) {
					out.at(name, index) = layer[in.getIndex(column, row)];
				}
			}
		}
	}
	return out;
}

nav_msgs::msg::OccupancyGrid toOccupancyGrid(const GridMap& map, const MapPlacement& placement,
	const CostMapParameters& parameters, const std::string& layerName) {
	nav_msgs::msg::OccupancyGrid grid;
	grid.header.frame_id = placement.frameId;
	grid.header.stamp.sec = (int32_t)(placement.timestampNs / 1000000000ULL);
	grid.header.stamp.nanosec = (uint32_t)(placement.timestampNs % 1000000000ULL);
	grid.info.resolution = (float)map.getResolution();
	grid.info.width = map.getColumns();
	grid.info.height = map.getRows();
	//origin = the outer corner of OccupancyGrid cell (0, 0), which is the bottom-left cell (lowest x and y)
	grid.info.origin.position.x = placement.centerX - map.getColumns() * map.getResolution() / 2;
	grid.info.origin.position.y = placement.centerY - map.getRows() * map.getResolution() / 2;
	grid.info.origin.orientation.w = 1.0;
	grid.data.assign(map.getCellCount(), -1);
	if (!map.hasLayer(layerName)) return grid;

	const std::vector<float>& layer = map.getLayer(layerName);
	for (int row = 0; row < map.getRows(); row++) {
		for (int column = 0; column < map.getColumns(); column++) {
			float cost = layer[map.getIndex(column, row)];
			int8_t value;
			if (GridMap::isUnknown(cost)) value = -1;
			else if (isLethal(cost, parameters)) value = 100;
			else value = (int8_t)std::clamp((int)std::lround(cost * 99.0), 0, 99);
			//OccupancyGrid rows go up in y (row 0 at the bottom); the core's row 0 is the top
			int gridRow = map.getRows() - 1 - row;
			grid.data[gridRow * map.getColumns() + column] = value;
		}
	}
	return grid;
}

} // namespace terrain_planner
