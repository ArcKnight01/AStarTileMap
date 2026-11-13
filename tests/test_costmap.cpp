// Tests of core/ (no SFML): the grid map, each stage of the cost-map pipeline, the planners, and the example maps.
// Run: ctest --test-dir build --output-on-failure   (runs from the repo folder, so maps/ is found)
#include <cmath>
#include <cstdio>
#include <queue>
#include <random>
#include "CostMap.h"
#include "CostMapParameters.h"
#include "GridMap.h"
#include "GridMapFile.h"
#include "PathPlanner.h"
#include "RosExport.h"
#include "Waypoints.h"
#include <algorithm>
#include <sstream>
#include "TerrainFeatures.h"
#include "TerrainGenerator.h"
#include "Traversability.h"
#include "../src/TerrainBrush.h" //the brush kernel is header-only, with no SFML

static int failures = 0;

//true if the straight line between two world positions (metres) passes through the inside of a cell: a slab test on the
//cell's square in column / row units, shrunk by 1e-9 so a line that only touches a corner or an edge does not count
static bool crossesCellInside(const GridMap& map, double x0, double y0, double x1, double y1, int cell) {
	//world -> grid units: u = x / r + columns / 2 grows with the column, v = rows / 2 - y / r grows with the row
	double r = map.getResolution();
	double start[2] = { x0 / r + map.getColumns() / 2.0, map.getRows() / 2.0 - y0 / r };
	double direction[2] = { (x1 - x0) / r, -(y1 - y0) / r };
	double low[2] = { map.getColumn(cell) + 1e-9, map.getRow(cell) + 1e-9 }, high[2] = { map.getColumn(cell) + 1 - 1e-9, map.getRow(cell) + 1 - 1e-9 };
	double tEnter = 0, tLeave = 1;
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
#define CHECK(cond, ...) do { if (!(cond)) { failures++; std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); } } while (0)

// Cheapest path cost with the planner's own step cost (eq. 3.8), by brute force (Dijkstra with a priority queue).
// With unitCost every step costs 1, which gives the fewest steps (what BFS minimises). -1 = unreachable.
/**
 * Cheapest path cost by brute force (Dijkstra), for checking the planners.
 *
 * @param map       Map with a "cost" layer.
 * @param p         Cost-map parameters (lethal threshold, step cost).
 * @param start     Start cell index (row * columns + column).
 * @param end       End cell index (row * columns + column).
 * @param unitCost  True: every step costs 1 (fewest steps); false: the planner's step cost.
 * @return          Cheapest path cost, or -1 if the end is unreachable.
 */
static double bruteForceCost(const GridMap& map, const CostMapParameters& p, int start, int end, bool unitCost) {
	const std::vector<float>& cost = map.getLayer("cost");
	if (isLethal(cost[start], p) || isLethal(cost[end], p)) return -1;
	std::vector<double> best(map.getCellCount(), 1e300);
	std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<>> queue;
	best[start] = 0; queue.push({ 0, start });
	while (!queue.empty()) {
		auto top = queue.top(); queue.pop();
		double d = top.first; int cell = top.second;
		if (d > best[cell]) continue;
		if (cell == end) return d;
		for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
			if (!dx && !dy) continue;
			int c = map.getColumn(cell) + dx, r = map.getRow(cell) + dy;
			if (!map.isInside(c, r)) continue;
			int n = map.getIndex(c, r);
			if (isLethal(cost[n], p)) continue;
			double nd = d + (unitCost ? 1.0 : getStepCost(map, p, cell, n));
			if (nd < best[n] - 1e-12) { best[n] = nd; queue.push({ nd, n }); }
		}
	}
	return -1;
}

/**
 * A path's shape: starts and ends right, only neighbouring steps, never on a lethal cell.
 *
 * @param map    Map with a "cost" layer.
 * @param p      Cost-map parameters (lethal threshold).
 * @param path   Path as cell indices (row * columns + column), start first.
 * @param start  Expected first cell index.
 * @param end    Expected last cell index.
 * @param what   Label printed in failure messages.
 */
static void checkPathShape(const GridMap& map, const CostMapParameters& p, const std::vector<int>& path, int start, int end, const char* what) {
	CHECK(path.front() == start && path.back() == end, "%s: path does not run from start to end", what);
	for (std::size_t k = 0; k < path.size(); k++) {
		CHECK(!isLethal(map.getLayer("cost")[path[k]], p), "%s: path enters a lethal cell", what);
		if (k == 0) continue;
		int dc = std::abs(map.getColumn(path[k]) - map.getColumn(path[k - 1]));
		int dr = std::abs(map.getRow(path[k]) - map.getRow(path[k - 1]));
		CHECK(std::max(dc, dr) == 1, "%s: path jumps between cells", what);
	}
}

/**
 * Tells whether two values differ by at most a tolerance.
 *
 * @param a          First value.
 * @param b          Second value.
 * @param tolerance  Largest allowed absolute difference.
 * @return           True if |a - b| <= tolerance.
 */
static bool near(double a, double b, double tolerance) { return std::fabs(a - b) <= tolerance; }

/**
 * Runs all core checks and prints each failure.
 *
 * @return  Exit code: 0 if every check passed, 1 otherwise.
 */
int main() {
	CostMapParameters p;

	// --- GridMap: indices, positions, layers, file round trip ---
	{
		GridMap map(10, 6, 0.1);
		CHECK(map.getIndex(3, 2) == 23 && map.getColumn(23) == 3 && map.getRow(23) == 2, "grid: index <-> column/row");
		double x, y; int c, r;
		map.getPosition(3, 2, x, y);
		CHECK(map.getCell(x, y, c, r) && c == 3 && r == 2, "grid: cell -> world -> cell");
		map.getPosition(0, 0, x, y);
		CHECK(x < 0 && y > 0, "grid: the map is centred on (0, 0) with row 0 at the top (+y)");
		map.addLayer("elevation", 1.5f);
		map.at("elevation", 4, 4) = GridMap::getUnknownValue();
		CHECK(GridMap::isUnknown(map.at("elevation", 4, 4)) && map.at("elevation", 0, 0) == 1.5f, "grid: layers and unknown values");
		saveGridMap("/tmp/test_costmap_roundtrip.gridmap", map, 5, 17);
		GridMap loaded; int start, end;
		CHECK(loadGridMap("/tmp/test_costmap_roundtrip.gridmap", loaded, start, end), "file: loads what it saved");
		CHECK(loaded.getColumns() == 10 && loaded.getRows() == 6 && near(loaded.getResolution(), 0.1, 1e-9) && start == 5 && end == 17,
			"file: size, resolution, start and end survive");
		CHECK(GridMap::isUnknown(loaded.at("elevation", 4, 4)) && loaded.at("elevation", 0, 0) == 1.5f, "file: values and NaN survive");

		//grid_map's storage order: index (0, 0) is the corner with the largest x and y, i runs toward -x, j toward -y,
		//column-major (checked against grid_map_ros::GridMapRosConverter::fromMessage when the format was written)
		CHECK(getMessageIndex(map, 9, 0) == 0, "file: data[0] is the cell with the largest x and y (column 9, row 0)");
		CHECK(getMessageIndex(map, 8, 0) == 1 && getMessageIndex(map, 9, 1) == 10, "file: data runs toward -x first, then -y");
		CHECK(getEndpointsFilename("maps/rugged.gridmap") == "maps/rugged.endpoints.yaml", "file: start / end go next to the map");
		GridMap noEndpoints; int noStart, noEnd;
		saveGridMapMessage("/tmp/test_costmap_no_endpoints.gridmap", map);
		std::remove("/tmp/test_costmap_no_endpoints.endpoints.yaml");
		CHECK(loadGridMap("/tmp/test_costmap_no_endpoints.gridmap", noEndpoints, noStart, noEnd) && noStart == -1 && noEnd == -1,
			"file: a map without an endpoints file loads with no start or end");
	}

	// --- features on terrain with known answers ---
	{
		GridMap flat = createFlatMap(20, 20, 0.1);
		computeTerrainFeatures(flat, p);
		CHECK(near(flat.at("slope", 10, 10), 0, 1e-6) && near(flat.at("roughness", 10, 10), 0, 1e-6) && near(flat.at("step", 10, 10), 0, 1e-6),
			"features: flat ground has no slope, roughness or step");

		const double angle = 0.30; //rad: a ramp rising along +x
		GridMap ramp = createFlatMap(20, 20, 0.1);
		for (int row = 0; row < 20; row++) for (int column = 0; column < 20; column++) {
			double x, y; ramp.getPosition(column, row, x, y);
			ramp.at("elevation", column, row) = (float)(std::tan(angle) * x);
		}
		computeTerrainFeatures(ramp, p);
		CHECK(near(ramp.at("slope", 10, 10), angle, 1e-3), "features: a %.2f rad ramp has slope %.3f", angle, ramp.at("slope", 10, 10));
		CHECK(near(ramp.at("roughness", 10, 10), 0, 1e-4), "features: a plane has no roughness");

		GridMap block = createFlatMap(20, 20, 0.1);
		addBlock(block, 10, 0, 19, 19, 0.3); //a 0.3 m step at column 10
		computeTerrainFeatures(block, p);
		CHECK(near(block.at("step", 9, 10), 0.3, 1e-5) && near(block.at("step", 3, 10), 0, 1e-6), "features: the step height is found at the edge only");

		GridMap holes = createFlatMap(20, 20, 0.1);
		holes.at("elevation", 5, 5) = GridMap::getUnknownValue();
		computeTerrainFeatures(holes, p);
		CHECK(GridMap::isUnknown(holes.at("slope", 5, 5)), "features: an unknown cell has unknown features");
	}

	// --- traversability (Wermelinger et al. / ETH scoring) ---
	{
		CHECK(getFeatureScore(0, 1) == 1.0 && near(getFeatureScore(0.25, 1), 0.75, 1e-12) && getFeatureScore(1, 1) == 0.0 && getFeatureScore(2, 1) == 0.0,
			"traversability: score = 1 - value / critical, 0 at or above critical");
		GridMap map(1, 1, 0.1);
		map.addLayer("slope", (float)(2 * p.criticalSlope)); //too steep
		map.addLayer("roughness", 0.f);
		map.addLayer("step", 0.f);
		computeTraversability(map, p);
		CHECK(near(map.getLayer("traversability")[0], 2.0 / 3.0, 1e-6), "traversability: (ETH) a critical slope only zeroes its own third");
		CostMapParameters strict = p;
		strict.zeroIfAnyCritical = true;
		computeTraversability(map, strict);
		CHECK(map.getLayer("traversability")[0] == 0.f, "traversability: (strict) any critical feature makes the cell 0");
	}

	// --- cost map (Rajput Algorithm 1) and inflation ---
	{
		GridMap map(3, 1, 0.1);
		map.addLayer("traversability");
		std::vector<float>& t = map.getLayer("traversability");
		t[0] = 0.9f; t[1] = 0.3f; t[2] = GridMap::getUnknownValue();
		buildCostMap(map, p);
		const std::vector<float>& cost = map.getLayer("cost");
		CHECK(near(cost[0], 0.1, 1e-6) && isLethal(cost[1], p) && isLethal(cost[2], p),
			"cost map: 1 - t when t >= 0.5, lethal below, unknown lethal by default");
		CostMapParameters fixed = p;
		fixed.unknownPolicy = UnknownPolicy::FixedCost;
		buildCostMap(map, fixed);
		CHECK(map.getLayer("cost")[2] == fixed.unknownCost, "cost map: UnknownPolicy::FixedCost");

		GridMap inflate(21, 21, 0.1);
		inflate.addLayer("cost", 0.f);
		inflate.at("cost", 10, 10) = p.lethalCost;
		inflateCostMap(inflate, p);
		int reach = (int)std::round(p.inflationRadius / 0.1);
		CHECK(isLethal(inflate.at("cost", 10 + reach, 10), p) && !isLethal(inflate.at("cost", 10 + reach + 1, 10), p),
			"inflation: lethal grows by exactly the inflation radius");
	}

	// --- planners on random cost maps, against brute force ---
	{
		std::mt19937 rng(2026);
		int withPath = 0, withoutPath = 0;
		for (int trial = 0; trial < 400; trial++) {
			GridMap map(30, 30, 0.1);
			map.addLayer("cost", 0.f);
			std::uniform_real_distribution<float> costValue(0.f, 0.5f);
			std::bernoulli_distribution lethal((trial % 5) * 0.08);
			for (float& c : map.getLayer("cost")) c = lethal(rng) ? p.lethalCost : costValue(rng);
			std::uniform_int_distribution<int> cell(0, map.getCellCount() - 1);
			int start = cell(rng), end = cell(rng);
			double cheapest = bruteForceCost(map, p, start, end, false);
			double fewest = bruteForceCost(map, p, start, end, true);
			(cheapest < 0 ? withoutPath : withPath)++;
			for (PathAlgorithm algorithm : { PathAlgorithm::AStar, PathAlgorithm::Dijkstra, PathAlgorithm::BFS, PathAlgorithm::DFS }) {
				const std::string name = getPathAlgorithmName(algorithm);
				PathResult result = findPath(map, p, algorithm, start, end);
				if (cheapest < 0) {
					CHECK(result.path.empty(), "trial %d: %s found a path where none exists", trial, name.c_str());
					continue;
				}
				CHECK(!result.path.empty(), "trial %d: %s found no path but one exists", trial, name.c_str());
				if (result.path.empty()) continue;
				checkPathShape(map, p, result.path, start, end, name.c_str());
				CHECK(near(result.cost, getPathCost(map, p, result.path), 1e-9), "trial %d: %s reports the wrong cost", trial, name.c_str());
				if (algorithm == PathAlgorithm::AStar || algorithm == PathAlgorithm::Dijkstra)
					CHECK(near(result.cost, cheapest, 1e-6), "trial %d: %s cost %.4f, cheapest %.4f", trial, name.c_str(), result.cost, cheapest);
				if (algorithm == PathAlgorithm::BFS)
					CHECK((double)result.path.size() - 1 == fewest, "trial %d: BFS is not the fewest steps", trial);
				std::vector<bool> searched(map.getCellCount(), false);
				for (int s : result.searchedCells) searched[s] = true;
				for (int s : result.path) CHECK(searched[s], "trial %d: %s path cell not marked searched", trial, name.c_str());
			}
		}
		std::printf("random cost maps: %d with a path, %d without (each with A*, Dijkstra's, BFS, DFS)\n", withPath, withoutPath);
	}

	// --- costWeight: a costly strip is crossed when cost doesn't matter, and avoided when it does ---
	{
		GridMap map(30, 30, 0.1);
		map.addLayer("cost", 0.f);
		for (int row = 0; row < 25; row++) map.at("cost", 15, row) = 0.5f; //a costly wall-like strip with a gap at the bottom
		int start = map.getIndex(5, 5), end = map.getIndex(25, 5);
		CostMapParameters ignoreCost = p, avoidCost = p;
		ignoreCost.costWeight = 0.0;
		avoidCost.costWeight = 200.0; //crossing costs 200 * 0.5 = 100 extra; the detour around the gap is about 40 cells
		auto crossesStrip = [&](const PathResult& result) {
			for (int c : result.path) if (map.getColumn(c) == 15 && map.getRow(c) < 25) return true;
			return false;
		};
		CHECK(crossesStrip(pathfindingAstar(map, ignoreCost, start, end)), "costWeight 0: A* walks straight across the costly strip");
		CHECK(!crossesStrip(pathfindingAstar(map, avoidCost, start, end)), "costWeight 200: A* detours around the costly strip");
	}

	// --- ROS 2 exports (checked against the real message types when written; these keep them right without ROS) ---
	{
		CHECK(getOccupancyValue(GridMap::getUnknownValue(), p) == -1 && getOccupancyValue(p.lethalCost, p) == 100,
			"export: OccupancyGrid unknown = -1, lethal = 100");
		CHECK(getOccupancyValue(0.f, p) == 0 && getOccupancyValue(0.5f, p) == 99 && getOccupancyValue(0.25f, p) == 50,
			"export: walkable cost 0 .. 0.5 scales to 0 .. 99");
		GridMap map(10, 10, 0.1);
		std::vector<int> east = { map.getIndex(2, 5), map.getIndex(3, 5) }, north = { map.getIndex(2, 5), map.getIndex(2, 4) };
		CHECK(near(getPathYaw(map, east, 0), 0, 1e-9) && near(getPathYaw(map, north, 0), M_PI / 2, 1e-9) && near(getPathYaw(map, north, 1), M_PI / 2, 1e-9),
			"export: path heading is 0 toward +x (column + 1) and pi/2 toward +y (row - 1), also at the last pose");
		map.addLayer("cost", 0.f);
		map.at("cost", 0, 0) = p.lethalCost; //top-left cell: smallest x, largest y
		std::ostringstream grid;
		writeOccupancyGridMessage(grid, map, p);
		std::string text = grid.str();
		std::string lastRow = text.substr(text.rfind("\n  ") + 3);
		CHECK(lastRow.rfind("100, 0", 0) == 0, "export: OccupancyGrid data starts at the bottom row, so the top-left cell is first in the last row");
		std::ostringstream empty;
		writePathMessage(empty, map, {});
		CHECK(empty.str().find("poses: []") != std::string::npos, "export: an empty path has poses: []");
	}

	// --- path -> waypoints (core/Waypoints.h; the first four checks were ros2/terrain_planner's gtests) ---
	{
		//true if the straight line between two cell centres stays off lethal cells, sampled 64 times per cell
		//(independent of getCellsOnSegment, so it can check it)
		//(was sampled 64 times per cell, but a sample landing exactly on a grid corner is ambiguous; now exact, crossesCellInside)
		auto isLineClear = [&](const GridMap& map, double x0, double y0, double x1, double y1) {
			for (int cell = 0; cell < map.getCellCount(); cell++) {
				if (isLethal(map.getLayer("cost")[cell], p) && crossesCellInside(map, x0, y0, x1, y1, cell)) return false;
			}
			return true;
		};
		auto cellsAre = [&](const GridMap& map, const std::vector<int>& cells, const std::vector<std::pair<int, int>>& expected) {
			if (cells.size() != expected.size()) return false;
			for (std::size_t i = 0; i < cells.size(); i++) {
				if (map.getColumn(cells[i]) != expected[i].first || map.getRow(cells[i]) != expected[i].second) return false;
			}
			return true;
		};
		GridMap grid(10, 10, 0.1);
		grid.addLayer("cost", 0.f);
		CHECK(cellsAre(grid, getCellsOnSegment(grid, grid.getIndex(0, 0), grid.getIndex(4, 0)), { {0,0}, {1,0}, {2,0}, {3,0}, {4,0} }),
			"waypoints: a straight line visits each cell once");
		CHECK(cellsAre(grid, getCellsOnSegment(grid, grid.getIndex(0, 0), grid.getIndex(2, 2)), { {0,0}, {1,0}, {0,1}, {1,1}, {2,1}, {1,2}, {2,2} }),
			"waypoints: a diagonal through grid corners also visits both side cells of each corner");
		CHECK(cellsAre(grid, getCellsOnSegment(grid, grid.getIndex(0, 0), grid.getIndex(1, 3)), { {0,0}, {0,1}, {1,1}, {0,2}, {1,2}, {1,3} }),
			"waypoints: a steep line (1 across, 3 down) finds the one grid corner it passes through");
		bool coversSamples = true;
		for (int column = 0; column < 10; column++) {
			std::vector<int> cells = getCellsOnSegment(grid, grid.getIndex(0, 0), grid.getIndex(column, 9));
			double x0, y0, x1, y1;
			grid.getPosition(0, 0, x0, y0);
			grid.getPosition(column, 9, x1, y1);
			for (int i = 0; i <= 1000; i++) {
				int c, r;
				grid.getCell(x0 + (x1 - x0) * i / 1000, y0 + (y1 - y0) * i / 1000, c, r);
				coversSamples = coversSamples && std::find(cells.begin(), cells.end(), grid.getIndex(c, r)) != cells.end();
			}
		}
		CHECK(coversSamples, "waypoints: every cell a densely sampled line passes through is among getCellsOnSegment's cells");

		//a straight diagonal run over costly ground: the line equals the path, so it must prune to its ends
		GridMap costlyDiagonal(10, 10, 0.1);
		costlyDiagonal.addLayer("cost", 0.3f);
		std::vector<int> diagonal;
		for (int k = 0; k < 8; k++) diagonal.push_back(costlyDiagonal.getIndex(k, k));
		CHECK(prunePath(costlyDiagonal, p, diagonal).size() == 2,
			"waypoints: a straight diagonal run over costly ground prunes to its ends (corner-touched cells add no cost)");

		std::vector<int> straight;
		for (int column = 0; column < 10; column++) straight.push_back(grid.getIndex(column, 2));
		std::vector<int> prunedStraight = prunePath(grid, p, straight);
		CHECK(prunedStraight.size() == 2 && prunedStraight.front() == straight.front() && prunedStraight.back() == straight.back(),
			"waypoints: a straight path prunes to its two ends");

		CostMapParameters noInflation = p;
		noInflation.inflationRadius = 0;
		GridMap wall(10, 10, 0.1);
		wall.addLayer("cost", 0.f);
		for (int row = 0; row < 8; row++) wall.at("cost", 5, row) = p.lethalCost; //a wall with a gap at the bottom
		double acrossX0, acrossY0, acrossX1, acrossY1, touchX, touchY;
		wall.getPosition(0, 5, acrossX0, acrossY0);
		wall.getPosition(9, 5, acrossX1, acrossY1);
		wall.getPosition(4, 7, touchX, touchY);
		double cornerX = touchX + 0.05, cornerY = touchY - 0.05; //the bottom-right corner of cell (4, 7): the wall's corner
		CHECK(!isLineClear(wall, acrossX0, acrossY0, acrossX1, acrossY1) && isLineClear(wall, touchX, touchY, cornerX, cornerY),
			"test helper: a line through the wall is caught, one that only touches its corner is not");
		PathResult aroundWall = findPath(wall, noInflation, PathAlgorithm::AStar, wall.getIndex(0, 0), wall.getIndex(9, 0));
		std::vector<int> prunedWall = prunePath(wall, noInflation, aroundWall.path);
		bool segmentsClear = !prunedWall.empty();
		for (std::size_t i = 1; i < prunedWall.size(); i++) {
			double x0, y0, x1, y1;
			wall.getPosition(wall.getColumn(prunedWall[i - 1]), wall.getRow(prunedWall[i - 1]), x0, y0);
			wall.getPosition(wall.getColumn(prunedWall[i]), wall.getRow(prunedWall[i]), x1, y1);
			segmentsClear = segmentsClear && isLineClear(wall, x0, y0, x1, y1);
		}
		CHECK(prunedWall.size() < aroundWall.path.size() && segmentsClear && getPathCost(wall, noInflation, prunedWall) <= aroundWall.cost + 1e-6,
			"waypoints: pruning around a wall shortens the path, never crosses the wall, and costs no more");

		WaypointParameters waypointParameters;
		std::vector<Waypoint> wallWaypoints = makeWaypoints(wall, prunedWall, waypointParameters);
		bool cornersKept = true, waypointsClear = true;
		for (int corner : prunedWall) {
			double x, y;
			wall.getPosition(wall.getColumn(corner), wall.getRow(corner), x, y);
			cornersKept = cornersKept && std::any_of(wallWaypoints.begin(), wallWaypoints.end(),
				[&](const Waypoint& waypoint) { return std::hypot(waypoint.x - x, waypoint.y - y) < 1e-9; });
		}
		for (std::size_t i = 1; i < wallWaypoints.size(); i++) {
			waypointsClear = waypointsClear && isLineClear(wall, wallWaypoints[i - 1].x, wallWaypoints[i - 1].y, wallWaypoints[i].x, wallWaypoints[i].y);
			waypointsClear = waypointsClear && std::hypot(wallWaypoints[i].x - wallWaypoints[i - 1].x, wallWaypoints[i].y - wallWaypoints[i - 1].y) <= waypointParameters.spacing + 1e-9;
		}
		CHECK(cornersKept, "waypoints: every corner of the pruned path is a waypoint");
		CHECK(waypointsClear, "waypoints: no line between two waypoints touches the wall, and none is longer than the spacing");

		GridMap line(21, 3, 0.1);
		line.addLayer("cost", 0.f);
		WaypointParameters halfMetre;
		halfMetre.spacing = 0.5;
		halfMetre.nominalSpeed = 1.0;
		std::vector<int> twoMetres = { line.getIndex(0, 1), line.getIndex(20, 1) }; //2 m along +x
		std::vector<Waypoint> evenly = makeWaypoints(line, twoMetres, halfMetre, M_PI / 2);
		bool evenSteps = evenly.size() == 5; //0, 0.5, 1.0, 1.5, 2.0 m
		for (std::size_t i = 1; evenSteps && i < evenly.size(); i++) {
			evenSteps = near(evenly[i].x - evenly[i - 1].x, 0.5, 1e-9) && near(evenly[i].time - evenly[i - 1].time, 0.5, 1e-9);
		}
		CHECK(evenSteps && near(evenly.front().yaw, 0, 1e-9) && near(evenly.back().yaw, M_PI / 2, 1e-9),
			"waypoints: 2 m at 0.5 m spacing gives 5 waypoints 0.5 s apart at 1 m/s, heading +x, the last taking the goal's yaw");
		//a waypoint exactly on a lethal cell's corner (the middle of (0,0) -> (3,3), beside lethal (2,1)) must not slow its piece
		GridMap cornerMap(10, 10, 0.1);
		cornerMap.addLayer("cost", 0.f);
		cornerMap.at("cost", 2, 1) = p.lethalCost;
		std::vector<Waypoint> pastCorner = makeWaypoints(cornerMap, { cornerMap.getIndex(0, 0), cornerMap.getIndex(3, 3) }, waypointParameters);
		CHECK(pastCorner.size() == 3 && near(pastCorner.back().time, 3 * std::sqrt(2.0) * 0.1 / waypointParameters.nominalSpeed, 1e-9),
			"waypoints: a waypoint on a lethal cell's corner does not slow its piece (cost read just before it)");
		std::vector<Waypoint> listed(2);
		listed[0].x = -0.35; listed[0].y = 1.2; listed[1].x = 2; listed[1].y = -0.05;
		CHECK(getWaypointListText(listed) == "[-0.35,1.20] [2.00,-0.05]", "waypoints: the husky-stable_ws list is [x,y] [x,y] in metres");
		double cheapTime = makeWaypoints(line, twoMetres, waypointParameters).back().time;
		line.addLayer("cost", 0.5f);
		CHECK(makeWaypoints(line, twoMetres, waypointParameters).back().time > cheapTime, "waypoints: costly cells are slower");
	}

	// --- the example maps in maps/ (made by ./build/generate_maps) ---
	{
		GridMap map; int start, end;
		CHECK(loadGridMap("maps/flat.gridmap", map, start, end), "maps: flat.gridmap loads");
		bool allZero = true;
		for (float c : map.getLayer("cost")) allZero = allZero && c == 0.f;
		CHECK(allZero && start >= 0 && end >= 0 && !pathfindingAstar(map, p, start, end).path.empty(), "maps: flat costs 0 everywhere and has a path");

		for (const char* name : { "rugged", "dungeon", "hybrid" }) {
			CHECK(loadGridMap(std::string("maps/") + name + ".gridmap", map, start, end), "maps: %s.gridmap loads", name);
			CHECK(!pathfindingAstar(map, p, start, end).path.empty(), "maps: %s has a path from its start to its end", name);
			CHECK(map.getColumns() == 72 && map.getRows() == 72, "maps: %s is 72 x 72 (10 px cells, so the sprites draw cleanly)", name);
		}
		//hybrid: every route crosses rugged terrain (blocking all costly cells leaves no path)
		GridMap zeroOnly = map;
		for (float& c : zeroOnly.getLayer("cost")) if (c > 0.f) c = p.lethalCost;
		CHECK(pathfindingBFS(zeroOnly, p, start, end).path.empty(), "maps: hybrid has no route that avoids rugged terrain");
	}

	// --- the terrain brush's tapered Gaussian kernel (src/TerrainBrush.h) ---
	{
		CHECK(buildGaussianKernel(1).size() == 1 && buildGaussianKernel(1)[0] == 1.f, "brush: a 1-cell brush is a single weight of 1");
		for (int width : getBrushWidths()) {
			std::vector<float> kernel = buildGaussianKernel(width);
			int k = (width - 1) / 2;
			CHECK(kernel[k * width + k] == 1.f, "brush %d: the centre weight is 1", width);
			bool inRange = true, symmetric = true;
			for (int r = 0; r < width; r++) for (int c = 0; c < width; c++) {
				float w = kernel[r * width + c];
				inRange = inRange && w >= 0.f && w <= 1.f;
				symmetric = symmetric && w == kernel[c * width + r] && w == kernel[r * width + (width - 1 - c)];
			}
			CHECK(inRange && symmetric, "brush %d: weights in [0, 1] and symmetric", width);
			//the taper is 0 at width / 2 cells from the centre, so a 3 x 3 brush's corner cells (1.41 cells out) keep a
			//little weight and wider brushes' corners get none
			if (width == 3) CHECK(kernel[0] > 0.f && kernel[0] < 0.05f, "brush 3: the corners keep a small weight");
			if (width > 3) CHECK(kernel[0] == 0.f, "brush %d: the taper reaches 0 at the box's corners", width);
		}
	}

	// --- brush cost table: one click of each preset height on a flat map, with each brush width ---
	// Prints the worst cost of the cells around the patch (cN = worst walkable cost, L = lethal), for TerrainBrush.h.
	// Also checks that the elevation changes only inside the brush box.
	{
		std::printf("brush cost table (one click on a flat 0.1 m map; cN = worst walkable cost, L = lethal):\n  brush ");
		const float heights[] = { 0.02f, 0.05f, 0.08f, 0.10f, 0.15f, 0.50f };
		for (float h : heights) std::printf("  %+.2f m", h);
		std::printf("\n");
		for (int width : getBrushWidths()) {
			std::printf("  %-5d", width);
			for (float h : heights) {
				GridMap map(21, 21, 0.1);
				map.addLayer("elevation", 0.f);
				std::vector<float> kernel = buildGaussianKernel(width);
				int k = (width - 1) / 2, centre = 10;
				for (int r = 0; r < width; r++) for (int c = 0; c < width; c++) {
					map.at("elevation", centre - k + c, centre - k + r) += h * kernel[r * width + c];
				}
				bool outsideFlat = true;
				for (int r = 0; r < 21; r++) for (int c = 0; c < 21; c++) {
					bool inside = std::abs(c - centre) <= k && std::abs(r - centre) <= k;
					if (!inside) outsideFlat = outsideFlat && map.at("elevation", c, r) == 0.f;
				}
				CHECK(outsideFlat, "brush %d: one click changes no elevation outside the box", width);
				runCostMapPipeline(map, p);
				float worst = 0.f; bool lethal = false;
				for (float c : map.getLayer("cost")) {
					if (isLethal(c, p)) lethal = true;
					else worst = std::max(worst, c);
				}
				if (lethal) std::printf("  L      ");
				else std::printf("  c%.2f  ", worst);
			}
			std::printf("\n");
		}
		//flatten: height 0 at full weight brings the centre back to 0 m
		std::vector<float> kernel = buildGaussianKernel(5);
		float z = 0.3f;
		z = z * (1.f - kernel[2 * 5 + 2]);
		CHECK(z == 0.f, "brush: flattening sets the centre to 0 m");
	}

	std::printf(failures ? "%d FAILURE(S)\n" : "all core checks passed\n", failures);
	return failures ? 1 : 0;
}
