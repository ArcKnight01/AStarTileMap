#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <grid_map_core/GridMap.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include "CostMapParameters.h"
#include "GridMap.h"

// grid_map::GridMap  <->  the core's GridMap.
//
// The core's GridMap is always centred on (0, 0) (x right with the column, y up toward row 0). A grid_map map has its
// centre somewhere in its frame and moves with the robot, so the centre and frame travel next to the core map in a
// MapPlacement:   world (frame) position = placement.center + core position.
//
// Cells are copied by POSITION (each core cell centre -> grid_map.atPosition), never by index, so neither library's index
// order (grid_map: index (0, 0) at max x / max y, possibly a circular buffer) matters. NaN stays NaN (unknown).
namespace terrain_planner {

struct MapPlacement {
	std::string frameId = "map";
	double centerX = 0; //m, in frameId
	double centerY = 0;
	uint64_t timestampNs = 0;
};

//world (frame) position of a core cell's centre, and the core cell at a world position (false if outside the map)
void getWorldPosition(const GridMap& map, const MapPlacement& placement, int column, int row, double& x, double& y);
bool getCellAtWorld(const GridMap& map, const MapPlacement& placement, double x, double y, int& column, int& row);

//copies `layers` (or every layer if empty) of a grid_map map into `out`; false if a requested layer is missing
bool fromGridMap(const grid_map::GridMap& in, GridMap& out, MapPlacement& placement,
	const std::vector<std::string>& layers = {});
//a grid_map map with every layer of `in` (or only `layers`), at the placement's centre and frame
grid_map::GridMap toGridMap(const GridMap& in, const MapPlacement& placement, const std::vector<std::string>& layers = {});

//the "cost" layer for RViz: unknown -> -1, lethal -> 100, traversable cost 0 .. 1 -> 0 .. 99
nav_msgs::msg::OccupancyGrid toOccupancyGrid(const GridMap& map, const MapPlacement& placement,
	const CostMapParameters& parameters, const std::string& layer = "cost");

} // namespace terrain_planner
