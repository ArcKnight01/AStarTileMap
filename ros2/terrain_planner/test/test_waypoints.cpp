// Path pruning and waypoints (plain C++, no ROS).
#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include "CostMap.h"
#include "PathPlanner.h"
#include "Waypoints.h" //core/Waypoints.h (was terrain_planner/Waypoints.h)

//using namespace terrain_planner; (the waypoint code is in core/ now, outside the namespace)

//true if the straight line between two cell centres passes through the inside of a cell (slab test on the cell's
//square, shrunk by 1e-9 so a line that only touches a corner or an edge does not count), in column / row units
static bool crossesCellInside(const GridMap& map, int fromCell, int toCell, int cell) {
	double x0 = map.getColumn(fromCell) + 0.5, y0 = map.getRow(fromCell) + 0.5;
	double dx = map.getColumn(toCell) + 0.5 - x0, dy = map.getRow(toCell) + 0.5 - y0;
	double low[2] = { map.getColumn(cell) + 1e-9, map.getRow(cell) + 1e-9 }, high[2] = { map.getColumn(cell) + 1 - 1e-9, map.getRow(cell) + 1 - 1e-9 };
	double start[2] = { x0, y0 }, direction[2] = { dx, dy }, tEnter = 0, tLeave = 1;
	for (int axis = 0; axis < 2; axis++) {
		if (direction[axis] == 0) {
			if (start[axis] <= low[axis] || start[axis] >= high[axis]) return false;
			continue;
		}
		double t0 = (low[axis] - start[axis]) / direction[axis], t1 = (high[axis] - start[axis]) / direction[axis];
		tEnter = std::max(tEnter, std::min(t0, t1));
		tLeave = std::min(tLeave, std::max(t0, t1));
	}
	return tEnter < tLeave;
}

static GridMap makeFlatCostMap(int columns, int rows) {
	GridMap map(columns, rows, 0.1);
	map.addLayer("cost", 0.f);
	return map;
}

TEST(Waypoints, StraightPathPrunesToItsEnds) {
	GridMap map = makeFlatCostMap(20, 5);
	CostMapParameters parameters;
	std::vector<int> path;
	for (int column = 0; column < 20; column++) path.push_back(map.getIndex(column, 2));
	std::vector<int> pruned = prunePath(map, parameters, path);
	ASSERT_EQ(pruned.size(), 2u);
	EXPECT_EQ(pruned.front(), path.front());
	EXPECT_EQ(pruned.back(), path.back());
}

TEST(Waypoints, PruningNeverCrossesALethalCell) {
	GridMap map = makeFlatCostMap(10, 10);
	CostMapParameters parameters;
	parameters.inflationRadius = 0;
	for (int row = 0; row < 8; row++) map.at("cost", 5, row) = parameters.lethalCost; //a wall with a gap at the bottom
	PathResult result = findPath(map, parameters, PathAlgorithm::AStar, map.getIndex(0, 0), map.getIndex(9, 0));
	ASSERT_FALSE(result.path.empty());
	std::vector<int> pruned = prunePath(map, parameters, result.path);
	EXPECT_LT(pruned.size(), result.path.size());
	for (std::size_t i = 1; i < pruned.size(); i++) {
		//(was: every cell getCellsOnSegment returns must be walkable; the exact walk now also returns the cells whose corner
		//a line only touches, as A*'s own diagonal step past the wall's end does, so check for crossing a cell's inside)
		for (int cell = 0; cell < map.getCellCount(); cell++) {
			if (isLethal(map.getLayer("cost")[cell], parameters)) {
				EXPECT_FALSE(crossesCellInside(map, pruned[i - 1], pruned[i], cell)) << "segment " << i << " crosses the wall";
			}
		}
	}
	EXPECT_LE(getPathCost(map, parameters, pruned), result.cost + 1e-6);
}

TEST(Waypoints, SpacingHeadingAndTime) {
	GridMap map = makeFlatCostMap(21, 3);
	WaypointParameters waypointParameters;
	waypointParameters.spacing = 0.5;
	waypointParameters.nominalSpeed = 1.0;
	std::vector<int> path = { map.getIndex(0, 1), map.getIndex(20, 1) }; //2 m along +x
	std::vector<Waypoint> waypoints = makeWaypoints(map, path, waypointParameters, M_PI / 2);
	ASSERT_EQ(waypoints.size(), 5u); //0, 0.5, 1.0, 1.5, 2.0 m
	for (std::size_t i = 1; i < waypoints.size(); i++) {
		EXPECT_NEAR(waypoints[i].x - waypoints[i - 1].x, 0.5, 1e-9);
		EXPECT_NEAR(waypoints[i].time - waypoints[i - 1].time, 0.5, 1e-9); //cost 0, 1 m/s
	}
	EXPECT_NEAR(waypoints.front().yaw, 0.0, 1e-9);     //toward +x
	EXPECT_NEAR(waypoints.back().yaw, M_PI / 2, 1e-9); //the goal's yaw
}

TEST(Waypoints, CostlyCellsAreSlower) {
	GridMap map = makeFlatCostMap(21, 3);
	WaypointParameters waypointParameters;
	std::vector<int> path = { map.getIndex(0, 1), map.getIndex(20, 1) };
	double cheapTime = makeWaypoints(map, path, waypointParameters).back().time;
	map.addLayer("cost", 0.5f);
	double costlyTime = makeWaypoints(map, path, waypointParameters).back().time;
	EXPECT_GT(costlyTime, cheapTime);
}
