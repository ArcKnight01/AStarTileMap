// grid_map::GridMap <-> core GridMap (ROS2_PLAN.md section 5): features must land at the same world position.
#include <cmath>
#include <gtest/gtest.h>
#include <grid_map_ros/grid_map_ros.hpp>
#include "terrain_planner/GridMapConversion.h"

using namespace terrain_planner;

//a grid_map map of columns x rows cells at 0.1 m, centred on (centerX, centerY), elevation 0 with one raised cell
static grid_map::GridMap makeElevationMap(int columns, int rows, double centerX, double centerY, double bumpX, double bumpY) {
	grid_map::GridMap map({ "elevation" });
	map.setFrameId("map");
	map.setGeometry(grid_map::Length(columns * 0.1, rows * 0.1), 0.1, grid_map::Position(centerX, centerY));
	map["elevation"].setZero();
	map.atPosition("elevation", grid_map::Position(bumpX, bumpY)) = 1.0f;
	return map;
}

static void expectBumpAt(const GridMap& map, const MapPlacement& placement, double bumpX, double bumpY) {
	int column, row;
	ASSERT_TRUE(getCellAtWorld(map, placement, bumpX, bumpY, column, row));
	EXPECT_FLOAT_EQ(map.at("elevation", column, row), 1.0f);
	int raised = 0;
	for (float value : map.getLayer("elevation")) raised += (value == 1.0f);
	EXPECT_EQ(raised, 1);
}

TEST(Conversion, FeatureKeepsItsPosition) {
	grid_map::GridMap in = makeElevationMap(40, 40, 0, 0, 1.05, 0.05);
	GridMap map;
	MapPlacement placement;
	ASSERT_TRUE(fromGridMap(in, map, placement));
	EXPECT_EQ(map.getColumns(), 40);
	EXPECT_EQ(map.getRows(), 40);
	expectBumpAt(map, placement, 1.05, 0.05);
	//+x is to the right of the centre column
	int column, row;
	getCellAtWorld(map, placement, 1.05, 0.05, column, row);
	EXPECT_GT(column, map.getColumns() / 2);
}

TEST(Conversion, YUpIsTowardRowZero) {
	grid_map::GridMap in = makeElevationMap(40, 40, 0, 0, 0.05, 1.05);
	GridMap map;
	MapPlacement placement;
	ASSERT_TRUE(fromGridMap(in, map, placement));
	int column, row;
	ASSERT_TRUE(getCellAtWorld(map, placement, 0.05, 1.05, column, row));
	EXPECT_LT(row, map.getRows() / 2);
	EXPECT_FLOAT_EQ(map.at("elevation", column, row), 1.0f);
}

TEST(Conversion, NonSquareAndOffCentre) {
	grid_map::GridMap in = makeElevationMap(30, 50, 3.0, -2.0, 3.85, -3.15);
	GridMap map;
	MapPlacement placement;
	ASSERT_TRUE(fromGridMap(in, map, placement));
	EXPECT_EQ(map.getColumns(), 30);
	EXPECT_EQ(map.getRows(), 50);
	EXPECT_DOUBLE_EQ(placement.centerX, 3.0);
	EXPECT_DOUBLE_EQ(placement.centerY, -2.0);
	expectBumpAt(map, placement, 3.85, -3.15);
}

TEST(Conversion, MovedMapStillLinesUp) {
	grid_map::GridMap in = makeElevationMap(40, 40, 0, 0, 0.55, 0.55);
	in.move(grid_map::Position(0.3, -0.2)); //circular buffer: the start index is no longer (0, 0)
	GridMap map;
	MapPlacement placement;
	ASSERT_TRUE(fromGridMap(in, map, placement));
	expectBumpAt(map, placement, 0.55, 0.55);
}

TEST(Conversion, RoundTripAndUnknown) {
	grid_map::GridMap in = makeElevationMap(20, 30, 1.0, 1.0, 1.25, 0.35);
	in.atPosition("elevation", grid_map::Position(0.35, 1.65)) = NAN;
	GridMap map;
	MapPlacement placement;
	ASSERT_TRUE(fromGridMap(in, map, placement));
	grid_map::GridMap out = toGridMap(map, placement);
	EXPECT_FLOAT_EQ(out.atPosition("elevation", grid_map::Position(1.25, 0.35)), 1.0f);
	EXPECT_TRUE(std::isnan(out.atPosition("elevation", grid_map::Position(0.35, 1.65))));
	EXPECT_FLOAT_EQ(out.atPosition("elevation", grid_map::Position(1.05, 1.05)), 0.0f);
	//through a message too
	grid_map::GridMap fromMessage;
	ASSERT_TRUE(grid_map::GridMapRosConverter::fromMessage(*grid_map::GridMapRosConverter::toMessage(out), fromMessage));
	EXPECT_FLOAT_EQ(fromMessage.atPosition("elevation", grid_map::Position(1.25, 0.35)), 1.0f);
}

TEST(Conversion, MissingLayerFails) {
	grid_map::GridMap in = makeElevationMap(10, 10, 0, 0, 0.05, 0.05);
	GridMap map;
	MapPlacement placement;
	EXPECT_FALSE(fromGridMap(in, map, placement, { "cost" }));
}

TEST(Conversion, OccupancyGridRowsGoUp) {
	GridMap map(4, 3, 0.1);
	map.addLayer("cost", 0.f);
	CostMapParameters parameters;
	map.at("cost", 0, 0) = parameters.lethalCost; //top-left in the core = highest y
	map.at("cost", 3, 2) = GridMap::getUnknownValue();
	MapPlacement placement;
	nav_msgs::msg::OccupancyGrid grid = toOccupancyGrid(map, placement, parameters);
	ASSERT_EQ(grid.data.size(), 12u);
	EXPECT_EQ(grid.data[2 * 4 + 0], 100); //top row of the grid (row 2 of 3) = core row 0
	EXPECT_EQ(grid.data[0 * 4 + 3], -1);  //bottom row = core row 2
	EXPECT_NEAR(grid.info.origin.position.x, -0.2, 1e-9);
	EXPECT_NEAR(grid.info.origin.position.y, -0.15, 1e-9);
}
